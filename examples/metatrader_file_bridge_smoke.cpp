#include "bridge_example_utils.hpp"

#include <optionx_cpp/bridges/metatrader_file.hpp>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>

namespace {

using optionx::bridges::metatrader_file::MetaTraderFileBridge;
using optionx::bridges::metatrader_file::MetaTraderFileBridgeConfig;

MetaTraderFileBridgeConfig default_config(bool self_test);
nlohmann::json make_self_test_command();
bool append_command_line(const std::filesystem::path& commands_log,
                         const nlohmann::json& command);
std::string read_text_file(const std::filesystem::path& path);
int run_self_test(const MetaTraderFileBridgeConfig& config,
                  MetaTraderFileBridge& bridge,
                  std::atomic<int>& received_signals);

} // namespace

int main(int argc, char** argv) {
    const bool self_test = optionx::examples::has_arg(argc, argv, "--self-test");
    optionx::examples::install_stop_handlers();

    auto config = default_config(self_test);
    if (!optionx::examples::load_json_config(
            optionx::examples::option_value(argc, argv, "--config"), config)) {
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

    // The C++ side reads commands written by MT4/MT5 scripts and writes
    // JSON-RPC result/events back into the same client directory.
    bridge.on_signal_id() = [&next_signal_id]() {
        return next_signal_id.fetch_add(1);
    };
    bridge.on_status_update() = [](const optionx::BridgeStatusUpdate& update) {
        optionx::examples::print_status_update(update);
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
        return run_self_test(config, bridge, received_signals);
    }

    bridge.run();
    std::cout << "Press Enter or Ctrl+C to stop...\n";

    std::atomic_bool input_done{false};
    std::thread input_thread([&input_done]() {
        std::string line;
        std::getline(std::cin, line);
        input_done.store(true);
        optionx::examples::request_stop();
    });

    while (!optionx::examples::stop_requested()) {
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

namespace {

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

int run_self_test(
        const MetaTraderFileBridgeConfig& config,
        MetaTraderFileBridge& bridge,
        std::atomic<int>& received_signals) {
    const auto root = config.client_root();
    const auto commands_log = root / "commands.ndjson";
    const auto events_log = root / "events.ndjson";

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
    return received_signals.load() > 0 &&
           events.find("\"status\":\"accepted\"") != std::string::npos
        ? 0
        : 5;
}

} // namespace
