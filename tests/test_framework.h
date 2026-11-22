// tests/test_framework.h
// A deliberately tiny test helper shared by every test file in this folder.
// It exists so the project needs no external test library: each test file is
// an ordinary C++17 program that prints [PASS] / [FAIL] lines and a final
//
//     Result: <N> passed, <M> failed
//
// line, and returns a non-zero exit code if any check failed.
//
// TEMPORARY DATA ONLY: tests that touch the file system must create their
// files inside a TempDir (below), which lives in the operating system's
// temporary folder and is deleted afterwards. No test may use the project's
// real data/patients.txt.

#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <random>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

namespace testfw {

inline int passed = 0;
inline int failed = 0;

// Records one check. `label` says what is being verified.
inline void check(bool condition, const std::string& label) {
    std::cout << (condition ? "[PASS] " : "[FAIL] ") << label << "\n";
    if (condition) {
        ++passed;
    } else {
        ++failed;
    }
}

// Prints a section heading.
inline void section(const std::string& title) {
    std::cout << "--- " << title << " ---\n";
}

inline bool contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

// Number of non-overlapping occurrences of `part` in `text`.
inline int countOf(const std::string& text, const std::string& part) {
    int count = 0;
    for (std::size_t pos = text.find(part); pos != std::string::npos;
         pos = text.find(part, pos + part.size())) {
        ++count;
    }
    return count;
}

// ---- small file helpers ----------------------------------------------------

inline std::string readFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

// Writes `content` to `path`, creating missing parent folders.
inline void writeFile(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << content;
}

inline std::vector<std::string> linesOf(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        lines.push_back(line);
    }
    return lines;
}

// ---- temporary folder ------------------------------------------------------

// A fresh, uniquely named folder inside the system temporary directory
// (for example /tmp/hqms_persistence_1a2b3c4d). It is deleted again when the
// object goes out of scope, so tests leave nothing behind and can never
// collide with each other or with the project's real data folder.
class TempDir {
public:
    explicit TempDir(const std::string& testName) {
        std::random_device device;
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        std::ostringstream name;
        name << "hqms_" << testName << "_" << std::hex << device() << stamp;
        path_ = std::filesystem::temp_directory_path() / name.str();
        std::filesystem::remove_all(path_);
        std::filesystem::create_directories(path_);
    }

    ~TempDir() {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }

    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

// Prints the final summary line and returns the process exit code.
inline int finish() {
    std::cout << "\nResult: " << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}

}  // namespace testfw

#endif  // TEST_FRAMEWORK_H
