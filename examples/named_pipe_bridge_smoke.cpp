#include "bridge_example_utils.hpp"

#include <optionx_cpp/bridges.hpp>

#include <atomic>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>

#if defined(_WIN32)
#include <SimpleNamedPipe/NamedPipeClient.hpp>
#endif

namespace {

using optionx::bridges::legacy_trading::LegacyTradingBridge;
using optionx::bridges::legacy_trading::LegacyTradingBridgeConfig;

LegacyTradingBridgeConfig default_config();
void print_usage();

#if defined(_WIN32)
int run_self_test(const LegacyTradingBridgeConfig& config,
                  std::mutex& mutex,
                  std::condition_variable& cv,
                  bool& server_started,
                  bool& signal_received);
#endif

} // namespace

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--help") {
        print_usage();
        return 0;
    }

    const bool self_test =
        argc > 1 && std::string(argv[1]) == "--self-test";

#if !defined(_WIN32)
    if (self_test) {
        std::cout << "Legacy named-pipe bridge is Windows-only; self-test skipped.\n";
        return 0;
    }
#endif

    optionx::examples::install_stop_handlers();

    auto config = default_config();
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::string(argv[i]) == "--pipe") {
            config.named_pipe = argv[i + 1];
        }
    }

    LegacyTradingBridge bridge;
    std::atomic<optionx::SignalId> next_signal_id{1};
    std::mutex mutex;
    std::condition_variable cv;
    bool server_started = false;
    bool signal_received = false;

    if (!bridge.configure(std::make_unique<LegacyTradingBridgeConfig>(config))) {
        std::cerr << "Bridge configuration failed\n";
        return 2;
    }

    struct BridgeCleanup {
        LegacyTradingBridge& bridge;

        ~BridgeCleanup() noexcept {
            try {
                bridge.shutdown();
                bridge.on_status_update() = {};
                bridge.on_trade_signal() = {};
                bridge.on_signal_id() = {};
            } catch (...) {
            }
        }
    } cleanup{bridge};

    // LegacyTradingBridge accepts the older named-pipe JSON shape and converts
    // it into the same OptionX TradeSignal callback used by newer bridges.
    bridge.on_signal_id() = [&next_signal_id]() {
        return next_signal_id.fetch_add(1);
    };

    bridge.on_status_update() = [&](const optionx::BridgeStatusUpdate& update) {
        optionx::examples::print_status_update(update);

        {
            std::lock_guard<std::mutex> lock(mutex);
            if (update.status == optionx::BridgeStatus::SERVER_STARTED) {
                server_started = true;
            }
        }
        cv.notify_all();
    };

    bridge.on_trade_signal() = [&](std::unique_ptr<optionx::TradeSignal> signal) {
        if (!signal) {
            return;
        }
        nlohmann::json json = *signal;
        std::cout << "signal:\n" << json.dump(2) << '\n';

        optionx::TradeResult result;
        result.trade_id = signal->signal_id;
        result.option_id = 1000 + signal->signal_id;
        result.amount = signal->amount;
        result.profit = signal->amount * 0.82;
        result.payout = 0.82;
        result.trade_state = optionx::TradeState::WIN;
        bridge.update_trade_result(signal->to_trade_request(), result);

        {
            std::lock_guard<std::mutex> lock(mutex);
            signal_received = true;
        }
        cv.notify_all();
    };

    bridge.run();
    std::cout << "Legacy named-pipe bridge pipe: " << config.named_pipe << '\n';

#if defined(_WIN32)
    if (self_test) {
        return run_self_test(config, mutex, cv, server_started, signal_received);
    }
#endif

    std::cout << "Press Ctrl+C to stop...\n";
    while (!optionx::examples::stop_requested()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return 0;
}

namespace {

LegacyTradingBridgeConfig default_config() {
    LegacyTradingBridgeConfig config;
    config.named_pipe =
        optionx::examples::unique_pipe_name("OptionXLegacyBridgeSmoke");
    config.bridge_id = 1;
    config.min_payout = 0.0;
    config.ping_period_ms = 60000;
    return config;
}

void print_usage() {
    std::cout
        << "Usage: named_pipe_bridge_smoke [--self-test] [--pipe name]\n"
        << "Starts the legacy JSON named-pipe bridge.\n";
}

#if defined(_WIN32)
int run_self_test(
        const LegacyTradingBridgeConfig& config,
        std::mutex& mutex,
        std::condition_variable& cv,
        bool& server_started,
        bool& signal_received) {
    if (!optionx::examples::wait_for_flag(
            mutex, cv, server_started, std::chrono::seconds(3))) {
        std::cerr << "Named-pipe server did not start\n";
        return 3;
    }

    SimpleNamedPipe::ClientConfig client_config(
        config.named_pipe,
        config.buffer_size,
        3000);
    SimpleNamedPipe::NamedPipeClient client(client_config);
    std::error_code ec;
    if (!client.connect(&ec)) {
        std::cerr << "Client connect failed: " << ec.message() << '\n';
        return 4;
    }

    const std::string command = R"json({
        "contract": {
            "s": "EURUSD",
            "note": "named-pipe-smoke&payload",
            "a": 1.0,
            "dir": "BUY",
            "dur": 60
        }
    })json";
    if (!client.write(command, &ec)) {
        std::cerr << "Client write failed: " << ec.message() << '\n';
        client.close();
        return 5;
    }

    std::string response;
    if (!client.read(response, 3000, &ec)) {
        std::cerr << "Client read failed: " << ec.message() << '\n';
        client.close();
        return 6;
    }
    std::cout << "self-test response: " << response << '\n';

    if (!optionx::examples::wait_for_flag(
            mutex, cv, signal_received, std::chrono::seconds(1))) {
        std::cerr << "Bridge did not publish a signal\n";
        client.close();
        return 7;
    }

    client.close();
    return 0;
}
#endif

} // namespace
