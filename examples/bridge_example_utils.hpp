#pragma once

#include <optionx_cpp/bridges.hpp>
#include <optionx_cpp/utils/json_comments.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace optionx::examples {

inline std::atomic_bool g_stop_requested{false};
inline std::atomic_int g_interrupt_count{0};

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

inline std::int64_t monotonic_stamp_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

inline std::filesystem::path unique_temp_dir(const std::string& prefix) {
    return std::filesystem::temp_directory_path() /
           (prefix + "_" + std::to_string(monotonic_stamp_ns()));
}

inline std::string unique_pipe_name(const std::string& prefix) {
    return prefix + "_" + std::to_string(monotonic_stamp_ns());
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

inline bool wait_for_count(const std::atomic<int>& count,
                           const int expected,
                           const std::chrono::milliseconds timeout =
                               std::chrono::seconds(3)) {
    return wait_until([&count, expected]() {
        return count.load() >= expected;
    }, timeout);
}

inline bool wait_for_zero(const std::atomic<int>& count,
                          const std::chrono::milliseconds timeout =
                              std::chrono::seconds(3)) {
    return wait_until([&count]() {
        return count.load() == 0;
    }, timeout);
}

inline bool wait_for_flag(std::mutex& mutex,
                          std::condition_variable& cv,
                          bool& value,
                          const std::chrono::seconds timeout) {
    std::unique_lock<std::mutex> lock(mutex);
    return cv.wait_for(lock, timeout, [&value]() {
        return value;
    });
}

inline void print_status_update(const optionx::BridgeStatusUpdate& update) {
    std::cout << "status=" << optionx::to_str(update.status);
    if (!update.connection_id.empty()) {
        std::cout << " connection=" << update.connection_id;
    }
    if (!update.message.empty()) {
        std::cout << " message=" << update.message;
    }
    std::cout << '\n';
}

class DemoAccountInfo final : public optionx::BaseAccountInfoData {
public:
    std::int64_t user_id = 7;
    double balance = 1000.0;
    optionx::CurrencyType currency = optionx::CurrencyType::USD;
    optionx::AccountType account_type = optionx::AccountType::DEMO;

    std::unique_ptr<optionx::BaseAccountInfoData> clone_unique() const override {
        return std::make_unique<DemoAccountInfo>(*this);
    }

    std::shared_ptr<optionx::BaseAccountInfoData> clone_shared() const override {
        return std::make_shared<DemoAccountInfo>(*this);
    }

private:
    bool get_info_bool(const optionx::AccountInfoRequest& request) const override {
        return request.type == optionx::AccountInfoType::CONNECTION_STATUS;
    }

    std::int64_t get_info_int64(const optionx::AccountInfoRequest& request) const override {
        switch (request.type) {
        case optionx::AccountInfoType::USER_ID:
            return user_id;
        case optionx::AccountInfoType::CONNECTION_STATUS:
            return 1;
        case optionx::AccountInfoType::ACCOUNT_TYPE:
            return static_cast<std::int64_t>(account_type);
        case optionx::AccountInfoType::CURRENCY:
            return static_cast<std::int64_t>(currency);
        default:
            return 0;
        }
    }

    double get_info_f64(const optionx::AccountInfoRequest& request) const override {
        return request.type == optionx::AccountInfoType::BALANCE ? balance : 0.0;
    }

    std::string get_info_str(const optionx::AccountInfoRequest& request) const override {
        return request.type == optionx::AccountInfoType::USER_ID
            ? std::to_string(user_id)
            : std::string();
    }

    optionx::AccountType get_info_account_type(
            const optionx::AccountInfoRequest&) const override {
        return account_type;
    }

    optionx::CurrencyType get_info_currency(
            const optionx::AccountInfoRequest&) const override {
        return currency;
    }
};

inline void request_stop_from_interrupt() {
    const auto count = g_interrupt_count.fetch_add(1) + 1;
    if (count == 1) {
        g_stop_requested.store(true);
        return;
    }
    std::_Exit(130);
}

#ifdef _WIN32
inline BOOL WINAPI console_ctrl_handler(DWORD event_type) {
    switch (event_type) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        request_stop_from_interrupt();
        return TRUE;
    default:
        return FALSE;
    }
}
#else
inline void signal_handler(int) {
    request_stop_from_interrupt();
}
#endif

inline void install_stop_handlers() {
    g_stop_requested.store(false);
    g_interrupt_count.store(0);
#ifdef _WIN32
    SetConsoleCtrlHandler(console_ctrl_handler, TRUE);
#else
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
#endif
}

inline bool stop_requested() {
    return g_stop_requested.load();
}

inline void request_stop() {
    g_stop_requested.store(true);
}

} // namespace optionx::examples
