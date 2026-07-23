#include "bridge_example_utils.hpp"

#include <optionx_cpp/bridges.hpp>

#include <atomic>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>

#if defined(_WIN32)
#include <SimpleNamedPipe/NamedPipeClient.hpp>
#endif

namespace {

using optionx::bridges::protocol_v1::BridgeProtocolNamedPipeBridge;
using optionx::bridges::protocol_v1::BridgeProtocolNamedPipeConfig;

BridgeProtocolNamedPipeConfig default_config();
nlohmann::json make_self_test_command();
void print_usage();

#if defined(_WIN32)
nlohmann::json pipe_request(SimpleNamedPipe::NamedPipeClient& client,
                            const nlohmann::json& request);
int run_self_test(const BridgeProtocolNamedPipeConfig& config,
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
        std::cout << "Bridge Protocol v1 named-pipe transport is Windows-only; self-test skipped.\n";
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

    BridgeProtocolNamedPipeBridge bridge;
    optionx::bridges::BridgeHost host(bridge);
    std::atomic<optionx::SignalId> next_signal_id{1};
    std::mutex mutex;
    std::condition_variable cv;
    bool server_started = false;
    bool signal_received = false;

    if (!bridge.configure(std::make_unique<BridgeProtocolNamedPipeConfig>(config))) {
        std::cerr << "Bridge configuration failed\n";
        return 2;
    }

    struct BridgeCleanup {
        optionx::bridges::BridgeHost& host;
        BridgeProtocolNamedPipeBridge& bridge;

        ~BridgeCleanup() noexcept {
            try {
                host.shutdown();
            } catch (...) {
            }
            try {
                bridge.on_status_update() = {};
                bridge.on_trade_signal() = {};
                bridge.on_signal_id() = {};
            } catch (...) {
            }
        }
    } cleanup{host, bridge};

    // Named pipe uses the same Bridge Protocol v1 JSON-RPC commands as HTTP/WS,
    // but frames responses as newline-delimited messages on the pipe.
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

    // The host layer publishes the first account snapshot before the named
    // pipe starts accepting JSON-RPC clients.
    host.set_account_info_provider([]() -> std::optional<optionx::AccountInfoUpdate> {
        return optionx::AccountInfoUpdate(
            std::make_shared<optionx::examples::DemoAccountInfo>(),
            optionx::AccountUpdateStatus::BALANCE_UPDATED,
            1007);
    });
    host.hooks().before_run = [](optionx::bridges::BridgeHost& current_host) {
        current_host.refresh_account_info();
    };

    host.run();
    std::cout << "Bridge Protocol v1 named pipe: " << config.named_pipe << '\n';

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

BridgeProtocolNamedPipeConfig default_config() {
    BridgeProtocolNamedPipeConfig config;
    config.named_pipe =
        optionx::examples::unique_pipe_name("OptionXProtocolV1PipeSmoke");
    config.bridge_id = 3;
    config.request_body_limit = 8192;
    return config;
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

void print_usage() {
    std::cout
        << "Usage: protocol_v1_named_pipe_bridge_smoke [--self-test] [--pipe name]\n"
        << "Starts Bridge Protocol v1 over a local named pipe.\n";
}

#if defined(_WIN32)
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

int run_self_test(
        const BridgeProtocolNamedPipeConfig& config,
        std::mutex& mutex,
        std::condition_variable& cv,
        bool& server_started,
        bool& signal_received) {
    if (!optionx::examples::wait_for_flag(
            mutex, cv, server_started, std::chrono::seconds(3))) {
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
