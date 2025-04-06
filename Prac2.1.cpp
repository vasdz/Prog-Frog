#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <cmath>

void cpu_loader(int load_percentage) {
    const int duration_ms = 100; // Период контроля нагрузки
    const auto start_time = std::chrono::steady_clock::now();

    while (true) {
        const auto work_time = duration_ms * load_percentage / 100;
        const auto sleep_time = duration_ms - work_time;

        // Рабочий цикл
        auto end_work = std::chrono::steady_clock::now() + std::chrono::milliseconds(work_time);
        while (std::chrono::steady_clock::now() < end_work) {
            volatile double x = 0;
            for (int i = 0; i < 100000; ++i) {
                x += std::sin(i) * std::cos(i);
            }
        }

        // Пауза
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));

        // Периодический вывод информации
        if (std::chrono::steady_clock::now() - start_time > std::chrono::seconds(1)) {
            std::cout << "CPU load: " << load_percentage << "%" << std::endl;
        }
    }
}

int main() {
    int load_percentage;
    std::cout << "Enter CPU load percentage (1-100): ";
    std::cin >> load_percentage;

    if (load_percentage < 1 || load_percentage > 100) {
        std::cerr << "Invalid percentage value!" << std::endl;
        return 1;
    }

    // Запускаем по одному потоку на каждое ядро процессора
    unsigned int num_threads = std::thread::hardware_concurrency();
    std::vector<std::thread> threads;

    for (unsigned int i = 0; i < num_threads; ++i) {
        threads.emplace_back(cpu_loader, load_percentage);
    }

    for (auto& thread : threads) {
        thread.join();
    }

    return 0;
}
