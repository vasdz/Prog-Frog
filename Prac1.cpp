#include <iostream>
#include <vector>
#include <random>
#include <algorithm>
#include <string>
#include <chrono>

class PasswordGenerator {
public:
    PasswordGenerator() {
        // Инициализация нескольких зерен для генератора случайных чисел
        std::random_device rd; // Источник энтропии
        auto time_seed = static_cast<unsigned>(std::chrono::system_clock::now().time_since_epoch().count());
        auto this_seed = static_cast<unsigned>(reinterpret_cast<uintptr_t>(this));

        std::seed_seq seed_seq{rd(), time_seed, this_seed};

        // Создаем несколько генераторов с разными зернами
        generators_.reserve(num_generators_);
        for (size_t i = 0; i < num_generators_; ++i) {
            // Каждый генератор получает уникальное зерно
            std::seed_seq individual_seed{rd(), time_seed, this_seed, static_cast<unsigned>(i * 12345)};
            generators_.emplace_back(individual_seed);
        }
    }

    std::string generate_password(size_t max_length = 16) {
        // Определяем группы символов для пароля
        const std::string lowercase = "abcdefghijklmnopqrstuvwxyz"; // Строчные буквы
        const std::string uppercase = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"; // Заглавные буквы
        const std::string digits = "0123456789"; // Цифры
        const std::string specials = "!@#$%^&*()_+-=[]{}|;:,.<>?"; // Спецсимволы

        // Объединяем все символы в одну строку
        const std::string all_chars = lowercase + uppercase + digits + specials;

        // Минимальная длина пароля - 8 символов
        const size_t min_length = 8;

        // Выбираем случайную длину пароля
        std::uniform_int_distribution<size_t> length_dist(min_length, max_length);
        const size_t length = length_dist(get_combined_generator());

        std::string password;
        password.reserve(length);

        // Гарантируем наличие хотя бы одного символа из каждой группы
        std::uniform_int_distribution<size_t> lower_dist(0, lowercase.size() - 1);
        password += lowercase[lower_dist(get_combined_generator())];

        std::uniform_int_distribution<size_t> upper_dist(0, uppercase.size() - 1);
        password += uppercase[upper_dist(get_combined_generator())];

        std::uniform_int_distribution<size_t> digit_dist(0, digits.size() - 1);
        password += digits[digit_dist(get_combined_generator())];

        std::uniform_int_distribution<size_t> special_dist(0, specials.size() - 1);
        password += specials[special_dist(get_combined_generator())];

        // Заполняем оставшиеся позиции случайными символами
        std::uniform_int_distribution<size_t> char_dist(0, all_chars.size() - 1);
        while (password.size() < length) {
            password += all_chars[char_dist(get_combined_generator())];
        }

        // Перемешиваем символы в пароле для большей случайности
        std::shuffle(password.begin(), password.end(), get_combined_generator());

        return password;
    }

    void analyze_password_strength(const std::string& password) {
        // Флаги наличия разных типов символов
        bool has_lower = false, has_upper = false, has_digit = false, has_special = false;

        // Анализируем каждый символ пароля
        for (char c : password) {
            if (islower(c)) has_lower = true;
            else if (isupper(c)) has_upper = true;
            else if (isdigit(c)) has_digit = true;
            else has_special = true;
        }

        // Система оценки надежности пароля
        int score = 0;

        // Оценка за длину пароля
        if (password.length() >= 16) score += 4;
        else if (password.length() >= 12) score += 3;
        else if (password.length() >= 8) score += 2;
        else score += 1;

        // Оценка за разнообразие символов
        int char_types = has_lower + has_upper + has_digit + has_special;
        score += char_types * 2;

        // Вывод результатов анализа
        std::cout << "\nPassword strength analysis:\n";
        std::cout << "Length: " << password.length() << " characters\n";
        std::cout << "Contains lowercase: " << (has_lower ? "yes" : "no") << "\n";
        std::cout << "Contains uppercase: " << (has_upper ? "yes" : "no") << "\n";
        std::cout << "Contains digits: " << (has_digit ? "yes" : "no") << "\n";
        std::cout << "Contains special chars: " << (has_special ? "yes" : "no") << "\n";

        // Итоговая оценка надежности
        std::cout << "Strength rating: ";
        if (score >= 10) std::cout << "Very strong\n";
        else if (score >= 7) std::cout << "Strong\n";
        else if (score >= 5) std::cout << "Medium\n";
        else std::cout << "Weak\n";
    }

private:
    static constexpr size_t num_generators_ = 3; // Количество генераторов случайных чисел
    std::vector<std::mt19937> generators_; // Коллекция генераторов

    // Метод для получения случайного генератора
    std::mt19937& get_combined_generator() {
        std::uniform_int_distribution<size_t> dist(0, num_generators_ - 1);
        size_t idx = dist(generators_[0]);
        return generators_[idx];
    }
};

int main() {
    PasswordGenerator generator;

    std::cout << "Random Password Generator\n";
    std::cout << "------------------------\n";

    char choice;
    do {
        std::cout << "\nGenerate new password? (y/n): ";
        std::cin >> choice;

        if (tolower(choice) == 'y') {
            // Генерация и вывод пароля
            std::string password = generator.generate_password();
            std::cout << "\nYour password: " << password << "\n";

            // Анализ надежности пароля
            generator.analyze_password_strength(password);
        }
    } while (tolower(choice) == 'y');

    std::cout << "Exiting...\n";
    return 0;
}

//  std::uniform_int_distribution - класс который формирует равномерное (каждое значение одинаково вероятно) распределение целых чисел

//Конкретно в этом коде chrono выполняет следующие функции:

//Добавление энтропии - текущее время используется как дополнительный источник случайности при инициализации генератора случайных чисел.

//Уникальность зерен - даже если программа запускается несколько раз подряд, временная метка будет разной, что обеспечит разные последовательности паролей.

//Поддержка многозернового подхода - это часть системы, где используется несколько источников случайности для повышения качества генерации.
