#pragma once

#include <optionx_cpp/utils/json_comments.hpp>

#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace optionx::examples {

inline bool has_arg(int argc, char** argv, const std::string& value) {
    for (int i = 1; i < argc; ++i) {
        if (argv[i] == value) {
            return true;
        }
    }
    return false;
}

inline std::string option_value(int argc, char** argv, const std::string& name) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (argv[i] == name) {
            return argv[i + 1];
        }
    }
    return {};
}

inline std::string env_or(const char* name, std::string fallback = {}) {
    if (const char* value = std::getenv(name)) {
        return value;
    }
    return fallback;
}

inline std::int64_t wall_clock_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

inline std::int64_t monotonic_stamp_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

inline std::filesystem::path unique_temp_dir(const std::string& prefix) {
    return std::filesystem::temp_directory_path() /
           (prefix + "_" + std::to_string(monotonic_stamp_ns()));
}

template <typename Config>
bool load_json_config(const std::string& path, Config& config) {
    if (path.empty()) {
        return true;
    }

    std::ifstream input(path);
    if (!input) {
        std::cerr << "Could not open config: " << path << '\n';
        return false;
    }

    try {
        std::ostringstream buffer;
        buffer << input.rdbuf();
        config.from_json(optionx::utils::parse_json_with_comments(buffer.str()));
    } catch (const std::exception& ex) {
        std::cerr << "Could not parse config: " << ex.what() << '\n';
        return false;
    }
    return true;
}

inline std::string read_text_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return {};
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

inline std::vector<nlohmann::json> read_json_lines(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return {};
    }

    std::vector<nlohmann::json> result;
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty()) {
            result.push_back(nlohmann::json::parse(line));
        }
    }
    return result;
}

template <typename Predicate>
bool wait_until(Predicate&& predicate,
                const std::chrono::milliseconds timeout,
                const std::chrono::milliseconds interval =
                    std::chrono::milliseconds(10)) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) {
            return true;
        }
        std::this_thread::sleep_for(interval);
    }
    return predicate();
}

} // namespace optionx::examples
