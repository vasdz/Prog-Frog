#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <Windows.h>
#include <tchar.h>
#include <locale.h>
#include <fcntl.h> // Для _setmode и _O_U16TEXT
#include <io.h>    // Для _setmode

using namespace std;

// Определения для работы с расширенными атрибутами
#define FSCTL_SET_EA CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 15, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define FSCTL_GET_EA CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 16, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct _FILE_EA_INFO {
    ULONG EaSize;
} FILE_EA_INFO, * PFILE_EA_INFO;

#define FileEaInformation 7

// Функция для проверки существования файла
bool FileExists(const wstring& filename) {
    DWORD attrs = GetFileAttributes(filename.c_str());
    return (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY));
}

// Функция для внедрения сообщения в альтернативный поток данных
bool EmbedMessageInADS(const wstring& filename, const wstring& streamName, const vector<BYTE>& message) {
    wstring fullStreamPath = filename + L":" + streamName;

    HANDLE hFile = CreateFile(
        fullStreamPath.c_str(),
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        wcerr << L"Не удалось создать альтернативный поток. Ошибка: " << GetLastError() << endl;
        return false;
    }

    DWORD bytesWritten;
    if (!WriteFile(hFile, message.data(), static_cast<DWORD>(message.size()), &bytesWritten, NULL)) {
        wcerr << L"Не удалось записать данные в поток. Ошибка: " << GetLastError() << endl;
        CloseHandle(hFile);
        return false;
    }

    CloseHandle(hFile);
    return true;
}

// Функция для извлечения сообщения из альтернативного потока данных
vector<BYTE> ExtractMessageFromADS(const wstring& filename, const wstring& streamName) {
    wstring fullStreamPath = filename + L":" + streamName;
    vector<BYTE> message;

    HANDLE hFile = CreateFile(
        fullStreamPath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        wcerr << L"Не удалось открыть альтернативный поток. Ошибка: " << GetLastError() << endl;
        return message;
    }

    // Получаем размер файла
    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize)) {
        wcerr << L"Не удалось получить размер файла. Ошибка: " << GetLastError() << endl;
        CloseHandle(hFile);
        return message;
    }

    // Читаем содержимое потока
    message.resize(fileSize.QuadPart);
    DWORD bytesRead;
    if (!ReadFile(hFile, message.data(), static_cast<DWORD>(message.size()), &bytesRead, NULL)) {
        wcerr << L"Не удалось прочитать данные из потока. Ошибка: " << GetLastError() << endl;
        message.clear();
    }

    CloseHandle(hFile);
    return message;
}

// Функция для скрытия данных в расширенных атрибутах файла
bool EmbedMessageInExtendedAttributes(const wstring& filename, const vector<BYTE>& message) {
    // NTFS позволяет хранить до 64KB данных в расширенных атрибутах
    if (message.size() > 65536) {
        wcerr << L"Сообщение слишком большое для хранения в расширенных атрибутах" << endl;
        return false;
    }

    HANDLE hFile = CreateFile(
        filename.c_str(),
        GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        wcerr << L"Не удалось открыть файл для записи атрибутов. Ошибка: " << GetLastError() << endl;
        return false;
    }

    // Записываем данные в расширенные атрибуты
    DWORD bytesReturned;
    if (!DeviceIoControl(
        hFile,
        FSCTL_SET_EA,
        (LPVOID)message.data(),
        static_cast<DWORD>(message.size()),
        NULL,
        0,
        &bytesReturned,
        NULL
    )) {
        wcerr << L"Не удалось установить расширенные атрибуты. Ошибка: " << GetLastError() << endl;
        CloseHandle(hFile);
        return false;
    }

    CloseHandle(hFile);
    return true;
}

// Функция для извлечения данных из расширенных атрибутов файла
vector<BYTE> ExtractMessageFromExtendedAttributes(const wstring& filename) {
    vector<BYTE> message;

    HANDLE hFile = CreateFile(
        filename.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        wcerr << L"Не удалось открыть файл для чтения атрибутов. Ошибка: " << GetLastError() << endl;
        return message;
    }

    // Получаем размер расширенных атрибутов
    FILE_EA_INFO eaInfo;
if (!GetFileInformationByHandleEx(
    hFile,
    (FILE_INFO_BY_HANDLE_CLASS)FileEaInformation,  // Явное приведение типа
    &eaInfo,
    sizeof(eaInfo)
)) {
    wcerr << L"Не удалось получить информацию о расширенных атрибутах. Ошибка: " << GetLastError() << endl;
    CloseHandle(hFile);
    return message;
}

    if (eaInfo.EaSize == 0) {
        wcerr << L"Файл не содержит расширенных атрибутов" << endl;
        CloseHandle(hFile);
        return message;
    }

    // Читаем расширенные атрибуты
    message.resize(eaInfo.EaSize);
    DWORD bytesReturned;
    if (!DeviceIoControl(
        hFile,
        FSCTL_GET_EA,
        NULL,
        0,
        message.data(),
        static_cast<DWORD>(message.size()),
        &bytesReturned,
        NULL
    )) {
        wcerr << L"Не удалось прочитать расширенные атрибуты. Ошибка: " << GetLastError() << endl;
        message.clear();
    }

    CloseHandle(hFile);
    return message;
}

// Функция для чтения файла в вектор байтов
vector<BYTE> ReadFileToVector(const wstring& filename) {
    vector<BYTE> content;
    ifstream file(filename, ios::binary | ios::ate);
    if (!file) {
        wcerr << L"Не удалось открыть файл: " << filename << endl;
        return content;
    }

    streamsize size = file.tellg();
    file.seekg(0, ios::beg);
    content.resize(static_cast<size_t>(size));

    if (!file.read(reinterpret_cast<char*>(content.data()), size)) {
        wcerr << L"Не удалось прочитать файл: " << filename << endl;
        content.clear();
    }

    return content;
}

// Функция для записи вектора байтов в файл
bool WriteVectorToFile(const wstring& filename, const vector<BYTE>& data) {
    ofstream file(filename, ios::binary);
    if (!file) {
        wcerr << L"Не удалось создать файл: " << filename << endl;
        return false;
    }

    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    return static_cast<bool>(file);
}

void PrintUsage() {
    wcout << L"Использование:" << endl;
    wcout << L"  stego embed <файл-контейнер> <файл-сообщение> <выходной-файл> [метод]" << endl;
    wcout << L"  stego extract <файл-контейнер> <выходной-файл> [метод]" << endl;
    wcout << L"Методы:" << endl;
    wcout << L"  ads - альтернативные потоки данных (по умолчанию)" << endl;
    wcout << L"  ea - расширенные атрибуты" << endl;
}

int wmain(int argc, wchar_t* argv[]) {
    _setmode(_fileno(stdout), _O_U16TEXT);
    _setmode(_fileno(stderr), _O_U16TEXT);

    if (argc < 2) {
        PrintUsage();
        return 1;
    }

    try {
        wstring operation = argv[1];

        if (operation == L"embed") {
            if (argc < 5) {
                wcerr << L"Неверное количество аргументов для embed" << endl;
                PrintUsage();
                return 1;
            }

            wstring containerFile = argv[2];
            wstring secretFile = argv[3];
            wstring outputFile = argv[4];
            wstring method = (argc > 5) ? argv[5] : L"ads";

            if (!FileExists(containerFile)) {
                wcerr << L"Файл-контейнер не существует: " << containerFile << endl;
                return 1;
            }

            if (!FileExists(secretFile)) {
                wcerr << L"Файл с секретным сообщением не существует: " << secretFile << endl;
                return 1;
            }

            vector<BYTE> secretData = ReadFileToVector(secretFile);
            if (secretData.empty()) {
                wcerr << L"Не удалось прочитать секретное сообщение" << endl;
                return 1;
            }

            bool success = false;
            if (method == L"ads") {
                // Используем имя выходного файла как имя потока
                wstring streamName = outputFile.substr(outputFile.find_last_of(L"\\/") + 1);
                success = EmbedMessageInADS(containerFile, streamName, secretData);
            }
            else if (method == L"ea") {
                success = EmbedMessageInExtendedAttributes(containerFile, secretData);
            }
            else {
                wcerr << L"Неизвестный метод: " << method << endl;
                return 1;
            }

            if (success) {
                wcout << L"Сообщение успешно внедрено в " << containerFile << L" методом " << method << endl;
            }
            else {
                wcerr << L"Не удалось внедрить сообщение" << endl;
                return 1;
            }
        }
        else if (operation == L"extract") {
            if (argc < 4) {
                wcerr << L"Неверное количество аргументов для extract" << endl;
                PrintUsage();
                return 1;
            }

            wstring containerFile = argv[2];
            wstring outputFile = argv[3];
            wstring method = (argc > 4) ? argv[4] : L"ads";

            if (!FileExists(containerFile)) {
                wcerr << L"Файл-контейнер не существует: " << containerFile << endl;
                return 1;
            }

            vector<BYTE> secretData;
            if (method == L"ads") {
                // Используем имя выходного файла как имя потока
                wstring streamName = outputFile.substr(outputFile.find_last_of(L"\\/") + 1);
                secretData = ExtractMessageFromADS(containerFile, streamName);
            }
            else if (method == L"ea") {
                secretData = ExtractMessageFromExtendedAttributes(containerFile);
            }
            else {
                wcerr << L"Неизвестный метод: " << method << endl;
                return 1;
            }

            if (secretData.empty()) {
                wcerr << L"Не удалось извлечь сообщение" << endl;
                return 1;
            }

            if (WriteVectorToFile(outputFile, secretData)) {
                wcout << L"Сообщение успешно извлечено в " << outputFile << endl;
            }
            else {
                wcerr << L"Не удалось сохранить извлеченное сообщение" << endl;
                return 1;
            }
        }
        else {
            wcerr << L"Неизвестная операция: " << operation << endl;
            PrintUsage();
            return 1;
        }
    }
    catch (const exception& ex) {
        wcerr << L"Ошибка: " << ex.what() << endl;
        return 1;
    }

    return 0;
}
