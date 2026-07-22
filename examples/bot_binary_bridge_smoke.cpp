#include "bridge_example_utils.hpp"

#include <optionx_cpp/bridges/bot_binary.hpp>

#include <client_http.hpp>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>

namespace {

using BotBinaryBridge = optionx::bridges::bot_binary::BotBinaryBridge;
using BotBinaryBridgeConfig = optionx::bridges::bot_binary::BotBinaryBridgeConfig;
using HttpClient = SimpleWeb::Client<SimpleWeb::HTTP>;

BotBinaryBridgeConfig default_config(bool self_test);
bool wait_for_http_bind(const BotBinaryBridge& bridge);
std::string request_path(const BotBinaryBridgeConfig& config, const std::string& raw_value);
int run_self_test(const BotBinaryBridgeConfig& config,
                  BotBinaryBridge& bridge,
                  std::atomic<int>& received_signals,
                  std::atomic<int>& active_signal_callbacks);

} // namespace

int main(int argc, char** argv) {
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

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

    BotBinaryBridge bridge;

    // Bridges publish normalized OptionX TradeSignal DTOs. The host application
    // owns signal IDs, trading execution, and optional status/report logging.
    std::atomic<optionx::SignalId> next_signal_id{1};
    std::atomic<int> received_signals{0};
    std::atomic<int> active_signal_callbacks{0};
    std::mutex output_mutex;

    struct BridgeCleanup {
        BotBinaryBridge& bridge;

        ~BridgeCleanup() noexcept {
            try {
                bridge.shutdown();
                bridge.on_trade_signal() = {};
                bridge.on_signal_report() = {};
                bridge.on_status_update() = {};
                bridge.on_signal_id() = {};
            } catch (...) {
            }
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
        optionx::examples::print_status_update(update);
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

    // BotBinary compatibility has two intake surfaces: legacy WebRequest
    // `request=...` values and file-signal filenames from MetaTrader scripts.
    bridge.run();
    if (config.enable_http && !wait_for_http_bind(bridge)) {
        std::cerr << "BotBinary bridge did not bind a HTTP port\n";
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
        return run_self_test(
            config,
            bridge,
            received_signals,
            active_signal_callbacks);
    }

    std::cout << "Press Ctrl+C to stop...\n";
    while (!optionx::examples::stop_requested()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return 0;
}

namespace {

BotBinaryBridgeConfig default_config(const bool self_test) {
    BotBinaryBridgeConfig config;
    config.address = "127.0.0.1";
    config.port = self_test ? 0 : 6561;
    config.bridge_id = 1;
    config.signal_name = "bot_binary";
    if (self_test) {
        config.file_signal_dir =
            optionx::examples::unique_temp_dir(
                "optionx_bot_binary_bridge_smoke").u8string();
        config.poll_interval_ms = 25;
    }
    return config;
}

bool wait_for_http_bind(const BotBinaryBridge& bridge) {
    return optionx::examples::wait_until([&bridge]() {
        return bridge.bound_http_port() != 0;
    }, std::chrono::seconds(3));
}

std::string request_path(
        const BotBinaryBridgeConfig& config,
        const std::string& raw_value) {
    return config.http_path +
           "?request=" +
           optionx::bridges::bot_binary::detail::percent_encode_query_value(raw_value);
}

int run_self_test(
        const BotBinaryBridgeConfig& config,
        BotBinaryBridge& bridge,
        std::atomic<int>& received_signals,
        std::atomic<int>& active_signal_callbacks) {
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
            const auto file_path =
                std::filesystem::u8path(config.file_signal_dir) /
                "R_50=PUT=0.50=duration=30=s=smoke-file.txt";
            std::ofstream output(file_path);
            if (!output) {
                std::cerr << "Could not create file signal\n";
                return 4;
            }
        }

        const int expected_signals =
            config.enable_http && config.enable_file_signal ? 2 : 1;
        if (!optionx::examples::wait_for_count(received_signals, expected_signals)) {
            std::cerr << "Self-test did not receive expected BotBinary signals\n";
            return 5;
        }
        if (!optionx::examples::wait_for_zero(active_signal_callbacks)) {
            std::cerr << "Self-test callbacks did not drain\n";
            return 5;
        }
    } catch (const std::exception& ex) {
        std::cerr << "Self-test failed: " << ex.what() << '\n';
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

} // namespace
