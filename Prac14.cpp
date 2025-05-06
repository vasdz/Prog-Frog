#include <iostream>
#include <vector>
#include <thread>
#include <future>
#include <algorithm>
#include <fstream>
#include <chrono>
#include <numeric>
#include <cmath>
#include <set>
#include <algorithm>
#include <iomanip>

using namespace std;

// Sequential implementation of Euclidean algorithm for GCD
int gcdSequential(int a, int b) {
    while (b != 0) {
        int temp = b;
        b = a % b;
        a = temp;
    }
    return abs(a);
}

// Multithreaded implementation of GCD using async tasks
int gcdParallel(int a, int b) {
    if (a == 0 || b == 0)
        return max(abs(a), abs(b));

    // Create an asynchronous task to compute GCD recursively
    future<int> result = async(launch::async, [a, b]() {
        return gcdParallel(b % a, a);
        });

    return result.get();
}

// Sequential prime number search in a given range
vector<int> findPrimesSequential(int start, int end) {
    vector<int> primes;
    if (start <= 2) {
        primes.push_back(2);
        start = 3;
    }

    // Ensure start is odd
    if (start % 2 == 0) start++;

    for (int i = start; i <= end; i += 2) {
        bool isPrime = true;
        int sqrtI = static_cast<int>(sqrt(i));

        for (int j = 3; j <= sqrtI; j += 2) {
            if (i % j == 0) {
                isPrime = false;
                break;
            }
        }

        if (isPrime) {
            primes.push_back(i);
        }
    }

    return primes;
}

// Parallel prime number search across multiple threads
vector<int> findPrimesParallel(int start, int end, int numThreads = thread::hardware_concurrency()) {
    // Limit the number of threads to available hardware concurrency
    numThreads = min(static_cast<int>(numThreads), static_cast<int>(thread::hardware_concurrency()));

    int rangeSize = end - start + 1;
    int chunkSize = max(100, rangeSize / numThreads);

    vector<future<vector<int>>> futures;
    vector<int> results;

    // Process each chunk in parallel
    for (int i = 0; i < numThreads && (start + i * chunkSize) <= end; ++i) {
        int chunkStart = max(start, start + i * chunkSize);
        int chunkEnd = min(end, chunkStart + chunkSize - 1);

        futures.push_back(async(launch::async, [chunkStart, chunkEnd]() {
            return findPrimesSequential(chunkStart, chunkEnd);
            }));
    }

    // Collect and merge results
    for (auto& f : futures) {
        vector<int> chunkPrimes = f.get();
        results.insert(results.end(), chunkPrimes.begin(), chunkPrimes.end());
    }

    // Sort final result (to maintain order when chunks overlap)
    sort(results.begin(), results.end());
    return results;
}

// Sequential search for coprime numbers relative to a given number
vector<int> findCoprimesSequential(int number, int start, int end) {
    vector<int> coprimes;
    for (int i = start; i <= end; ++i) {
        if (gcdSequential(number, i) == 1) {
            coprimes.push_back(i);
        }
    }
    return coprimes;
}

// Parallel search for coprime numbers
vector<int> findCoprimesParallel(int number, int start, int end, int numThreads = thread::hardware_concurrency()) {
    vector<int> coprimes;
    vector<future<vector<int>>> futures;

    // Divide range into equal parts
    int range = end - start + 1;
    int chunkSize = max(1, range / numThreads);

    for (int i = 0; i < numThreads; ++i) {
        int chunkStart = start + i * chunkSize;
        int chunkEnd = (i == numThreads - 1) ? end : min(end, chunkStart + chunkSize - 1);

        // Launch chunk processing in a separate thread
        futures.push_back(async(launch::async, [number, chunkStart, chunkEnd]() {
            vector<int> result;
            for (int i = chunkStart; i <= chunkEnd; ++i) {
                if (gcdSequential(number, i) == 1) {
                    result.push_back(i);
                }
            }
            return result;
            }));
    }

    // Collect results
    for (auto& f : futures) {
        vector<int> chunkResult = f.get();
        coprimes.insert(coprimes.end(), chunkResult.begin(), chunkResult.end());
    }

    // Sort and remove duplicates
    sort(coprimes.begin(), coprimes.end());
    coprimes.erase(unique(coprimes.begin(), coprimes.end()), coprimes.end());

    return coprimes;
}

// Function to measure execution time
template <typename Func, typename... Args>
double measureTime(Func func, Args&&... args) {
    auto start = chrono::high_resolution_clock::now();
    func(forward<Args>(args)...);
    auto end = chrono::high_resolution_clock::now();
    return chrono::duration<double, milli>(end - start).count();
}

// Save results to file
void saveResultsToFile(const string& filename, const vector<int>& primes, const vector<int>& coprimes = {}) {
    ofstream file(filename);
    if (!file) {
        cerr << "Failed to open file for writing: " << filename << endl;
        return;
    }

    // Save prime numbers
    file << "Prime numbers in range: " << endl;
    for (int prime : primes) {
        file << prime << " ";
    }
    file << endl << endl;

    // If there are coprimes, save them too
    if (!coprimes.empty()) {
        file << "Coprime numbers with " << primes[0] << " in range: " << endl;
        for (int coprime : coprimes) {
            file << coprime << " ";
        }
        file << endl << endl;
    }

    file.close();
    cout << "Results saved to file: " << filename << endl;
}

// Menu to select program mode
int showMenu() {
    int choice;
    cout << "\nSelect operation mode:" << endl;
    cout << "1. Find all prime numbers in a range" << endl;
    cout << "2. Find prime and coprime numbers for a specific number" << endl;
    cout << "0. Exit" << endl;
    cout << "Your choice: ";
    cin >> choice;
    return choice;
}

// Get valid numeric range from user
pair<int, int> getRange() {
    int start, end;
    do {
        cout << "Enter start of range (>= 1): ";
        cin >> start;
        cout << "Enter end of range (>= start): ";
        cin >> end;

        if (start < 1 || end < start) {
            cout << "Invalid range! Try again." << endl;
        }
    } while (start < 1 || end < start);

    return { start, end };
}

// Check if a number is within the specified range
bool isInRange(int number, const pair<int, int>& range) {
    return number >= range.first && number <= range.second;
}

int main() {
    cout << "=== Prime and Coprime Number Search Program ===" << endl;

    while (true) {
        int choice = showMenu();
        if (choice == 0) {
            cout << "Exiting program." << endl;
            break;
        }

        auto range = getRange();
        vector<int> primes;
        vector<int> coprimes;

        // Measure sequential prime search
        double primesSeq = measureTime([&]() {
            primes = findPrimesSequential(range.first, range.second);
            });

        // Measure parallel prime search
        double primesPar = measureTime([&]() {
            auto parallelPrimes = findPrimesParallel(range.first, range.second);
            // Verify correctness
            if (parallelPrimes.size() != primes.size() ||
                !equal(primes.begin(), primes.end(), parallelPrimes.begin())) {
                cout << "Error! Results differ (sequential vs parallel prime search)" << endl;
            }
            });

        cout << "\nFound " << primes.size() << " prime numbers in the range "
            << range.first << " to " << range.second << endl;
        cout << "Execution time:" << endl;
        cout << "Sequential search: " << fixed << setprecision(2) << primesSeq << " ms" << endl;
        cout << "Multithreaded search: " << fixed << setprecision(2) << primesPar << " ms" << endl;
        cout << "Speedup:              " << fixed << setprecision(2) << primesSeq / primesPar << "x" << endl;

        if (choice == 2) {
            int target;
            do {
                cout << "\nEnter number to find coprimes for: ";
                cin >> target;

                if (!isInRange(target, range)) {
                    cout << "Number must be within the specified range!" << endl;
                }
            } while (!isInRange(target, range));

            // Sequential coprime search
            vector<int> seqCoprimes;
            double coprimesSeq = measureTime([&]() {
                seqCoprimes = findCoprimesSequential(target, range.first, range.second);
                });

            // Parallel coprime search
            vector<int> parCoprimes;
            double coprimesPar = measureTime([&]() {
                parCoprimes = findCoprimesParallel(target, range.first, range.second);
                });

            // Verify correctness
            if (seqCoprimes.size() != parCoprimes.size() ||
                !equal(seqCoprimes.begin(), seqCoprimes.end(), parCoprimes.begin())) {
                cout << "Error! Results differ (sequential vs parallel coprime search)" << endl;
            }

            cout << "\nFound " << seqCoprimes.size() << " coprime numbers with "
                << target << " in range" << endl;
            cout << "Execution time:" << endl;
            cout << "Sequential search: " << fixed << setprecision(2) << coprimesSeq << " ms" << endl;
            cout << "Multithreaded search: " << fixed << setprecision(2) << coprimesPar << " ms" << endl;
            cout << "Speedup:              " << fixed << setprecision(2) << coprimesSeq / coprimesPar << "x" << endl;

            // Save results to file
            string filename = "results_" + to_string(range.first) + "-" + to_string(range.second) + ".txt";
            saveResultsToFile(filename, primes, seqCoprimes);
        }
        else {
            // Save only primes to file
            string filename = "primes_" + to_string(range.first) + "-" + to_string(range.second) + ".txt";
            saveResultsToFile(filename, primes);
        }

        // Ask user if they want to view results
        char showResults;
        cout << "\nShow results? (y/n): ";
        cin >> showResults;

        if (showResults == 'y' || showResults == 'Y') {
            cout << "\nPrime numbers (" << primes.size() << " items):" << endl;
            for (size_t i = 0; i < primes.size(); ++i) {
                cout << primes[i] << " ";
                if ((i + 1) % 10 == 0) cout << endl;
            }
            cout << endl;

            if (choice == 2) {
                cout << "\n\nCoprime numbers (first 20 shown):" << endl;
                for (size_t i = 0; i < min<size_t>(20, coprimes.size()); ++i) {
                    cout << coprimes[i] << " ";
                    if ((i + 1) % 10 == 0) cout << endl;
                }
                if (coprimes.size() > 20) {
                    cout << "\n... and " << coprimes.size() - 20 << " more numbers";
                }
                cout << endl;
            }
        }

        // Ask if user wants to continue
        char continueWork;
        cout << "\nContinue with new range? (y/n): ";
        cin >> continueWork;

        if (continueWork != 'y' && continueWork != 'Y') {
            cout << "Program finished." << endl;
            break;
        }
    }

    return 0;
}
