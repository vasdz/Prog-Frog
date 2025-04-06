#include <iostream>
#include <string>
#include <vector>
#include <openssl/evp.h>
#include <iomanip>
#include <sstream>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "libssl.lib")
#pragma comment(lib, "libcrypto.lib")

using namespace std;

string hash_password(const string& password, const string& salt) {
    string salted_password = salt + password;
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len;

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        throw runtime_error("Failed to create EVP_MD_CTX");
    }

    if (EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1 ||
        EVP_DigestUpdate(ctx, salted_password.c_str(), salted_password.size()) != 1 ||
        EVP_DigestFinal_ex(ctx, hash, &hash_len) != 1) {
        EVP_MD_CTX_free(ctx);
        throw runtime_error("Failed to compute hash");
    }

    EVP_MD_CTX_free(ctx);

    stringstream ss;
    for (unsigned int i = 0; i < hash_len; ++i) {
        ss << hex << setw(2) << setfill('0') << (int)hash[i];
    }

    return ss.str();
}

bool send_to_server(const string& username, const string& password_hash, const string& salt) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "WSAStartup failed" << endl;
        return false;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        cerr << "Socket creation error: " << WSAGetLastError() << endl;
        WSACleanup();
        return false;
    }

    sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

    if (connect(sock, (sockaddr*)&serv_addr, sizeof(serv_addr)) == SOCKET_ERROR) {
        cerr << "Connection failed: " << WSAGetLastError() << endl;
        closesocket(sock);
        WSACleanup();
        return false;
    }

    string message = username + ":" + salt + ":" + password_hash;
    send(sock, message.c_str(), message.size(), 0);

    char buffer[1024] = { 0 };
    recv(sock, buffer, sizeof(buffer), 0);
    cout << "Server response: " << buffer << endl;

    closesocket(sock);
    WSACleanup();
    return true;
}

int main() {
    while (true) {
        cout << "\n1. Register\n2. Login\n3. Exit\nChoose option: ";
        int choice;
        cin >> choice;

        string username, password;

        switch (choice) {
        case 1: {
            cout << "Enter username: ";
            cin >> username;
            cout << "Enter password: ";
            cin >> password;

            string salt = "static_salt_123";
            string hashed_password = hash_password(password, salt);

            send_to_server(username, hashed_password, salt);
            break;
        }
        case 2: {
            cout << "Enter username: ";
            cin >> username;
            cout << "Enter password: ";
            cin >> password;

            string salt = "static_salt_123";
            string hashed_password = hash_password(password, salt);

            send_to_server(username, hashed_password, salt);
            break;
        }
        case 3:
            return 0;
        default:
            cout << "Invalid choice!" << endl;
        }
    }
}
