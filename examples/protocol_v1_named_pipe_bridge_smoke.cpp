#include <optionx_cpp/bridges.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
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

using optionx::bridges::protocol_v1::BridgeProtocolNamedPipeBridge;
using optionx::bridges::protocol_v1::BridgeProtocolNamedPipeConfig;

std::atomic_bool g_stop_requested{false};
std::atomic_int g_interrupt_count{0};

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

std::string unique_pipe_name() {
    const auto stamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    return "OptionXProtocolV1PipeSmoke_" + std::to_string(stamp);
}

BridgeProtocolNamedPipeConfig default_config() {
    BridgeProtocolNamedPipeConfig config;
    config.named_pipe = unique_pipe_name();
    config.bridge_id = 3;
    config.request_body_limit = 8192;
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
        << "Usage: protocol_v1_named_pipe_bridge_smoke [--self-test] [--pipe name]\n"
        << "Starts Bridge Protocol v1 over a local named pipe.\n";
}

nlohmann::json make_self_test_command() {
    return nlohmann::json{
        {"jsonrpc", "2.0"},
        {"id", "protocol-v1-pipe-smoke-trade"},
        {"method", "trade.open"},
        {"params", {
            {"context", {
                {"idempotency_key", "protocol-v1-pipe-smoke-trade"},
                {"valid_until_ms",
                 optionx::bridges::metatrader_file::detail::unix_time_ms() + 60000}
            }},
            {"routing", {
                {"selector", {
                    {"kind", "account"},
                    {"account_id", "7"}
                }}
            }},
            {"identity", {
                {"unique_hash", "protocol-v1-pipe-smoke"},
                {"signal_name", "protocol_v1_pipe_smoke"}
            }},
            {"trade", {
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

nlohmann::json pipe_request(
        SimpleNamedPipe::NamedPipeClient& client,
        const nlohmann::json& request) {
    std::error_code ec;
    if (!client.write(request.dump(-1), &ec)) {
        throw std::runtime_error("write failed: " + ec.message());
    }

    const auto expected_id = request.at("id");
    std::string buffered;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < deadline) {
        std::string chunk;
        if (!client.read(chunk, 250, &ec)) {
            continue;
        }
        buffered += chunk;
        for (;;) {
            const auto newline = buffered.find('\n');
            if (newline == std::string::npos) {
                break;
            }
            auto parsed = nlohmann::json::parse(buffered.substr(0, newline));
            buffered.erase(0, newline + 1);
            if (parsed.contains("id") && parsed.at("id") == expected_id) {
                return parsed;
            }
        }
    }
    throw std::runtime_error("read timed out");
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
        std::cout << "Bridge Protocol v1 named-pipe transport is Windows-only; self-test skipped.\n";
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

    BridgeProtocolNamedPipeBridge bridge;
    if (!bridge.configure(std::make_unique<BridgeProtocolNamedPipeConfig>(config))) {
        std::cerr << "Bridge configuration failed\n";
        return 2;
    }

    struct BridgeCleanup {
        BridgeProtocolNamedPipeBridge& bridge;

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

    bridge.update_account_info(optionx::AccountInfoUpdate(
        std::make_shared<DemoAccountInfo>(),
        optionx::AccountUpdateStatus::BALANCE_UPDATED));
    bridge.run();
    std::cout << "Bridge Protocol v1 named pipe: " << config.named_pipe << '\n';

#if defined(_WIN32)
    if (self_test) {
        if (!wait_for_flag(mutex, cv, server_started, std::chrono::seconds(3))) {
            std::cerr << "Named-pipe protocol server did not start\n";
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

        try {
            const auto response = pipe_request(client, make_self_test_command());
            std::cout << "self-test response: " << response.dump(2) << '\n';
            if (response.at("result").at("status").get<std::string>() != "accepted") {
                std::cerr << "Unexpected response status\n";
                client.close();
                return 5;
            }
        } catch (const std::exception& ex) {
            std::cerr << "Self-test request failed: " << ex.what() << '\n';
            client.close();
            return 6;
        }

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
