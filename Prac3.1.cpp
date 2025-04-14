#include <iostream>
#include <string>
#include <unordered_map>
#include <fstream>
#include <iomanip>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <openssl/evp.h>
#include <sstream>   // Для stringstream
#include <iomanip>   // Для setw и setfill

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "libssl.lib")
#pragma comment(lib, "libcrypto.lib")

using namespace std;

// Статическая соль (должна быть одинаковой на клиенте и сервере)
const string STATIC_SALT = "mysecretsalt";

unordered_map<string, string> user_db;

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

void process_client(SOCKET client_socket) {
    char buffer[1024] = { 0 };
    int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);

    if (bytes_received <= 0) {
        closesocket(client_socket);
        return;
    }

    string request(buffer, bytes_received);
    size_t delim1 = request.find(':');
    size_t delim2 = request.find(':', delim1 + 1);

    if (delim1 == string::npos || delim2 == string::npos) {
        send(client_socket, "INVALID_FORMAT", 14, 0);
        closesocket(client_socket);
        return;
    }

    string command = request.substr(0, delim1);
    string username = request.substr(delim1 + 1, delim2 - delim1 - 1);
    string received_hash = request.substr(delim2 + 1);

    string response;
    if (command == "REGISTER") {
        if (user_db.count(username)) {
            response = "USER_EXISTS";
        }
        else {
            user_db[username] = received_hash;
            ofstream db_file("users.db", ios::app);
            if (db_file) {
                db_file << username << ":" << received_hash << endl;
            }
            response = "REGISTER_SUCCESS";
        }
    }
    else if (command == "LOGIN") {
        if (!user_db.count(username)) {
            response = "USER_NOT_FOUND";
        }
        else {
            string stored_hash = user_db[username];
            response = (received_hash == stored_hash) ? "LOGIN_SUCCESS" : "INVALID_PASSWORD";
        }
    }
    else {
        response = "UNKNOWN_COMMAND";
    }

    send(client_socket, response.c_str(), response.size(), 0);
    closesocket(client_socket);
}

void load_users() {
    ifstream db_file("users.db");
    if (db_file) {
        string line;
        while (getline(db_file, line)) {
            size_t pos = line.find(":");
            if (pos != string::npos) {
                string username = line.substr(0, pos);
                string hash = line.substr(pos + 1);
                user_db[username] = hash;
            }
        }
    }
}

int main() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    SOCKET server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in server_addr = { AF_INET, htons(8080), INADDR_ANY };

    bind(server_socket, (sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_socket, SOMAXCONN);

    cout << "Server started on port 8080. Waiting for connections..." << endl;

    load_users();

    while (true) {
        SOCKET client_socket = accept(server_socket, NULL, NULL);
        if (client_socket != INVALID_SOCKET) {
            process_client(client_socket);
        }
    }

    closesocket(server_socket);
    WSACleanup();
    return 0;
}
