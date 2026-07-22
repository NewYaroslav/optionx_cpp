#include <optionx_cpp/bridges.hpp>

#include <atomic>
#include <chrono>
#include <csignal>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#if defined(_WIN32)
#include <SimpleNamedPipe/NamedPipeClient.hpp>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

using optionx::bridges::legacy_trading::LegacyTradingBridge;
using optionx::bridges::legacy_trading::LegacyTradingBridgeConfig;

std::atomic_bool g_stop_requested{false};
std::atomic_int g_interrupt_count{0};

std::string unique_pipe_name() {
    const auto stamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    return "OptionXLegacyBridgeSmoke_" + std::to_string(stamp);
}

LegacyTradingBridgeConfig default_config() {
    LegacyTradingBridgeConfig config;
    config.named_pipe = unique_pipe_name();
    config.bridge_id = 1;
    config.min_payout = 0.0;
    config.ping_period_ms = 60000;
    return config;
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

void print_usage() {
    std::cout
        << "Usage: named_pipe_bridge_smoke [--self-test] [--pipe name]\n"
        << "Starts the legacy JSON named-pipe bridge.\n";
}

#if defined(_WIN32)
bool wait_for_flag(
        std::mutex& mutex,
        std::condition_variable& cv,
        bool& value,
        const std::chrono::seconds timeout) {
    std::unique_lock<std::mutex> lock(mutex);
    return cv.wait_for(lock, timeout, [&value]() {
        return value;
    });
}
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

    install_stop_handlers();

    auto config = default_config();
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::string(argv[i]) == "--pipe") {
            config.named_pipe = argv[i + 1];
        }
    }

    std::atomic<optionx::SignalId> next_signal_id{1};
    std::mutex mutex;
    std::condition_variable cv;
    bool server_started = false;
    bool signal_received = false;

    LegacyTradingBridge bridge;
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

    bridge.on_signal_id() = [&next_signal_id]() {
        return next_signal_id.fetch_add(1);
    };

    bridge.on_status_update() = [&](const optionx::BridgeStatusUpdate& update) {
        std::cout << "status=" << optionx::to_str(update.status);
        if (!update.connection_id.empty()) {
            std::cout << " connection=" << update.connection_id;
        }
        if (!update.message.empty()) {
            std::cout << " message=" << update.message;
        }
        std::cout << '\n';

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
        if (!wait_for_flag(mutex, cv, server_started, std::chrono::seconds(3))) {
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

        if (!wait_for_flag(mutex, cv, signal_received, std::chrono::seconds(1))) {
            std::cerr << "Bridge did not publish a signal\n";
            client.close();
            return 7;
        }

        client.close();
        return 0;
    }
#endif

    std::cout << "Press Ctrl+C to stop...\n";

    while (!g_stop_requested.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return 0;
}
