#include <iostream>
#include <fstream>
#include <vector>
#include <stdexcept>
#include <Windows.h>

using namespace std;

#pragma pack(push, 1)
struct BitmapFileHeader {
    WORD bfType;
    DWORD bfSize;
    WORD bfReserved1;
    WORD bfReserved2;
    DWORD bfOffBits;
};
#pragma pack(pop)

struct BitmapInfoHeader {
    DWORD biSize;
    LONG biWidth;
    LONG biHeight;
    WORD biPlanes;
    WORD biBitCount;
    DWORD biCompression;
    DWORD biSizeImage;
    LONG biXPelsPerMeter;
    LONG biYPelsPerMeter;
    DWORD biClrUsed;
    DWORD biClrImportant;
};

struct Pixel {
    BYTE blue;
    BYTE green;
    BYTE red;
};

bool ReadBMP(const string& filename, BitmapFileHeader& fileHeader, BitmapInfoHeader& infoHeader, vector<BYTE>& pixelData) {
    ifstream in(filename, ios::binary);
    if (!in) {
        cerr << "Не удалось открыть файл " << filename << endl;
        return false;
    }

    in.read(reinterpret_cast<char*>(&fileHeader), sizeof(fileHeader));
    if (fileHeader.bfType != 0x4D42) { // 'BM'
        cerr << "Файл не является BMP\n";
        return false;
    }

    in.read(reinterpret_cast<char*>(&infoHeader), sizeof(infoHeader));
    if (infoHeader.biBitCount != 24 || infoHeader.biCompression != BI_RGB) {
        cerr << "Поддерживаются только 24-битные BMP без сжатия\n";
        return false;
    }

    const int headersSize = sizeof(BitmapFileHeader) + sizeof(BitmapInfoHeader);
    const int pixelDataSize = fileHeader.bfSize - headersSize;
    pixelData.resize(pixelDataSize);
    in.seekg(headersSize, ios::beg);
    in.read(reinterpret_cast<char*>(pixelData.data()), pixelDataSize);

    return true;
}

bool WriteBMP(const string& filename, const BitmapFileHeader& fileHeader, const BitmapInfoHeader& infoHeader, const vector<BYTE>& pixelData) {
    ofstream out(filename, ios::binary);
    if (!out) {
        cerr << "Не удалось создать файл " << filename << endl;
        return false;
    }

    out.write(reinterpret_cast<const char*>(&fileHeader), sizeof(BitmapFileHeader));
    out.write(reinterpret_cast<const char*>(&infoHeader), sizeof(BitmapInfoHeader));

    const int rowSize = ((infoHeader.biWidth * 3 + 3) / 4) * 4;
    for (LONG y = 0; y < abs(infoHeader.biHeight); ++y) {
        const int start = y * rowSize;
        out.write(reinterpret_cast<const char*>(pixelData.data() + start), infoHeader.biWidth * 3);
        const int padding = rowSize - infoHeader.biWidth * 3;
        if (padding > 0) {
            char pad[4] = { 0 };
            out.write(pad, padding);
        }
    }

    return true;
}

void EmbedMessage(vector<BYTE>& pixelData, const BitmapInfoHeader& infoHeader, const vector<bool>& messageBits) {
    const int rowSize = ((infoHeader.biWidth * 3 + 3) / 4) * 4;
    const size_t capacity = infoHeader.biWidth * abs(infoHeader.biHeight) * 3;
    const size_t requiredSize = messageBits.size();
    if (requiredSize > capacity) {
        throw runtime_error("Сообщение слишком большое для контейнера");
    }

    size_t bitIndex = 0;
    for (LONG y = 0; y < abs(infoHeader.biHeight); ++y) {
        for (LONG x = 0; x < infoHeader.biWidth; ++x) {
            const int offset = y * rowSize + x * 3;
            // R
            if (bitIndex < requiredSize) {
                pixelData[offset + 2] = (pixelData[offset + 2] & 0xFE) | messageBits[bitIndex++];
            }
            // G
            if (bitIndex < requiredSize) {
                pixelData[offset + 1] = (pixelData[offset + 1] & 0xFE) | messageBits[bitIndex++];
            }
            // B
            if (bitIndex < requiredSize) {
                pixelData[offset] = (pixelData[offset] & 0xFE) | messageBits[bitIndex++];
            }
        }
    }
}

vector<bool> ExtractMessage(const vector<BYTE>& pixelData, const BitmapInfoHeader& infoHeader) {
    const int rowSize = ((infoHeader.biWidth * 3 + 3) / 4) * 4;
    vector<bool> bits;

    for (LONG y = 0; y < abs(infoHeader.biHeight); ++y) {
        for (LONG x = 0; x < infoHeader.biWidth; ++x) {
            const int offset = y * rowSize + x * 3;
            bits.push_back(pixelData[offset + 2] & 0x01); // R
            bits.push_back(pixelData[offset + 1] & 0x01); // G
            bits.push_back(pixelData[offset] & 0x01);     // B
        }
    }

    return bits;
}

vector<bool> GetMessageBitsFromInput(const string& input) {
    vector<bool> bits;
    for (char c : input) {
        if (c == '0') {
            bits.push_back(false);
        }
        else if (c == '1') {
            bits.push_back(true);
        }
    }
    return bits;
}

string ConvertBitsToString(const vector<bool>& bits, size_t length) {
    string result;
    for (size_t i = 0; i < length && i < bits.size(); ++i) {
        result += bits[i] ? '1' : '0';
    }
    return result;
}

vector<bool> AddLengthHeader(const vector<bool>& messageBits) {
    size_t msgLength = messageBits.size();
    vector<bool> result;

    for (int i = 31; i >= 0; --i) {
        result.push_back((msgLength >> i) & 1);
    }

    result.insert(result.end(), messageBits.begin(), messageBits.end());
    return result;
}

vector<bool> RemoveLengthHeader(const vector<bool>& bits, size_t& length) {
    if (bits.size() < 32) {
        throw runtime_error("Недостаточно данных для извлечения длины сообщения");
    }

    length = 0;
    for (int i = 0; i < 32; ++i) {
        length = (length << 1) | bits[i];
    }

    if (bits.size() < 32 + length) {
        throw runtime_error("Недостаточно данных для извлечения сообщения");
    }

    return vector<bool>(bits.begin() + 32, bits.begin() + 32 + length);
}

int main(int argc, char* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    if (argc < 2) {
        cout << "Использование:\n";
        cout << "  stego embed <input.bmp> <secret.txt> <output.bmp>\n";
        cout << "  stego extract <container.bmp> <output.txt>\n";
        return 1;
    }

    try {
        string operation = argv[1];

        if (operation == "embed") {
            if (argc != 5) {
                cerr << "Неверное количество аргументов для embed\n";
                return 1;
            }
            string bmpFile = argv[2];
            string secretFile = argv[3];
            string outFile = argv[4];

            ifstream secretIn(secretFile);
            if (!secretIn) {
                cerr << "Не удалось открыть файл секрета\n";
                return 1;
            }
            string secretString((istreambuf_iterator<char>(secretIn)), istreambuf_iterator<char>());
            secretIn.close();

            vector<bool> messageBits = GetMessageBitsFromInput(secretString);
            vector<bool> fullMessage = AddLengthHeader(messageBits);

            BitmapFileHeader fileHeader;
            BitmapInfoHeader infoHeader;
            vector<BYTE> pixelData;
            if (!ReadBMP(bmpFile, fileHeader, infoHeader, pixelData)) {
                return 1;
            }

            EmbedMessage(pixelData, infoHeader, fullMessage);

            if (!WriteBMP(outFile, fileHeader, infoHeader, pixelData)) {
                return 1;
            }

            cout << "Сообщение успешно внедрено\n";

        }
        else if (operation == "extract") {
            if (argc != 4) {
                cerr << "Неверное количество аргументов для extract\n";
                return 1;
            }
            string containerFile = argv[2];
            string outFile = argv[3];

            BitmapFileHeader fileHeader;
            BitmapInfoHeader infoHeader;
            vector<BYTE> pixelData;
            if (!ReadBMP(containerFile, fileHeader, infoHeader, pixelData)) {
                return 1;
            }

            vector<bool> allBits = ExtractMessage(pixelData, infoHeader);
            size_t messageLength;
            vector<bool> messageBits = RemoveLengthHeader(allBits, messageLength);

            string messageStr = ConvertBitsToString(messageBits, messageLength);
            ofstream out(outFile);
            out << messageStr;
            out.close();

            cout << "Сообщение извлечено и сохранено в " << outFile << "\n";

        }
        else {
            cerr << "Неизвестная операция: " << operation << "\n";
            return 1;
        }
    }
    catch (const exception& ex) {
        cerr << "Ошибка: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
