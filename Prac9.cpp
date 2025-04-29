#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <stdexcept>
#include <windows.h>
#include <shellapi.h> // Для CommandLineToArgvW

using namespace std;

#pragma pack(push, 1)
struct WavHeader {
    char riff[4] = { 'R', 'I', 'F', 'F' };
    uint32_t fileSize = 0;
    char wave[4] = { 'W', 'A', 'V', 'E' };

    char fmt[4] = { 'f', 'm', 't', ' ' };
    uint32_t fmtSize = 16;
    uint16_t audioFormat = 1; // PCM
    uint16_t numChannels = 1; // Mono
    uint32_t sampleRate = 44100;
    uint32_t byteRate = 88200;
    uint16_t blockAlign = 2;
    uint16_t bitsPerSample = 16;

    char data[4] = { 'd', 'a', 't', 'a' };
    uint32_t dataSize = 0;
};
#pragma pack(pop)

// Convert string to bit stream
vector<bool> StringToBits(const string& input) {
    vector<bool> bits;
    for (char c : input) {
        if (c == '0') bits.push_back(false);
        else if (c == '1') bits.push_back(true);
    }
    return bits;
}

// Convert bit stream to string
string BitsToString(const vector<bool>& bits) {
    string result;
    for (bool bit : bits) {
        result += bit ? '1' : '0';
    }
    return result;
}

// Add length header to message
vector<bool> AddLengthHeader(const vector<bool>& message) {
    vector<bool> result;
    size_t length = message.size();
    for (int i = 31; i >= 0; --i) {
        result.push_back((length >> i) & 1);
    }
    result.insert(result.end(), message.begin(), message.end());
    return result;
}

// Extract length header and return message
vector<bool> RemoveLengthHeader(const vector<bool>& bits, size_t& messageLength) {
    if (bits.size() < 32) {
        throw runtime_error("Not enough data to extract message length");
    }

    messageLength = 0;
    for (int i = 0; i < 32; ++i) {
        messageLength = (messageLength << 1) | bits[i];
    }

    if (bits.size() < 32 + messageLength) {
        throw runtime_error("Not enough data to extract message");
    }

    return vector<bool>(bits.begin() + 32, bits.begin() + 32 + messageLength);
}

// Read WAV file
void ReadWavFile(const wstring& filename, WavHeader& header, vector<int16_t>& samples) {
    ifstream file(filename, ios::binary);
    if (!file) {
        throw runtime_error("Failed to open file: " + string(filename.begin(), filename.end()));
    }

    file.read(reinterpret_cast<char*>(&header), sizeof(header));

    // Validate WAV format
    if (memcmp(header.riff, "RIFF", 4) != 0 ||
        memcmp(header.wave, "WAVE", 4) != 0 ||
        memcmp(header.fmt, "fmt ", 4) != 0 ||
        header.audioFormat != 1 || header.numChannels != 1 || header.bitsPerSample != 16) {
        throw runtime_error("Only 16-bit mono PCM WAV files are supported");
    }

    samples.resize(header.dataSize / sizeof(int16_t));
    file.read(reinterpret_cast<char*>(samples.data()), header.dataSize);
}

// Write WAV file
void WriteWavFile(const wstring& filename, const WavHeader& header, const vector<int16_t>& samples) {
    WavHeader modifiedHeader = header;
    modifiedHeader.fileSize = sizeof(modifiedHeader) - 8 + samples.size() * sizeof(int16_t);
    modifiedHeader.dataSize = samples.size() * sizeof(int16_t);

    ofstream file(filename, ios::binary);
    if (!file) {
        throw runtime_error("Failed to create file: " + string(filename.begin(), filename.end()));
    }

    file.write(reinterpret_cast<const char*>(&modifiedHeader), sizeof(modifiedHeader));
    file.write(reinterpret_cast<const char*>(samples.data()), samples.size() * sizeof(int16_t));
}

// Generate silent WAV file
void GenerateSilentWavFile(const wstring& filename, int durationSeconds) {
    const int sampleRate = 44100;
    const int numSamples = sampleRate * durationSeconds;

    WavHeader header{};
    memcpy(header.riff, "RIFF", 4);
    memcpy(header.wave, "WAVE", 4);
    memcpy(header.fmt, "fmt ", 4);
    memcpy(header.data, "data", 4);

    header.audioFormat = 1;           // PCM
    header.numChannels = 1;           // Моно
    header.sampleRate = sampleRate;   // 44.1 kHz
    header.byteRate = sampleRate * 2; // byteRate = sampleRate * channels * bitsPerSample/8
    header.blockAlign = 2;            // blockAlign = channels * bitsPerSample/8
    header.bitsPerSample = 16;        // 16 бит на семпл
    header.fmtSize = 16;
    header.dataSize = numSamples * sizeof(int16_t);
    header.fileSize = sizeof(WavHeader) - 8 + header.dataSize;

    vector<int16_t> samples(numSamples, 0); // Тишина

    ofstream file(filename, ios::binary);
    if (!file) {
        throw runtime_error("Failed to create WAV file: " + string(filename.begin(), filename.end()));
    }

    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    file.write(reinterpret_cast<const char*>(samples.data()), samples.size() * sizeof(int16_t));
}

// Embed message into audio samples
void EmbedMessage(vector<int16_t>& samples, const vector<bool>& message) {
    if (message.size() > samples.size()) {
        throw runtime_error("Message is too large for the container");
    }

    for (size_t i = 0; i < message.size(); ++i) {
        samples[i] = (samples[i] & ~1) | message[i]; // Modify LSB
    }
}

// Extract message from audio samples
vector<bool> ExtractMessage(const vector<int16_t>& samples) {
    vector<bool> bits;
    for (auto sample : samples) {
        bits.push_back(sample & 1); // Read LSB
    }
    return bits;
}

int wmain(int argc, wchar_t* argv[]) {
    SetConsoleOutputCP(CP_UTF8);

    if (argc < 2) {
        wcout << L"Usage:\n";
        wcout << L"  stego embed <input.wav> <secret.txt> <output.wav>\n";
        wcout << L"  stego extract <container.wav> <output.txt>\n";
        wcout << L"  stego generate <output.wav> <seconds>\n";
        return 1;
    }

    try {
        wstring operation = argv[1];

        if (operation == L"embed") {
            if (argc != 5) {
                wcerr << L"Invalid number of arguments for 'embed'\n";
                return 1;
            }

            wstring wavFile = argv[2];
            wstring secretFile = argv[3];
            wstring outFile = argv[4];

            // Read secret text
            wifstream secretIn(secretFile);
            if (!secretIn) {
                wcerr << L"Failed to open secret file\n";
                return 1;
            }
            wstring secretStr;
            getline(secretIn, secretStr, {}); // Read entire content
            secretIn.close();

            // Convert to bit stream
            string secretUtf8(secretStr.begin(), secretStr.end());
            vector<bool> messageBits = StringToBits(secretUtf8);
            vector<bool> fullMessage = AddLengthHeader(messageBits);

            // Read WAV file
            WavHeader header;
            vector<int16_t> samples;
            ReadWavFile(wavFile, header, samples);

            // Debug output
            wcout << L"[DEBUG] Container capacity: " << samples.size() << L" bits\n";
            wcout << L"[DEBUG] Secret message size: " << fullMessage.size() << L" bits\n";

            // Embed message
            EmbedMessage(samples, fullMessage);

            // Save output
            WriteWavFile(outFile, header, samples);

            wcout << L"Message successfully embedded\n";

        }
        else if (operation == L"extract") {
            if (argc != 4) {
                wcerr << L"Invalid number of arguments for 'extract'\n";
                return 1;
            }

            wstring containerFile = argv[2];
            wstring outFile = argv[3];

            // Read container file
            WavHeader header;
            vector<int16_t> samples;
            ReadWavFile(containerFile, header, samples);

            // Extract message
            vector<bool> allBits = ExtractMessage(samples);
            size_t messageLength;
            vector<bool> messageBits = RemoveLengthHeader(allBits, messageLength);

            // Save extracted message
            string messageStr = BitsToString(messageBits);
            ofstream out(outFile);
            out << messageStr;
            out.close();

            wcout << L"Message extracted and saved to " << outFile << L"\n";

        }
        else if (operation == L"generate") {
            if (argc != 4) {
                wcerr << L"Usage: stego generate <output.wav> <seconds>\n";
                return 1;
            }

            wstring outFile = argv[2];
            int seconds = _wtoi(argv[3]);

            if (seconds <= 0) {
                wcerr << L"Error: Duration must be greater than 0\n";
                return 1;
            }

            GenerateSilentWavFile(outFile, seconds);
            wcout << L"Silent WAV file created successfully (" << seconds << L" sec)\n";

        }
        else {
            wcerr << L"Unknown operation: " << operation << L"\n";
            return 1;
        }

    }
    catch (const exception& ex) {
        cerr << "Error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
