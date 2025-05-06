#include <iostream>
#include <fstream>
#include <vector>
#include <winsock2.h>
#include <ws2tcpip.h>


#pragma comment(lib, "Ws2_32.lib")

#define PORT 9000

using namespace std;

void saveToFile(const string& path, const vector<char>& data) {
    ofstream out(path, ios::binary);
    out.write(data.data(), data.size());
}

int main() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);


    if (connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cerr << "Connection failed!\n";
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    string filename = "virtual_disk.vd";
    send(sock, filename.c_str(), filename.size() + 1, 0);

    uint32_t size = 0;
    recv(sock, reinterpret_cast<char*>(&size), sizeof(size), 0);
    vector<char> buffer(size);
    recv(sock, buffer.data(), size, 0);

    saveToFile("received_disk.vd", buffer);
    cout << "File received and saved.\n";

    closesocket(sock);
    WSACleanup();
    return 0;
}
