#include <iostream>
#include <cstdlib>
#include <windows.h>

using namespace std;

int main() {
    // Выделяем память для конфиденциальных данных
    char* password = (char*)malloc(256);
    char* cardNumber = (char*)malloc(256);

    cout << "Enter password: ";
    cin >> password;
    cout << "Enter card number: ";
    cin >> cardNumber;

    // Освобождаем память без очистки
    free(password);
    free(cardNumber);

    // Ожидание для сохранения данных в памяти
    cout << "Data freed. Press Enter to exit...";
    cin.ignore();
    cin.get();

    return 0;
}
