#pragma once

#include <optionx_cpp/data.hpp>
#include <optionx_cpp/utils.hpp>

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>

namespace optionx::tests::intrade_bar_smoke {

inline std::string trim(std::string value) {
    return optionx::utils::trim_copy(value);
}

inline std::unordered_map<std::string, std::string> read_env_file(const std::string& path) {
    std::unordered_map<std::string, std::string> values;
    std::ifstream file(path);
    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        values[trim(line.substr(0, eq))] = trim(line.substr(eq + 1));
    }
    return values;
}

inline std::string getenv_or_empty(const char* name) {
    const char* value = std::getenv(name);
    return value ? std::string(value) : std::string();
}

inline std::string config_value(
        const std::unordered_map<std::string, std::string>& file_values,
        const char* key,
        std::string fallback = {}) {
    std::string value = getenv_or_empty(key);
    if (!value.empty()) return value;
    auto it = file_values.find(key);
    return it == file_values.end() ? std::move(fallback) : it->second;
}

inline bool parse_bool(const std::string& value, bool fallback = false) {
    const auto trimmed = trim(value);
    if (trimmed == "1" ||
        trimmed == "true" ||
        trimmed == "TRUE" ||
        trimmed == "yes" ||
        trimmed == "YES") {
        return true;
    }
    if (trimmed == "0" ||
        trimmed == "false" ||
        trimmed == "FALSE" ||
        trimmed == "no" ||
        trimmed == "NO") {
        return false;
    }
    return fallback;
}

inline int parse_int(const std::string& value, int fallback) {
    const auto trimmed = trim(value);
    if (trimmed.empty()) return fallback;
    const auto parsed = optionx::utils::parse_int_strict(trimmed);
    return parsed.value_or(fallback);
}

inline int64_t parse_i64(const std::string& value, int64_t fallback) {
    const auto trimmed = trim(value);
    if (trimmed.empty()) return fallback;
    const auto parsed = optionx::utils::parse_i64_strict(trimmed);
    return parsed.value_or(fallback);
}

inline double parse_double(const std::string& value, double fallback) {
    const auto trimmed = trim(value);
    if (trimmed.empty()) return fallback;
    const auto parsed = optionx::utils::parse_double_strict(trimmed);
    return parsed.value_or(fallback);
}

template<class EnumT>
EnumT parse_enum_or(const std::string& value, EnumT fallback) {
    const auto trimmed = trim(value);
    try {
        return trimmed.empty() ? fallback : optionx::to_enum<EnumT>(trimmed);
    } catch (...) {
        return fallback;
    }
}

inline std::string option_value_or(
        const std::map<std::string, std::string>& values,
        const std::string& key,
        std::string fallback) {
    auto it = values.find(key);
    return it == values.end() ? std::move(fallback) : it->second;
}

inline double option_double_or(
        const std::map<std::string, std::string>& values,
        const std::string& key,
        double fallback) {
    auto it = values.find(key);
    return it == values.end() ? fallback : parse_double(it->second, fallback);
}

inline int64_t option_i64_or(
        const std::map<std::string, std::string>& values,
        const std::string& key,
        int64_t fallback) {
    auto it = values.find(key);
    return it == values.end() ? fallback : parse_i64(it->second, fallback);
}

inline optionx::AccountType opposite_account_type(optionx::AccountType value) {
    return value == optionx::AccountType::DEMO ?
        optionx::AccountType::REAL : optionx::AccountType::DEMO;
}

inline optionx::CurrencyType opposite_currency(optionx::CurrencyType value) {
    return value == optionx::CurrencyType::USD ?
        optionx::CurrencyType::RUB : optionx::CurrencyType::USD;
}

} // namespace optionx::tests::intrade_bar_smoke
