#include "bridge_example_utils.hpp"

#include <optionx_cpp/bridges.hpp>

#include <client_http.hpp>
#include <client_ws.hpp>

#include <atomic>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

using HttpClient = SimpleWeb::Client<SimpleWeb::HTTP>;
using WsClient = SimpleWeb::SocketClient<SimpleWeb::WS>;
using optionx::bridges::protocol_v1::BridgeProtocolServerBridge;
using optionx::bridges::protocol_v1::BridgeProtocolServerConfig;

BridgeProtocolServerConfig default_config();
bool wait_for_transport_bind(const BridgeProtocolServerBridge& bridge);
nlohmann::json make_http_trade_open_command();
nlohmann::json make_ws_balance_command();
nlohmann::json run_http_self_test(const BridgeProtocolServerConfig& config,
                                  unsigned short port);
nlohmann::json run_websocket_self_test(const BridgeProtocolServerConfig& config,
                                       unsigned short port);
int run_self_test(const BridgeProtocolServerConfig& config,
                  BridgeProtocolServerBridge& bridge,
                  std::atomic<int>& received_signals);
void print_usage();

} // namespace

int main(int argc, char** argv) {
    if (optionx::examples::has_arg(argc, argv, "--help")) {
        print_usage();
        return 0;
    }

    const bool self_test = optionx::examples::has_arg(argc, argv, "--self-test");
    optionx::examples::install_stop_handlers();

    auto config = default_config();
    if (!optionx::examples::load_json_config(
            optionx::examples::option_value(argc, argv, "--config"), config)) {
        return 2;
    }

    const auto validation = config.validate();
    if (!validation.first) {
        std::cerr << "Invalid config: " << validation.second << '\n';
        return 2;
    }

    BridgeProtocolServerBridge bridge;
    std::atomic<optionx::SignalId> next_signal_id{1};
    std::atomic<int> received_signals{0};
    std::mutex output_mutex;

    struct BridgeCleanup {
        BridgeProtocolServerBridge& bridge;

        ~BridgeCleanup() noexcept {
            try {
                bridge.shutdown();
                bridge.on_trade_signal() = {};
                bridge.on_status_update() = {};
                bridge.on_signal_id() = {};
            } catch (...) {
            }
        }
    } cleanup{bridge};

    if (!bridge.configure(std::make_unique<BridgeProtocolServerConfig>(config))) {
        std::cerr << "Bridge configuration failed\n";
        return 2;
    }

    // The protocol bridge is transport-neutral at the callback boundary:
    // HTTP and WebSocket requests both become OptionX callbacks and DTOs.
    bridge.on_signal_id() = [&next_signal_id]() {
        return next_signal_id.fetch_add(1);
    };
    bridge.on_trade_signal() =
        [&received_signals, &output_mutex](std::unique_ptr<optionx::TradeSignal> signal) {
        ++received_signals;
        std::lock_guard<std::mutex> lock(output_mutex);
        nlohmann::json json = *signal;
        std::cout << "signal:\n" << json.dump(2) << '\n';
    };
    bridge.on_status_update() = [&output_mutex](const optionx::BridgeStatusUpdate& update) {
        std::lock_guard<std::mutex> lock(output_mutex);
        optionx::examples::print_status_update(update);
    };

    // Account snapshots feed account.balance.get and account.balance.updated.
    bridge.update_account_info(optionx::AccountInfoUpdate(
        std::make_shared<optionx::examples::DemoAccountInfo>(),
        optionx::AccountUpdateStatus::BALANCE_UPDATED));

    bridge.run();
    if (!wait_for_transport_bind(bridge)) {
        std::cerr << "Bridge did not bind HTTP and WebSocket ports\n";
        return 3;
    }

    std::cout << "Bridge Protocol v1 HTTP: http://"
              << config.address << ':' << bridge.bound_http_port()
              << config.command_path << '\n';
    std::cout << "Bridge Protocol v1 WebSocket: ws://"
              << config.address << ':' << bridge.bound_websocket_port()
              << config.websocket_path << '\n';

    if (self_test) {
        return run_self_test(config, bridge, received_signals);
    }

    std::cout << "Press Ctrl+C to stop...\n";
    while (!optionx::examples::stop_requested()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return 0;
}

namespace {

BridgeProtocolServerConfig default_config() {
    BridgeProtocolServerConfig config;
    config.bridge_id = 2;
    config.address = "127.0.0.1";
    config.http_port = 0;
    config.websocket_port = 0;
    config.secret = "local-secret";
    config.request_body_limit = 8192;
    return config;
}

bool wait_for_transport_bind(const BridgeProtocolServerBridge& bridge) {
    return optionx::examples::wait_until([&bridge]() {
        return bridge.bound_http_port() != 0 &&
               bridge.bound_websocket_port() != 0;
    }, std::chrono::seconds(3));
}

nlohmann::json make_http_trade_open_command() {
    return nlohmann::json{
        {"jsonrpc", "2.0"},
        {"id", "protocol-v1-smoke-trade"},
        {"method", "trade.open"},
        {"params", {
            {"context", {
                {"idempotency_key", "protocol-v1-smoke-trade"},
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
                {"unique_hash", "protocol-v1-smoke"},
                {"signal_name", "protocol_v1_smoke"}
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

nlohmann::json make_ws_balance_command() {
    return nlohmann::json{
        {"jsonrpc", "2.0"},
        {"id", "protocol-v1-smoke-balance"},
        {"method", "account.balance.get"},
        {"params", nlohmann::json::object()}
    };
}

nlohmann::json run_http_self_test(
        const BridgeProtocolServerConfig& config,
        const unsigned short port) {
    HttpClient client(config.address + ":" + std::to_string(port));
    SimpleWeb::CaseInsensitiveMultimap headers;
    headers.emplace("Content-Type", "application/json");
    headers.emplace("X-OptionX-Secret", config.secret);
    const auto response =
        client.request(
            "POST",
            config.command_path,
            make_http_trade_open_command().dump(-1),
            headers);
    return nlohmann::json::parse(response->content.string());
}

nlohmann::json run_websocket_self_test(
        const BridgeProtocolServerConfig& config,
        const unsigned short port) {
    WsClient client(
        config.address + ":" +
        std::to_string(port) +
        config.websocket_path);
    client.config.header.emplace("Sec-WebSocket-Protocol", config.websocket_subprotocol);
    client.config.header.emplace("X-OptionX-Secret", config.secret);

    std::mutex mutex;
    std::condition_variable cv;
    nlohmann::json response;
    std::string error_message;
    bool done = false;

    client.on_open = [&](std::shared_ptr<WsClient::Connection> connection) {
        connection->send(make_ws_balance_command().dump(-1));
    };
    client.on_message = [&](std::shared_ptr<WsClient::Connection> connection,
                            std::shared_ptr<WsClient::InMessage> message) {
        (void)connection;
        try {
            const auto parsed = nlohmann::json::parse(message->string());
            {
                std::lock_guard<std::mutex> lock(mutex);
                response = parsed;
                done = true;
            }
        } catch (const std::exception& ex) {
            std::lock_guard<std::mutex> lock(mutex);
            error_message = ex.what();
            done = true;
        }
        cv.notify_all();
    };
    client.on_error = [&](std::shared_ptr<WsClient::Connection>,
                          const SimpleWeb::error_code& ec) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (done) {
                return;
            }
            error_message = ec.message();
            done = true;
        }
        cv.notify_all();
    };

    std::thread client_thread([&client]() {
        client.start();
    });

    {
        std::unique_lock<std::mutex> lock(mutex);
        if (!cv.wait_for(lock, std::chrono::seconds(5), [&done]() {
                return done;
            })) {
            error_message = "timed out waiting for WebSocket response";
        }
    }

    client.stop();
    if (client_thread.joinable()) {
        client_thread.join();
    }

    if (!error_message.empty()) {
        throw std::runtime_error(error_message);
    }
    return response;
}

int run_self_test(
        const BridgeProtocolServerConfig& config,
        BridgeProtocolServerBridge& bridge,
        std::atomic<int>& received_signals) {
    try {
        const auto http_response =
            run_http_self_test(config, bridge.bound_http_port());
        std::cout << "HTTP self-test response: "
                  << http_response.dump(-1) << '\n';
        if (http_response.at("result").at("status").get<std::string>() != "accepted") {
            return 4;
        }

        const auto ws_response =
            run_websocket_self_test(config, bridge.bound_websocket_port());
        std::cout << "WebSocket self-test response: "
                  << ws_response.dump(-1) << '\n';
        if (ws_response.at("id").get<std::string>() !=
            "protocol-v1-smoke-balance") {
            return 4;
        }
        if (ws_response.at("result").at("status").get<std::string>() != "completed") {
            return 4;
        }
        if (ws_response.at("result")
                .at("account")
                .at("account_id")
                .get<std::string>() != "7") {
            return 4;
        }
    } catch (const std::exception& ex) {
        std::cerr << "Self-test request failed: " << ex.what() << '\n';
        return 4;
    }

    return optionx::examples::wait_for_count(
        received_signals,
        1,
        std::chrono::seconds(1)) ? 0 : 5;
}

void print_usage() {
    std::cout
        << "Usage: protocol_v1_bridge_smoke [--self-test] [--config path]\n"
        << "Starts Bridge Protocol v1 HTTP and WebSocket JSON-RPC transports.\n";
}

} // namespace
