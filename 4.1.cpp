#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <vector>
#include <winsock2.h>
#include <openssl/pem.h>
#include <openssl/evp.h>
#include <openssl/applink.c>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "libssl.lib")
#pragma comment(lib, "libcrypto.lib")

EVP_PKEY* loadPrivateKey(const char* filename) {
    FILE* fp = nullptr;
    if (fopen_s(&fp, filename, "rb") != 0 || !fp) {
        std::cerr << "Failed to open private key file\n";
        return nullptr;
    }

    EVP_PKEY* pkey = PEM_read_PrivateKey(fp, nullptr, nullptr, nullptr);
    fclose(fp);
    return pkey;
}

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(5555);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
    listen(serverSocket, 1);

    std::cout << "Waiting for client...\n";
    SOCKET clientSocket = accept(serverSocket, nullptr, nullptr);

    unsigned char encryptedData[512];
    int bytesReceived = recv(clientSocket, (char*)encryptedData, sizeof(encryptedData), 0);
    if (bytesReceived <= 0) {
        std::cerr << "Failed to receive data\n";
        return 1;
    }

    EVP_PKEY* privateKey = loadPrivateKey("private.pem");
    if (!privateKey) {
        std::cerr << "Private key loading failed\n";
        return 1;
    }

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(privateKey, nullptr);
    if (!ctx) {
        std::cerr << "EVP_PKEY_CTX_new failed\n";
        return 1;
    }

    if (EVP_PKEY_decrypt_init(ctx) <= 0) {
        std::cerr << "EVP_PKEY_decrypt_init failed\n";
        return 1;
    }

    size_t decryptedLen = 0;
    if (EVP_PKEY_decrypt(ctx, nullptr, &decryptedLen, encryptedData, bytesReceived) <= 0) {
        std::cerr << "Failed to determine decrypted length\n";
        return 1;
    }

    std::vector<unsigned char> decrypted(decryptedLen);
    if (EVP_PKEY_decrypt(ctx, decrypted.data(), &decryptedLen, encryptedData, bytesReceived) <= 0) {
        std::cerr << "Decryption failed\n";
        return 1;
    }

    std::cout << "Decrypted message: " << std::string((char*)decrypted.data(), decryptedLen) << "\n";

    EVP_PKEY_free(privateKey);
    EVP_PKEY_CTX_free(ctx);
    closesocket(clientSocket);
    closesocket(serverSocket);
    WSACleanup();
    return 0;
}
