#include <iostream>
#include <vector>
#include <thread>
#include <future>
#include <algorithm>
#include <random>
#include <chrono>
#include <iomanip>

using namespace std;
using Matrix = vector<vector<int>>;

// Generates a random matrix with values in the specified range
Matrix generateMatrix(size_t rows, size_t cols, int min = -100, int max = 100) {
    Matrix mat(rows, vector<int>(cols));
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> distr(min, max);

    // Fill matrix with random values
    for (auto& row : mat) {
        generate(row.begin(), row.end(), [&]() { return distr(gen); });
    }
    return mat;
}

// Classic matrix sorting (sorts each row)
void classicSort(Matrix& mat) {
    for (auto& row : mat) {
        sort(row.begin(), row.end());
    }
}

// Multithreaded matrix sorting (sorts each row concurrently)
void parallelSort(Matrix& mat, size_t numThreads = thread::hardware_concurrency()) {
    vector<future<void>> futures;
    size_t chunkSize = mat.size() / numThreads + 1;

    // Process each chunk in parallel
    for (size_t i = 0; i < mat.size(); i += chunkSize) {
        futures.push_back(async(launch::async,
            [&mat, i, end = min(i + chunkSize, mat.size())]() {
                // Sort rows in this chunk
                for (size_t j = i; j < end; ++j) {
                    sort(mat[j].begin(), mat[j].end());
                }
            }));
    }

    // Wait for all threads to finish
    for (auto& f : futures) f.wait();
}

// Classic matrix addition (row-wise)
Matrix classicAdd(const Matrix& a, const Matrix& b) {
    size_t rows = a.size();
    size_t cols = a[0].size();
    Matrix result(rows, vector<int>(cols));

    // Standard matrix addition
    for (size_t i = 0; i < rows; ++i) {
        for (size_t j = 0; j < cols; ++j) {
            result[i][j] = a[i][j] + b[i][j];
        }
    }
    return result;
}

// Multithreaded matrix addition (row-wise parallelization)
Matrix parallelAdd(const Matrix& a, const Matrix& b, size_t numThreads = thread::hardware_concurrency()) {
    size_t rows = a.size();
    size_t cols = a[0].size();
    Matrix result(rows, vector<int>(cols));
    vector<future<void>> futures;
    size_t chunkSize = rows / numThreads + 1;

    // Process matrix rows in parallel
    for (size_t i = 0; i < rows; i += chunkSize) {
        futures.push_back(async(launch::async,
            [&a, &b, &result, i, end = min(i + chunkSize, rows), cols]() {
                // Add rows in this chunk
                for (size_t row = i; row < end; ++row) {
                    for (size_t col = 0; col < cols; ++col) {
                        result[row][col] = a[row][col] + b[row][col];
                    }
                }
            }));
    }

    // Wait for all threads to finish
    for (auto& f : futures) f.wait();
    return result;
}

// Classic matrix subtraction (row-wise)
Matrix classicSubtract(const Matrix& a, const Matrix& b) {
    size_t rows = a.size();
    size_t cols = a[0].size();
    Matrix result(rows, vector<int>(cols));

    // Standard matrix subtraction
    for (size_t i = 0; i < rows; ++i) {
        for (size_t j = 0; j < cols; ++j) {
            result[i][j] = a[i][j] - b[i][j];
        }
    }
    return result;
}

// Multithreaded matrix subtraction (row-wise parallelization)
Matrix parallelSubtract(const Matrix& a, const Matrix& b, size_t numThreads = thread::hardware_concurrency()) {
    size_t rows = a.size();
    size_t cols = a[0].size();
    Matrix result(rows, vector<int>(cols));
    vector<future<void>> futures;
    size_t chunkSize = rows / numThreads + 1;

    // Process matrix rows in parallel
    for (size_t i = 0; i < rows; i += chunkSize) {
        futures.push_back(async(launch::async,
            [&a, &b, &result, i, end = min(i + chunkSize, rows), cols]() {
                // Subtract rows in this chunk
                for (size_t row = i; row < end; ++row) {
                    for (size_t col = 0; col < cols; ++col) {
                        result[row][col] = a[row][col] - b[row][col];
                    }
                }
            }));
    }

    // Wait for all threads to finish
    for (auto& f : futures) f.wait();
    return result;
}

// Measures execution time of a function
template <typename Func, typename... Args>
double measureTime(Func func, Args&&... args) {
    auto start = chrono::high_resolution_clock::now();
    func(forward<Args>(args)...);
    auto end = chrono::high_resolution_clock::now();
    return chrono::duration<double, milli>(end - start).count();
}

// Verifies matrix equality
bool matricesEqual(const Matrix& a, const Matrix& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i].size() != b[i].size()) return false;
        for (size_t j = 0; j < a[i].size(); ++j) {
            if (a[i][j] != b[i][j]) return false;
        }
    }
    return true;
}

int main() {
    // Test configuration
    size_t rows = 2000;
    size_t cols = 2000;
    cout << "Matrix size: " << rows << "x" << cols << endl << endl;

    // Generate test matrices
    Matrix a = generateMatrix(rows, cols);
    Matrix b = generateMatrix(rows, cols);
    Matrix c;

    // Test matrix addition
    cout << "Matrix Addition Test:\n";
    double classicTime = measureTime([&]() { c = classicAdd(a, b); });
    cout << "Classic addition: " << setw(10) << classicTime << " ms\n";

    Matrix d;
    double parallelTime = measureTime([&]() { d = parallelAdd(a, b); });
    cout << "Multithreaded addition: " << setw(8) << parallelTime << " ms\n";
    cout << "Speedup: " << setw(20) << fixed << setprecision(2) << classicTime / parallelTime << "x\n\n";

    // Verify correctness
    cout << "Addition verification: " << (matricesEqual(c, d) ? "OK" : "Error") << endl << endl;

    // Test matrix subtraction
    cout << "Matrix Subtraction Test:\n";
    classicTime = measureTime([&]() { c = classicSubtract(a, b); });
    cout << "Classic subtraction: " << setw(8) << classicTime << " ms\n";

    parallelTime = measureTime([&]() { d = parallelSubtract(a, b); });
    cout << "Multithreaded subtraction: " << setw(6) << parallelTime << " ms\n";
    cout << "Speedup: " << setw(20) << fixed << setprecision(2) << classicTime / parallelTime << "x\n\n";

    // Test sorting performance
    cout << "Matrix Sorting Test:\n";
    Matrix e = generateMatrix(rows, cols);
    classicTime = measureTime([&]() { classicSort(e); });
    cout << "Classic sorting: " << setw(6) << classicTime << " ms\n";

    Matrix f = e;
    parallelTime = measureTime([&]() { parallelSort(f); });
    cout << "Multithreaded sorting: " << setw(4) << parallelTime << " ms\n";
    cout << "Speedup: " << setw(20) << fixed << setprecision(2) << classicTime / parallelTime << "x\n";

    return 0;
}
