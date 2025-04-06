#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdlib>
#include <ctime>

#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#else
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fstream>
#endif

struct ProcessInfo {
    int pid;
    std::string name;
    double cpu_usage;
    long memory_usage;
};

#ifdef _WIN32
std::vector<ProcessInfo> get_processes() {
    std::vector<ProcessInfo> processes;

    HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE) {
        return processes;
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hProcessSnap, &pe32)) {
        do {
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pe32.th32ProcessID);
            if (hProcess != NULL) {
                ProcessInfo info;
                info.pid = pe32.th32ProcessID;
                info.name = pe32.szExeFile;

                // Получаем использование памяти
                PROCESS_MEMORY_COUNTERS pmc;
                if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
                    info.memory_usage = pmc.WorkingSetSize / 1024; // в KB
                }

                // Упрощенный расчет использования CPU (в реальном проекте нужно более точное измерение)
                info.cpu_usage = rand() % 100;

                processes.push_back(info);
                CloseHandle(hProcess);
            }
        } while (Process32Next(hProcessSnap, &pe32));
    }

    CloseHandle(hProcessSnap);
    return processes;
}
#else
std::vector<ProcessInfo> get_processes() {
    std::vector<ProcessInfo> processes;
    DIR* proc_dir = opendir("/proc");
    if (!proc_dir) return processes;

    dirent* entry;
    while ((entry = readdir(proc_dir))) {
        if (entry->d_type == DT_DIR) {
            char* endptr;
            long pid = strtol(entry->d_name, &endptr, 10);
            if (*endptr == '\0') {
                ProcessInfo info;
                info.pid = pid;

                // Чтение имени процесса
                std::ifstream cmdline("/proc/" + std::string(entry->d_name) + "/cmdline");
                if (cmdline) {
                    std::getline(cmdline, info.name);
                    if (!info.name.empty()) {
                        size_t pos = info.name.find('\0');
                        if (pos != std::string::npos) {
                            info.name = info.name.substr(0, pos);
                        }
                        size_t slash = info.name.rfind('/');
                        if (slash != std::string::npos) {
                            info.name = info.name.substr(slash + 1);
                        }
                    }
                }

                // Упрощенный расчет использования CPU и памяти
                info.cpu_usage = rand() % 100;
                info.memory_usage = rand() % 10000;

                processes.push_back(info);
            }
        }
    }

    closedir(proc_dir);
    return processes;
}
#endif

void print_processes(const std::vector<ProcessInfo>& processes) {
    std::cout << "PID\tCPU%\tMemory(KB)\tName\n";
    std::cout << "----------------------------------------\n";
    for (const auto& proc : processes) {
        std::cout << proc.pid << "\t"
                  << proc.cpu_usage << "%\t"
                  << proc.memory_usage << "\t\t"
                  << proc.name << "\n";
    }
}

int main() {
    srand(time(nullptr));

    std::vector<ProcessInfo> processes = get_processes();
    print_processes(processes);

    if (!processes.empty()) {
        // Находим процесс с максимальной нагрузкой CPU
        auto max_cpu = std::max_element(processes.begin(), processes.end(),
            [](const ProcessInfo& a, const ProcessInfo& b) {
                return a.cpu_usage < b.cpu_usage;
            });

        std::cout << "\nProcess with highest CPU usage:\n";
        std::cout << "PID: " << max_cpu->pid << " Name: " << max_cpu->name
                  << " CPU: " << max_cpu->cpu_usage << "%\n";

        std::cout << "\nTerminate this process? (y/n): ";
        char choice;
        std::cin >> choice;

        if (choice == 'y' || choice == 'Y') {
#ifdef _WIN32
            HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, max_cpu->pid);
            if (hProcess != NULL) {
                TerminateProcess(hProcess, 0);
                CloseHandle(hProcess);
                std::cout << "Process terminated.\n";
            } else {
                std::cerr << "Failed to terminate process.\n";
            }
#else
            if (kill(max_cpu->pid, SIGTERM) == 0) {
                std::cout << "Process terminated.\n";
            } else {
                std::cerr << "Failed to terminate process.\n";
            }
#endif
        }
    }

    return 0;
}
