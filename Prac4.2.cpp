#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <openssl/pem.h>
#include <openssl/evp.h>
#include <vector>
#include <openssl/applink.c>


#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "libssl.lib")
#pragma comment(lib, "libcrypto.lib")

EVP_PKEY* loadPublicKey(const char* filename) {
    FILE* fp = nullptr;
    if (fopen_s(&fp, filename, "rb") != 0 || !fp) {
        std::cerr << "Failed to open public key file\n";
        return nullptr;
    }

    EVP_PKEY* pkey = PEM_read_PUBKEY(fp, nullptr, nullptr, nullptr);
    fclose(fp);
    return pkey;
}

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(5555);

    // IPv4-совместимое преобразование
    if (InetPton(AF_INET, L"127.0.0.1", &serverAddr.sin_addr) != 1) {
        std::cerr << "InetPton failed\n";
        return 1;
    }

    if (connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) != 0) {
        std::cerr << "Connection failed\n";
        return 1;
    }

    EVP_PKEY* publicKey = loadPublicKey("public.pem");
    if (!publicKey) {
        std::cerr << "Public key loading failed\n";
        return 1;
    }

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(publicKey, nullptr);
    if (!ctx) {
        std::cerr << "EVP_PKEY_CTX_new failed\n";
        return 1;
    }

    if (EVP_PKEY_encrypt_init(ctx) <= 0) {
        std::cerr << "EVP_PKEY_encrypt_init failed\n";
        return 1;
    }

    const char* message = "Hello, secure world!";
    size_t encryptedLen = 0;

    if (EVP_PKEY_encrypt(ctx, nullptr, &encryptedLen, (const unsigned char*)message, strlen(message)) <= 0) {
        std::cerr << "Failed to determine encrypted length\n";
        return 1;
    }

    std::vector<unsigned char> encrypted(encryptedLen);
    if (EVP_PKEY_encrypt(ctx, encrypted.data(), &encryptedLen, (const unsigned char*)message, strlen(message)) <= 0) {
        std::cerr << "Encryption failed\n";
        return 1;
    }

    send(sock, (const char*)encrypted.data(), static_cast<int>(encryptedLen), 0);

    EVP_PKEY_free(publicKey);
    EVP_PKEY_CTX_free(ctx);
    closesocket(sock);
    WSACleanup();
    return 0;
}
