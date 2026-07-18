#include <optionx_cpp/bridges/metatrader_file.hpp>
#include <optionx_cpp/utils/json_comments.hpp>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
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

using optionx::bridges::metatrader_file::MetaTraderFileBridge;
using optionx::bridges::metatrader_file::MetaTraderFileBridgeConfig;

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

std::filesystem::path smoke_root() {
    return std::filesystem::temp_directory_path() / "optionx-metatrader-file-smoke";
}

MetaTraderFileBridgeConfig default_config(const bool self_test) {
    MetaTraderFileBridgeConfig config;
    config.bridge_id = 1;
    config.client_id = "smoke";
    config.poll_interval_ms = 100;
    config.max_scanned_records_per_poll = 128;
    config.max_returned_records_per_poll = 32;
    if (self_test || config.common_files_root.empty()) {
        config.common_files_root = smoke_root().u8string();
    }
    return config;
}

bool load_config(
        const std::string& path,
        MetaTraderFileBridgeConfig& config) {
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

nlohmann::json make_self_test_command() {
    const auto operation_key =
        optionx::bridges::metatrader_file::make_compact_operation_key("smoke");
    return nlohmann::json{
        {"file_seq", 1},
        {"jsonrpc", "2.0"},
        {"id", operation_key},
        {"method", "signal.submit"},
        {"params", {
            {"context", {
                {"idempotency_key", operation_key},
                {"valid_until_ms",
                 optionx::bridges::metatrader_file::detail::unix_time_ms() + 60000}
            }},
            {"identity", {
                {"signal_name", "metatrader_file_smoke"},
                {"unique_hash", operation_key}
            }},
            {"signal", {
                {"symbol", "EURUSD"},
                {"order_type", "BUY"},
                {"option_type", "SPRINT"},
                {"amount", {
                    {"value", "1.00"},
                    {"currency", "USD"}
                }},
                {"expiry", {
                    {"kind", "duration"},
                    {"duration_ms", 60000}
                }}
            }}
        }}
    };
}

bool append_command_line(
        const std::filesystem::path& commands_log,
        const nlohmann::json& command) {
    try {
        std::filesystem::create_directories(commands_log.parent_path());
        std::ofstream out(commands_log, std::ios::binary | std::ios::app);
        if (!out) {
            std::cerr << "Could not open commands log: "
                      << commands_log.u8string() << '\n';
            return false;
        }
        out << command.dump(-1) << '\n';
    } catch (const std::exception& ex) {
        std::cerr << "Could not append command: " << ex.what() << '\n';
        return false;
    }
    return true;
}

std::string read_text_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return {};
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
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

} // namespace

int main(int argc, char** argv) {
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

    if (self_test) {
        std::error_code ec;
        std::filesystem::remove_all(config.client_root(), ec);
    }

    MetaTraderFileBridge bridge;
    if (!bridge.configure(std::make_unique<MetaTraderFileBridgeConfig>(config))) {
        std::cerr << "Bridge configuration failed\n";
        return 2;
    }

    std::atomic<optionx::SignalId> next_signal_id{1};
    std::atomic<int> received_signals{0};

    bridge.on_signal_id() = [&next_signal_id]() {
        return next_signal_id.fetch_add(1);
    };
    bridge.on_status_update() = [](const optionx::BridgeStatusUpdate& update) {
        std::cout << "status=" << optionx::to_str(update.status);
        if (!update.connection_id.empty()) {
            std::cout << " connection=" << update.connection_id;
        }
        if (!update.message.empty()) {
            std::cout << " message=" << update.message;
        }
        std::cout << '\n';
    };
    bridge.on_trade_signal() = [&received_signals](std::unique_ptr<optionx::TradeSignal> signal) {
        ++received_signals;
        nlohmann::json json = *signal;
        std::cout << "signal:\n" << json.dump(2) << '\n';
    };
    bridge.on_signal_report() = [](const optionx::BridgeSignalReport& report) {
        nlohmann::json json = report;
        std::cout << "signal_report:\n" << json.dump(2) << '\n';
    };

    const auto root = config.client_root();
    const auto commands_log = root / "commands.ndjson";
    const auto events_log = root / "events.ndjson";

    std::cout << "MetaTrader file bridge root: " << root.u8string() << '\n';
    std::cout << "MQL writes commands to: " << commands_log.u8string() << '\n';
    std::cout << "OptionX writes events to: " << events_log.u8string() << '\n';

    if (self_test) {
        if (!append_command_line(commands_log, make_self_test_command())) {
            return 3;
        }

        try {
            bridge.process();
        } catch (const std::exception& ex) {
            std::cerr << "Bridge process failed: " << ex.what() << '\n';
            return 4;
        }

        const auto events = read_text_file(events_log);
        if (!events.empty()) {
            std::cout << "events.ndjson:\n" << events;
        }
        return received_signals.load() > 0 && events.find("\"status\":\"accepted\"") != std::string::npos
            ? 0
            : 5;
    }

    bridge.run();
    std::cout << "Press Enter or Ctrl+C to stop...\n";

    std::atomic_bool input_done{false};
    std::thread input_thread([&input_done]() {
        std::string line;
        std::getline(std::cin, line);
        input_done.store(true);
        g_stop_requested.store(true);
    });

    while (!g_stop_requested.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (input_thread.joinable()) {
        if (input_done.load()) {
            input_thread.join();
        } else {
            input_thread.detach();
        }
    }
    bridge.shutdown();
    return 0;
}
