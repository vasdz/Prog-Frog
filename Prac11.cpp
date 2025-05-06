#define _CRT_SECURE_NO_WARNINGS
#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <experimental/filesystem>
#include <cstring>
#include <algorithm>
#include <limits>

namespace fs = std::experimental::filesystem;
using namespace std;

#pragma pack(push, 1)
struct DiskHeader {
    char signature[4] = { 'V', 'D', 'I', 'S' };
    uint32_t diskSize = 0;
    uint16_t clusterSize = 4096;
    uint32_t fatOffset = sizeof(DiskHeader);
    uint32_t dataOffset = 0;
    uint32_t freeClusters = 0;
};

struct DirEntry {
    char filename[32] = {};
    uint32_t fileSize = 0;
    uint32_t firstCluster = 0;
    bool isUsed = false;
};
#pragma pack(pop)

class VirtualDisk {
private:
    fstream diskFile;
    DiskHeader header;
    vector<uint32_t> fat;
    vector<DirEntry> directory;

    vector<uint32_t> findFreeClusters(uint32_t count) {
        vector<uint32_t> clusters;
        for (uint32_t i = 1; i < fat.size() && clusters.size() < count; ++i) {
            if (fat[i] == 0) {
                clusters.push_back(i);
                if (clusters.size() == count) break;
            }
        }
        return clusters;
    }

    bool writeFAT() {
        diskFile.seekp(header.fatOffset);
        diskFile.write(reinterpret_cast<char*>(fat.data()), fat.size() * sizeof(uint32_t));
        return diskFile.good();
    }

    bool writeDirectory() {
        diskFile.seekp(header.dataOffset);
        diskFile.write(reinterpret_cast<char*>(directory.data()), directory.size() * sizeof(DirEntry));
        return diskFile.good();
    }

public:
    bool createDisk(const string& path, uint32_t sizeMB) {
        const uint32_t diskSize = sizeMB * 1024 * 1024;
        header.diskSize = diskSize;
        header.clusterSize = 4096;

        // Кластеры
        const uint32_t clustersCount = (diskSize + header.clusterSize - 1) / header.clusterSize;
        header.fatOffset = sizeof(DiskHeader);
        header.dataOffset = header.fatOffset + clustersCount * sizeof(uint32_t);

        // Выравнивание dataOffset
        header.dataOffset = ((header.dataOffset + header.clusterSize - 1) / header.clusterSize) * header.clusterSize;

        // Подсчет занятого места под директорию
        const size_t dirSizeBytes = 128 * sizeof(DirEntry);
        const uint32_t dirClusters = (dirSizeBytes + header.clusterSize - 1) / header.clusterSize;

        header.freeClusters = clustersCount - 1 - dirClusters;

        // Открытие
        diskFile.open(path, ios::binary | ios::out | ios::trunc);
        if (!diskFile) {
            cerr << "Failed to create file: " << path << endl;
            return false;
        }

        // Инициализация FAT
        fat.resize(clustersCount, 0);
        fat[0] = 0xFFFFFFFF; // метаданные
        for (uint32_t i = 1; i <= dirClusters; ++i) {
            fat[i] = 0xFFFFFFFF; // занятые под каталог
        }

        // Инициализация каталога
        directory.resize(128);

        // Запись заголовка
        diskFile.seekp(0);
        diskFile.write(reinterpret_cast<char*>(&header), sizeof(header));
        if (!diskFile) return false;

        // Запись FAT
        diskFile.seekp(header.fatOffset);
        diskFile.write(reinterpret_cast<char*>(fat.data()), fat.size() * sizeof(uint32_t));
        if (!diskFile) return false;

        // Запись каталога
        diskFile.seekp(header.dataOffset);
        diskFile.write(reinterpret_cast<char*>(directory.data()), directory.size() * sizeof(DirEntry));
        if (!diskFile) return false;

        // Заполняем оставшееся место
        diskFile.seekp(diskSize - 1);
        diskFile.put(0);
        if (!diskFile) {
            cerr << "Failed to allocate disk space" << endl;
            return false;
        }

        diskFile.flush();
        diskFile.close();

        // Проверка размера
        if (fs::file_size(path) != diskSize) {
            cerr << "Critical error: File size mismatch!" << endl;
            fs::remove(path);
            return false;
        }

        return true;
    }

    bool mountDisk(const string& path) {
        cout << "Attempting to mount disk at: " << path << endl;

        diskFile.open(path, ios::binary | ios::in | ios::out);
        if (!diskFile) {
            cerr << "Failed to open disk file!" << endl;
            return false;
        }
        cout << "Disk file opened successfully" << endl;

        // Чтение заголовка
        diskFile.read(reinterpret_cast<char*>(&header), sizeof(header));
        if (!diskFile) {
            cerr << "Header read failed!" << endl;
            return false;
        }
        cout << "Header read. Disk size: " << header.diskSize
            << ", Cluster size: " << header.clusterSize << endl;

        // Проверка сигнатуры
        if (memcmp(header.signature, "VDIS", 4) != 0) {
            cerr << "Invalid disk signature!" << endl;
            return false;
        }

        // Пересчитываем кластеры (точно как при создании)
        const uint32_t clustersCount = (header.diskSize + header.clusterSize - 1) / header.clusterSize;
        cout << "Calculated clusters: " << clustersCount << endl;

        // Загрузка FAT
        fat.resize(clustersCount);
        diskFile.seekg(header.fatOffset);
        diskFile.read(reinterpret_cast<char*>(fat.data()), fat.size() * sizeof(uint32_t));
        if (!diskFile) {
            cerr << "FAT read failed! FAT offset: " << header.fatOffset
                << ", FAT size: " << (fat.size() * sizeof(uint32_t))
                << ", File size: " << fs::file_size(path) << endl;
            return false;
        }
        cout << "FAT read successfully. Total clusters: " << fat.size() << endl;

        // Чтение каталога
        const uint32_t dirSize = 128 * sizeof(DirEntry);
        diskFile.seekg(header.dataOffset);
        directory.resize(128);
        diskFile.read(reinterpret_cast<char*>(directory.data()), dirSize);
        if (!diskFile) {
            cerr << "Directory read failed! Data offset: " << header.dataOffset << endl;
            return false;
        }

        return true;
    }

    bool saveFile(const string& filename, const vector<char>& data) {
        const uint32_t clustersNeeded = (static_cast<uint32_t>(data.size()) + header.clusterSize - 1) / header.clusterSize;
        if (clustersNeeded > header.freeClusters) {
            cerr << "Not enough space. Required: " << clustersNeeded
                << ", Available: " << header.freeClusters << endl;
            return false;
        }

        auto clusters = findFreeClusters(clustersNeeded);
        if (clusters.size() != clustersNeeded) {
            cerr << "Failed to allocate clusters" << endl;
            return false;
        }

        // Link clusters in FAT
        for (size_t i = 0; i < clusters.size(); ++i) {
            fat[clusters[i]] = (i == clusters.size() - 1) ? 0xFFFFFFFF : clusters[i + 1];
        }

        // Write data to clusters
        uint32_t bytesWritten = 0;
        for (const auto& cluster : clusters) {
            diskFile.seekp(header.dataOffset + cluster * header.clusterSize);
            const uint32_t chunkSize = min(
                static_cast<uint32_t>(header.clusterSize),
                static_cast<uint32_t>(data.size() - bytesWritten)
            );
            diskFile.write(data.data() + bytesWritten, chunkSize);
            bytesWritten += chunkSize;
            if (!diskFile) return false;
        }

        // Update directory
        auto it = find_if(directory.begin(), directory.end(),
            [](const DirEntry& e) { return !e.isUsed; });

        if (it == directory.end()) {
            cerr << "Directory is full" << endl;
            return false;
        }

        memset(it->filename, 0, sizeof(it->filename));
        filename.copy(it->filename, sizeof(it->filename) - 1);
        it->fileSize = static_cast<uint32_t>(data.size());
        it->firstCluster = clusters[0];
        it->isUsed = true;

        // Save changes
        if (!writeFAT() || !writeDirectory()) return false;

        header.freeClusters -= clustersNeeded;
        diskFile.seekp(0);
        diskFile.write(reinterpret_cast<char*>(&header), sizeof(header));

        return diskFile.good();
    }

    vector<char> loadFile(const string& filename) {
        for (const auto& entry : directory) {
            if (entry.isUsed && strncmp(entry.filename, filename.c_str(), sizeof(entry.filename)) == 0) {
                vector<char> data;
                data.reserve(entry.fileSize);

                uint32_t currentCluster = entry.firstCluster;
                uint32_t bytesRead = 0;

                while (currentCluster != 0xFFFFFFFF && bytesRead < entry.fileSize) {
                    diskFile.seekg(header.dataOffset + currentCluster * header.clusterSize);
                    vector<char> buffer(header.clusterSize);
                    diskFile.read(buffer.data(), header.clusterSize);

                    const uint32_t bytesToCopy = min(
                        static_cast<uint32_t>(entry.fileSize - bytesRead),
                        static_cast<uint32_t>(header.clusterSize)
                    );

                    data.insert(data.end(), buffer.begin(), buffer.begin() + bytesToCopy);
                    bytesRead += bytesToCopy;
                    currentCluster = fat[currentCluster];
                }

                return data;
            }
        }
        cerr << "File not found: " << filename << endl;
        return {};
    }
};

int main() {
    VirtualDisk vdisk;
    const string path = fs::absolute("virtual_disk.vd").string(); // Используем абсолютный путь

    cout << "Using disk path: " << path << endl;

    if (!fs::exists(path)) {
        cout << "Creating new 100MB virtual disk..." << endl;
        if (!vdisk.createDisk(path, 100)) {
            cerr << "Disk creation failed!" << endl;
            return 1;
        }
        cout << "Disk created successfully" << endl;
    }

    if (!vdisk.mountDisk(path)) {
        cerr << "Mount failed!" << endl;
        return 1;
    }

    const vector<char> testData = { 'H','e','l','l','o',' ','W','o','r','l','d','!' };
    if (vdisk.saveFile("test.txt", testData)) {
        cout << "File saved successfully!" << endl;

        const auto loaded = vdisk.loadFile("test.txt");
        if (!loaded.empty()) {
            cout << "Loaded data: ";
            cout.write(loaded.data(), loaded.size());
            cout << endl;
        }
    }

    cout << "Press Enter to exit...";
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    return 0;
}
