#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <iomanip>
#include <sstream>
#include <openssl/evp.h>
#include <fstream>
#include <random>

using namespace std;

unordered_map<string, pair<string, string>> user_db;

string generate_salt(size_t length = 16) {
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";

    string salt;
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dis(0, sizeof(alphanum) - 2);

    for (size_t i = 0; i < length; ++i) {
        salt += alphanum[dis(gen)];
    }

    return salt;
}

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

void register_user() {
    string username, password;
    cout << "Enter username: ";
    cin >> username;

    if (user_db.count(username)) {
        cout << "User already exists!" << endl;
        return;
    }

    cout << "Enter password: ";
    cin >> password;

    string salt = generate_salt();
    string hashed_password = hash_password(password, salt);

    user_db[username] = make_pair(salt, hashed_password);
    cout << "User registered successfully!" << endl;

    ofstream db_file("users.db", ios::app);
    if (db_file) {
        db_file << username << ":" << salt << ":" << hashed_password << endl;
    }
    db_file.close();
}

bool authenticate_user() {
    string username, password;
    cout << "Enter username: ";
    cin >> username;

    if (!user_db.count(username)) {
        cout << "User not found!" << endl;
        return false;
    }

    cout << "Enter password: ";
    cin >> password;

    auto& user_data = user_db[username];
    string salt = user_data.first;
    string stored_hash = user_data.second;

    string input_hash = hash_password(password, salt);

    if (input_hash == stored_hash) {
        cout << "Authentication successful!" << endl;
        return true;
    }
    else {
        cout << "Invalid password!" << endl;
        return false;
    }
}

void load_users() {
    ifstream db_file("users.db");
    if (db_file) {
        string line;
        while (getline(db_file, line)) {
            size_t pos1 = line.find(":");
            size_t pos2 = line.find(":", pos1 + 1);

            if (pos1 != string::npos && pos2 != string::npos) {
                string username = line.substr(0, pos1);
                string salt = line.substr(pos1 + 1, pos2 - pos1 - 1);
                string hash = line.substr(pos2 + 1);

                user_db[username] = make_pair(salt, hash);
            }
        }
    }
    db_file.close();
}

int main() {
    load_users();

    while (true) {
        cout << "\n1. Register\n2. Login\n3. Exit\nChoose option: ";
        int choice;
        cin >> choice;

        switch (choice) {
        case 1:
            register_user();
            break;
        case 2:
            authenticate_user();
            break;
        case 3:
            return 0;
        default:
            cout << "Invalid choice!" << endl;
        }
    }
}
