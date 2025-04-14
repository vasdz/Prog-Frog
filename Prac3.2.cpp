#include <iomanip>
#include <iostream>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <openssl/evp.h>
#include <sstream>   // Для stringstream
#include <iomanip>   // Для setw и setfill

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "libssl.lib")
#pragma comment(lib, "libcrypto.lib")

using namespace std;

const string STATIC_SALT = "mysecretsalt"; // Должна совпадать с серверной

string hash_password(const string& password, const string& salt) {
    string salted = salt + password;
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int len;

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha256(), NULL);
    EVP_DigestUpdate(ctx, salted.c_str(), salted.size());
    EVP_DigestFinal_ex(ctx, hash, &len);
    EVP_MD_CTX_free(ctx);

    stringstream ss;
    for (unsigned int i = 0; i < len; ++i) {
        ss << hex << setw(2) << setfill('0') << (int)hash[i];
    }
    return ss.str();
}

string send_command(const string& cmd, const string& user, const string& pass) {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr = { AF_INET, htons(8080) };
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        WSACleanup();
        return "CONNECTION_ERROR";
    }

    string hashed_pass = hash_password(pass, STATIC_SALT);
    string msg = cmd + ":" + user + ":" + hashed_pass;
    send(sock, msg.c_str(), msg.size(), 0);

    char buf[1024];
    int len = recv(sock, buf, sizeof(buf), 0);
    closesocket(sock);
    WSACleanup();

    return len > 0 ? string(buf, len) : "NO_RESPONSE";
}

int main() {
    while (true) {
        cout << "1. Register\n2. Login\n3. Exit\nChoose: ";
        int choice;
        cin >> choice;
        cin.ignore();

        if (choice == 3) break;

        string user, pass;
        cout << "Username: ";
        getline(cin, user);
        cout << "Password: ";
        getline(cin, pass);

        string cmd = (choice == 1) ? "REGISTER" : "LOGIN";
        string resp = send_command(cmd, user, pass);
        cout << "Server response: " << resp << endl;
    }
    return 0;
}
