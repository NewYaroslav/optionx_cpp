#include <optionx_cpp/bridges/bot_binary.hpp>
#include <optionx_cpp/utils/json_comments.hpp>

#include <client_http.hpp>

#include <atomic>
#include <chrono>
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

namespace {

using BotBinaryBridge = optionx::bridges::bot_binary::BotBinaryBridge;
using BotBinaryBridgeConfig = optionx::bridges::bot_binary::BotBinaryBridgeConfig;
using HttpClient = SimpleWeb::Client<SimpleWeb::HTTP>;

std::atomic_bool g_stop_requested{false};
std::atomic_int g_interrupt_count{0};

bool has_arg(int argc, char** argv, const std::string& value) {
    for (int i = 1; i < argc; ++i) {
        if (argv[i] == value) {
            return true;
        }
    }
    return false;
}

std::string option_value(int argc, char** argv, const std::string& name) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (argv[i] == name) {
            return argv[i + 1];
        }
    }
    return {};
}

std::filesystem::path unique_smoke_dir() {
    const auto stamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    return std::filesystem::temp_directory_path() /
           ("optionx_bot_binary_bridge_smoke_" + std::to_string(stamp));
}

BotBinaryBridgeConfig default_config(const bool self_test) {
    BotBinaryBridgeConfig config;
    config.address = "127.0.0.1";
    config.port = self_test ? 0 : 6561;
    config.bridge_id = 1;
    config.signal_name = "bot_binary";
    if (self_test) {
        config.file_signal_dir = unique_smoke_dir().u8string();
        config.poll_interval_ms = 25;
    }
    return config;
}

bool load_config(
        const std::string& path,
        BotBinaryBridgeConfig& config) {
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
        const auto json = optionx::utils::parse_json_with_comments(buffer.str());
        config.from_json(json);
    } catch (const std::exception& ex) {
        std::cerr << "Could not parse config: " << ex.what() << '\n';
        return false;
    }
    return true;
}

bool wait_for_port(const BotBinaryBridge& bridge) {
    for (int i = 0; i < 300; ++i) {
        if (bridge.bound_http_port() != 0) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

bool wait_for_count(const std::atomic<int>& count, const int expected) {
    for (int i = 0; i < 300; ++i) {
        if (count.load() >= expected) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

bool wait_for_callbacks_to_drain(const std::atomic<int>& active_callbacks) {
    for (int i = 0; i < 300; ++i) {
        if (active_callbacks.load() == 0) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

void request_stop_from_interrupt() {
    const auto count = g_interrupt_count.fetch_add(1) + 1;
    if (count == 1) {
        g_stop_requested.store(true);
        return;
    }
    std::_Exit(130);
}

#ifdef _WIN32
BOOL WINAPI console_ctrl_handler(DWORD event_type) {
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
void signal_handler(int) {
    request_stop_from_interrupt();
}
#endif

void install_stop_handlers() {
    g_stop_requested.store(false);
    g_interrupt_count.store(0);
#ifdef _WIN32
    SetConsoleCtrlHandler(console_ctrl_handler, TRUE);
#else
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
#endif
}

std::string request_path(
        const BotBinaryBridgeConfig& config,
        const std::string& raw_value) {
    return config.http_path +
           "?request=" +
           optionx::bridges::bot_binary::detail::percent_encode_query_value(raw_value);
}

} // namespace

int main(int argc, char** argv) {
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    const bool self_test = has_arg(argc, argv, "--self-test");
    install_stop_handlers();

    auto config = default_config(self_test);
    if (!load_config(option_value(argc, argv, "--config"), config)) {
        return 2;
    }

    const auto validation = config.validate();
    if (!validation.first) {
        std::cerr << "Invalid config: " << validation.second << '\n';
        return 2;
    }

    std::atomic<optionx::SignalId> next_signal_id{1};
    std::atomic<int> received_signals{0};
    std::atomic<int> active_signal_callbacks{0};
    std::mutex output_mutex;
    BotBinaryBridge bridge;

    struct BridgeCleanup {
        BotBinaryBridge& bridge;

        ~BridgeCleanup() {
            bridge.shutdown();
            bridge.on_trade_signal() = {};
            bridge.on_signal_report() = {};
            bridge.on_status_update() = {};
            bridge.on_signal_id() = {};
        }
    } cleanup{bridge};

    if (!bridge.configure(std::make_unique<BotBinaryBridgeConfig>(config))) {
        std::cerr << "Bridge configuration failed\n";
        return 2;
    }

    bridge.on_signal_id() = [&next_signal_id]() {
        return next_signal_id.fetch_add(1);
    };
    bridge.on_status_update() = [&output_mutex](const optionx::BridgeStatusUpdate& update) {
        std::lock_guard<std::mutex> lock(output_mutex);
        std::cout << "status=" << optionx::to_str(update.status);
        if (!update.connection_id.empty()) {
            std::cout << " connection=" << update.connection_id;
        }
        if (!update.message.empty()) {
            std::cout << " message=" << update.message;
        }
        std::cout << '\n';
    };
    bridge.on_trade_signal() =
        [&received_signals, &active_signal_callbacks, &output_mutex](
                std::unique_ptr<optionx::TradeSignal> signal) {
        ++active_signal_callbacks;
        struct ActiveSignalCallbackGuard {
            std::atomic<int>& active_callbacks;

            ~ActiveSignalCallbackGuard() {
                --active_callbacks;
            }
        } guard{active_signal_callbacks};

        std::lock_guard<std::mutex> lock(output_mutex);
        nlohmann::json json = *signal;
        std::cout << "signal:\n" << json.dump(2) << '\n';
        ++received_signals;
    };
    bridge.on_signal_report() = [&output_mutex](const optionx::BridgeSignalReport& report) {
        std::lock_guard<std::mutex> lock(output_mutex);
        nlohmann::json json = report;
        std::cout << "signal_report:\n" << json.dump(2) << '\n';
    };

    bridge.run();
    if (config.enable_http && !wait_for_port(bridge)) {
        std::cerr << "BotBinary bridge did not bind a HTTP port\n";
        bridge.shutdown();
        return 3;
    }

    if (config.enable_http) {
        std::cout << "BotBinary HTTP bridge is listening at http://"
                  << config.address << ':' << bridge.bound_http_port()
                  << config.http_path << "?request=...\n";
    }
    if (config.enable_file_signal) {
        std::cout << "BotBinary file bridge is polling "
                  << config.file_signal_dir << '\n';
    }

    if (self_test) {
        try {
            if (config.enable_http) {
                HttpClient client(config.address + ":" + std::to_string(bridge.bound_http_port()));
                const auto response = client.request(
                    "GET",
                    request_path(config, "R_25=CALL=1.00=duration=1=m=smoke-http"));
                std::cout << "http_response="
                          << response->content.string() << '\n';
            }

            if (config.enable_file_signal) {
                std::filesystem::create_directories(
                    std::filesystem::u8path(config.file_signal_dir));
                const auto file_name =
                    std::string("R_50=PUT=0.50=duration=30=s=smoke-file.txt");
                const auto file_path =
                    std::filesystem::u8path(config.file_signal_dir) / file_name;
                std::ofstream output(file_path);
                if (!output) {
                    std::cerr << "Could not create file signal\n";
                    bridge.shutdown();
                    return 4;
                }
            }

            if (!wait_for_count(received_signals, config.enable_http && config.enable_file_signal ? 2 : 1)) {
                std::cerr << "Self-test did not receive expected BotBinary signals\n";
                bridge.shutdown();
                return 5;
            }
            if (!wait_for_callbacks_to_drain(active_signal_callbacks)) {
                std::cerr << "Self-test callbacks did not drain\n";
                bridge.shutdown();
                return 5;
            }
        } catch (const std::exception& ex) {
            std::cerr << "Self-test failed: " << ex.what() << '\n';
            bridge.shutdown();
            return 6;
        }

        bridge.shutdown();
        if (config.enable_file_signal) {
            std::error_code ec;
            std::filesystem::remove_all(
                std::filesystem::u8path(config.file_signal_dir),
                ec);
        }
        std::cout << "BotBinary bridge self-test passed\n";
        return 0;
    }

    while (!g_stop_requested.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    bridge.shutdown();
    return 0;
}
