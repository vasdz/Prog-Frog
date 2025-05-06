#include <iostream>
#include <fstream>
#include <vector>
#include <winsock2.h>

#pragma comment(lib, "Ws2_32.lib")

#define PORT 9000

using namespace std;

vector<char> readFileFromDisk(const string& filename) {
    ifstream file(filename, ios::binary);
    return { istreambuf_iterator<char>(file), istreambuf_iterator<char>() };
}

void handleClient(SOCKET clientSocket) {
    char filename[256] = {};
    recv(clientSocket, filename, sizeof(filename), 0);
    cout << "Client requested: " << filename << endl;

    vector<char> data = readFileFromDisk(filename);
    uint32_t size = data.size();
    send(clientSocket, reinterpret_cast<const char*>(&size), sizeof(size), 0);
    send(clientSocket, data.data(), size, 0);

    closesocket(clientSocket);
}

int main() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
    listen(serverSocket, SOMAXCONN);

    cout << "Server listening on port " << PORT << "...\n";

    while (true) {
        SOCKET clientSocket = accept(serverSocket, nullptr, nullptr);
        handleClient(clientSocket);
    }

    closesocket(serverSocket);
    WSACleanup();
    return 0;
}

