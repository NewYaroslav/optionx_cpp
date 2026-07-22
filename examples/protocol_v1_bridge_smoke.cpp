#include <optionx_cpp/bridges.hpp>
#include <optionx_cpp/utils/json_comments.hpp>

#include <client_http.hpp>
#include <client_ws.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <csignal>
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

using HttpClient = SimpleWeb::Client<SimpleWeb::HTTP>;
using WsClient = SimpleWeb::SocketClient<SimpleWeb::WS>;
using optionx::bridges::protocol_v1::BridgeProtocolServerBridge;
using optionx::bridges::protocol_v1::BridgeProtocolServerConfig;

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

bool load_config(const std::string& path, BridgeProtocolServerConfig& config) {
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

bool wait_for_port(const BridgeProtocolServerBridge& bridge) {
    for (int i = 0; i < 200; ++i) {
        if (bridge.bound_http_port() != 0 &&
            bridge.bound_websocket_port() != 0) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

nlohmann::json make_self_test_command() {
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

nlohmann::json make_ws_self_test_command() {
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
            make_self_test_command().dump(-1),
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
        connection->send(make_ws_self_test_command().dump(-1));
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
        << "Usage: protocol_v1_bridge_smoke [--self-test] [--config path]\n"
        << "Starts Bridge Protocol v1 HTTP and WebSocket JSON-RPC transports.\n";
}

} // namespace

int main(int argc, char** argv) {
    if (has_arg(argc, argv, "--help")) {
        print_usage();
        return 0;
    }

    const bool self_test = has_arg(argc, argv, "--self-test");
    install_stop_handlers();

    auto config = default_config();
    if (!load_config(option_value(argc, argv, "--config"), config)) {
        return 2;
    }

    const auto validation = config.validate();
    if (!validation.first) {
        std::cerr << "Invalid config: " << validation.second << '\n';
        return 2;
    }

    BridgeProtocolServerBridge bridge;
    if (!bridge.configure(std::make_unique<BridgeProtocolServerConfig>(config))) {
        std::cerr << "Bridge configuration failed\n";
        return 2;
    }

    std::atomic<optionx::SignalId> next_signal_id{1};
    std::atomic<int> received_signals{0};

    bridge.on_signal_id() = [&next_signal_id]() {
        return next_signal_id.fetch_add(1);
    };
    bridge.on_trade_signal() = [&received_signals](std::unique_ptr<optionx::TradeSignal> signal) {
        ++received_signals;
        nlohmann::json json = *signal;
        std::cout << "signal:\n" << json.dump(2) << '\n';
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

    bridge.update_account_info(optionx::AccountInfoUpdate(
        std::make_shared<DemoAccountInfo>(),
        optionx::AccountUpdateStatus::BALANCE_UPDATED));

    bridge.run();
    if (!wait_for_port(bridge)) {
        std::cerr << "Bridge did not bind HTTP and WebSocket ports\n";
        bridge.shutdown();
        return 3;
    }

    std::cout << "Bridge Protocol v1 HTTP: http://"
              << config.address << ':' << bridge.bound_http_port()
              << config.command_path << '\n';
    std::cout << "Bridge Protocol v1 WebSocket: ws://"
              << config.address << ':' << bridge.bound_websocket_port()
              << config.websocket_path << '\n';

    if (self_test) {
        try {
            const auto http_response =
                run_http_self_test(config, bridge.bound_http_port());
            std::cout << "HTTP self-test response: "
                      << http_response.dump(-1) << '\n';
            if (http_response.at("result").at("status").get<std::string>() != "accepted") {
                bridge.shutdown();
                return 4;
            }

            const auto ws_response =
                run_websocket_self_test(config, bridge.bound_websocket_port());
            std::cout << "WebSocket self-test response: "
                      << ws_response.dump(-1) << '\n';
            if (ws_response.at("id").get<std::string>() !=
                "protocol-v1-smoke-balance") {
                bridge.shutdown();
                return 4;
            }
            if (ws_response.at("result").at("status").get<std::string>() != "completed") {
                bridge.shutdown();
                return 4;
            }
            if (ws_response.at("result")
                    .at("account")
                    .at("account_id")
                    .get<std::string>() != "7") {
                bridge.shutdown();
                return 4;
            }
        } catch (const std::exception& ex) {
            std::cerr << "Self-test request failed: " << ex.what() << '\n';
            bridge.shutdown();
            return 4;
        }

        for (int i = 0; i < 100 && received_signals.load() == 0; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        bridge.shutdown();
        return received_signals.load() > 0 ? 0 : 5;
    }

    std::cout << "Press Enter or Ctrl+C to stop...\n";
    std::thread input_thread([]() {
        std::string line;
        std::getline(std::cin, line);
        g_stop_requested.store(true);
    });

    while (!g_stop_requested.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (input_thread.joinable()) {
        input_thread.detach();
    }
    bridge.shutdown();
    return 0;
}
