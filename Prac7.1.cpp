#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>

using namespace std;

vector<string> ReadPatterns(const string& filename) {
    vector<string> patterns;
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Failed to open " << filename << endl;
        return patterns;
    }

    string line;
    while (getline(file, line)) {
        if (!line.empty()) {
            patterns.push_back(line);
        }
    }
    file.close();
    return patterns;
}

DWORD FindTestAppProcessId() {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        cerr << "Failed to create process snapshot\n";
        return 0;
    }

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(snapshot, &pe)) {
        do {
            if (std::wstring(pe.szExeFile) == L"7.2.exe") {
                CloseHandle(snapshot);
                return pe.th32ProcessID;
            }
        } while (Process32Next(snapshot, &pe));
    }

    CloseHandle(snapshot);
    return 0;
}

bool ContainsPattern(const char* buffer, size_t size, const string& pattern) {
    return search(buffer, buffer + size, pattern.begin(), pattern.end()) != buffer + size;
}

void ScanProcess(DWORD pid, const vector<string>& patterns) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (hProcess == NULL) {
        cerr << "[-] Cannot open process PID: " << pid << " (Error: " << GetLastError() << ")\n";
        return;
    }

    MEMORY_BASIC_INFORMATION mbi;
    uintptr_t address = 0;
    const size_t chunkSize = 4096; // Читаем память по 4 КБ для скорости
    vector<char> buffer(chunkSize);

    while (VirtualQueryEx(hProcess, (LPCVOID)address, &mbi, sizeof(mbi))) {
        if (mbi.State == MEM_COMMIT && (mbi.Protect == PAGE_READWRITE || mbi.Protect == PAGE_READONLY)) {
            for (uintptr_t offset = 0; offset < mbi.RegionSize; offset += chunkSize) {
                SIZE_T bytesRead;
                if (ReadProcessMemory(hProcess, (LPCVOID)(address + offset), buffer.data(), chunkSize, &bytesRead)) {
                    for (const auto& pattern : patterns) {
                        if (ContainsPattern(buffer.data(), bytesRead, pattern)) {
                            cout << "[+] Found pattern '" << pattern << "' in PID: " << pid
                                << " at address: 0x" << hex << (address + offset) << dec << endl;
                        }
                    }
                }
            }
        }
        address += mbi.RegionSize;
    }
    CloseHandle(hProcess);
}

int main() {
    vector<string> patterns = ReadPatterns("password_list.txt");
    if (patterns.empty()) {
        cerr << "No patterns loaded. Exiting.\n";
        return 1;
    }

    DWORD targetPid = FindTestAppProcessId();
    if (targetPid == 0) {
        cerr << "7.2.exe not found. Run it first!\n";
        return 1;
    }

    cout << "[*] Scanning PID: " << targetPid << " (7.2.exe)\n";
    ScanProcess(targetPid, patterns);
    cout << "[*] Scan completed.\n";

    return 0;
}
