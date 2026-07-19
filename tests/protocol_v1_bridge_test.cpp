#include <gtest/gtest.h>

#include <optionx_cpp/bridges.hpp>

#include <client_http.hpp>
#include <client_ws.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

using HttpClient = SimpleWeb::Client<SimpleWeb::HTTP>;
using WsClient = SimpleWeb::SocketClient<SimpleWeb::WS>;

class TestAccountInfo final : public optionx::BaseAccountInfoData {
public:
    std::int64_t user_id = 99;
    double balance = 321.25;
    optionx::CurrencyType currency = optionx::CurrencyType::USD;
    optionx::AccountType account_type = optionx::AccountType::DEMO;

    std::unique_ptr<optionx::BaseAccountInfoData> clone_unique() const override {
        return std::make_unique<TestAccountInfo>(*this);
    }

    std::shared_ptr<optionx::BaseAccountInfoData> clone_shared() const override {
        return std::make_shared<TestAccountInfo>(*this);
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

optionx::bridges::protocol_v1::BridgeProtocolServerConfig test_config() {
    optionx::bridges::protocol_v1::BridgeProtocolServerConfig config;
    config.bridge_id = 3;
    config.address = "127.0.0.1";
    config.http_port = 0;
    config.websocket_port = 0;
    config.secret = "secret";
    config.request_body_limit = 8192;
    config.dedupe_cache_size = 32;
    return config;
}

bool wait_for_http_port(
        const optionx::bridges::protocol_v1::BridgeProtocolServerBridge& bridge) {
    for (int i = 0; i < 200; ++i) {
        if (bridge.bound_http_port() != 0) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

bool wait_for_ws_port(
        const optionx::bridges::protocol_v1::BridgeProtocolServerBridge& bridge) {
    for (int i = 0; i < 200; ++i) {
        if (bridge.bound_websocket_port() != 0) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

nlohmann::json trade_command(
        std::string id,
        std::string idempotency_key,
        std::string symbol = "EURUSD") {
    return nlohmann::json{
        {"jsonrpc", "2.0"},
        {"id", std::move(id)},
        {"method", "trade.open"},
        {"params", {
            {"context", {
                {"idempotency_key", std::move(idempotency_key)},
                {"valid_until_ms",
                 optionx::bridges::metatrader_file::detail::unix_time_ms() + 60000}
            }},
            {"routing", {
                {"selector", {
                    {"kind", "account"},
                    {"account_id", "99"}
                }}
            }},
            {"identity", {
                {"unique_hash", "protocol-v1-test"},
                {"signal_name", "protocol_v1"}
            }},
            {"trade", {
                {"symbol", std::move(symbol)},
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

nlohmann::json ws_trade_command(std::string id, std::string idempotency_key) {
    return trade_command(std::move(id), std::move(idempotency_key));
}

nlohmann::json post_json(
        const optionx::bridges::protocol_v1::BridgeProtocolServerConfig& config,
        const unsigned short port,
        const nlohmann::json& request) {
    HttpClient client(config.address + ":" + std::to_string(port));
    SimpleWeb::CaseInsensitiveMultimap headers;
    headers.emplace("Content-Type", "application/json");
    headers.emplace("X-OptionX-Secret", config.secret);
    const auto response =
        client.request("POST", config.command_path, request.dump(-1), headers);
    return nlohmann::json::parse(response->content.string());
}

nlohmann::json post_json_without_auth(
        const optionx::bridges::protocol_v1::BridgeProtocolServerConfig& config,
        const unsigned short port,
        const nlohmann::json& request) {
    HttpClient client(config.address + ":" + std::to_string(port));
    SimpleWeb::CaseInsensitiveMultimap headers;
    headers.emplace("Content-Type", "application/json");
    const auto response =
        client.request("POST", config.command_path, request.dump(-1), headers);
    return nlohmann::json::parse(response->content.string());
}

} // namespace

TEST(BridgeProtocolServerConfig, RoundTripsAndValidates) {
    namespace proto = optionx::bridges::protocol_v1;

    proto::BridgeProtocolServerConfig config = test_config();
    nlohmann::json json;
    config.to_json(json);

    proto::BridgeProtocolServerConfig restored;
    restored.from_json(json);

    EXPECT_TRUE(restored.validate().first);
    EXPECT_EQ(restored.bridge_type(), optionx::BridgeType::BRIDGE_PROTOCOL_V1_HTTP_WEBSOCKET);
    EXPECT_EQ(restored.command_path, "/api/v1/bridge/command");
    EXPECT_FALSE(restored.allow_unauthenticated_local);
    EXPECT_FALSE(restored.allow_cors);

    restored.enable_http = false;
    restored.enable_websocket = false;
    EXPECT_FALSE(restored.validate().first);

    auto unsafe = test_config();
    unsafe.secret.clear();
    EXPECT_FALSE(unsafe.validate().first);

    unsafe.allow_unauthenticated_local = true;
    EXPECT_TRUE(unsafe.validate().first);

    unsafe.address = "0.0.0.0";
    EXPECT_FALSE(unsafe.validate().first);

    unsafe.address = "127.0.0.1";
    unsafe.allow_cors = true;
    unsafe.allowed_origin = "*";
    EXPECT_FALSE(unsafe.validate().first);
}

TEST(BridgeProtocolServerBridge, AcceptsHttpJsonRpcCommands) {
    namespace proto = optionx::bridges::protocol_v1;

    auto config = test_config();
    config.enable_websocket = false;

    proto::BridgeProtocolServerBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolServerConfig>(config)));

    std::atomic<optionx::SignalId> next_signal_id{10};
    std::atomic<int> signal_count{0};
    bridge.on_signal_id() = [&next_signal_id]() {
        return next_signal_id.fetch_add(1);
    };
    bridge.on_trade_signal() = [&signal_count](std::unique_ptr<optionx::TradeSignal> signal) {
        ASSERT_EQ(signal->symbol, "EURUSD");
        ++signal_count;
    };
    bridge.update_account_info(optionx::AccountInfoUpdate(
        std::make_shared<TestAccountInfo>(),
        optionx::AccountUpdateStatus::BALANCE_UPDATED));

    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));

    const auto hello = post_json(
        config,
        bridge.bound_http_port(),
        nlohmann::json{
            {"jsonrpc", "2.0"},
            {"id", "hello-1"},
            {"method", "protocol.hello"},
            {"params", nlohmann::json::object()}
        });
    EXPECT_EQ(hello.at("result").at("selected_protocol_version").get<std::string>(), "1");

    const auto balance = post_json(
        config,
        bridge.bound_http_port(),
        nlohmann::json{
            {"jsonrpc", "2.0"},
            {"id", "balance-1"},
            {"method", "account.balance.get"},
            {"params", nlohmann::json::object()}
        });
    EXPECT_EQ(balance.at("result").at("status").get<std::string>(), "completed");
    EXPECT_EQ(balance.at("result").at("account").at("account_id").get<std::string>(), "99");

    const auto accepted = post_json(
        config,
        bridge.bound_http_port(),
        trade_command("trade-1", "idem-1"));
    EXPECT_EQ(accepted.at("result").at("status").get<std::string>(), "accepted");
    EXPECT_EQ(signal_count.load(), 1);

    const auto retry = post_json(
        config,
        bridge.bound_http_port(),
        trade_command("trade-1-retry", "idem-1"));
    EXPECT_EQ(retry.at("result").at("signal_ref").at("signal_id").get<std::string>(), "10");
    EXPECT_EQ(signal_count.load(), 1);

    const auto conflict = post_json(
        config,
        bridge.bound_http_port(),
        trade_command("trade-1-conflict", "idem-1", "GBPUSD"));
    EXPECT_EQ(conflict.at("result").at("status").get<std::string>(), "rejected");
    EXPECT_EQ(
        conflict.at("result").at("reason").at("code").get<std::string>(),
        "idempotency_conflict");

    bridge.shutdown();
}

TEST(BridgeProtocolServerBridge, RejectsMissingHttpAuthorization) {
    namespace proto = optionx::bridges::protocol_v1;

    auto config = test_config();
    config.enable_websocket = false;

    proto::BridgeProtocolServerBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolServerConfig>(config)));
    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));

    const auto response = post_json_without_auth(
        config,
        bridge.bound_http_port(),
        nlohmann::json{
            {"jsonrpc", "2.0"},
            {"id", "hello-unauthorized"},
            {"method", "protocol.hello"},
            {"params", nlohmann::json::object()}
        });
    bridge.shutdown();

    ASSERT_TRUE(response.contains("error"));
    EXPECT_EQ(response.at("error").at("code").get<int>(), -32001);
}

TEST(BridgeProtocolServerBridge, DoesNotDeriveUniqueHashFromIdempotencyKey) {
    namespace proto = optionx::bridges::protocol_v1;

    auto config = test_config();
    config.enable_websocket = false;

    proto::BridgeProtocolServerBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolServerConfig>(config)));

    std::string unique_hash = "not-called";
    bridge.on_signal_id() = []() { return optionx::SignalId{20}; };
    bridge.on_trade_signal() = [&unique_hash](std::unique_ptr<optionx::TradeSignal> signal) {
        unique_hash = signal->unique_hash;
    };

    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));

    auto command = trade_command("trade-no-hash", "idem-no-hash");
    command["params"].erase("identity");
    const auto response = post_json(config, bridge.bound_http_port(), command);
    bridge.shutdown();

    EXPECT_EQ(response.at("result").at("status").get<std::string>(), "accepted");
    EXPECT_TRUE(unique_hash.empty());
}

TEST(BridgeProtocolServerBridge, RejectsUnsupportedRoutingShapes) {
    namespace proto = optionx::bridges::protocol_v1;

    auto config = test_config();
    config.enable_websocket = false;

    proto::BridgeProtocolServerBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolServerConfig>(config)));
    bridge.on_signal_id() = []() { return optionx::SignalId{30}; };
    bridge.on_trade_signal() = [](std::unique_ptr<optionx::TradeSignal>) {};

    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));

    auto trade_all = trade_command("trade-all", "idem-route-all");
    trade_all["params"]["routing"]["selector"] = {{"kind", "all"}};
    const auto trade_response = post_json(config, bridge.bound_http_port(), trade_all);

    auto signal_policy = trade_command("signal-policy", "idem-route-policy");
    signal_policy["method"] = "signal.submit";
    signal_policy["params"]["signal"] = signal_policy["params"]["trade"];
    signal_policy["params"].erase("trade");
    signal_policy["params"]["routing"]["policy"] = {{"kind", "fan_out"}};
    const auto signal_response = post_json(config, bridge.bound_http_port(), signal_policy);

    bridge.shutdown();

    ASSERT_TRUE(trade_response.contains("error"));
    EXPECT_EQ(trade_response.at("error").at("code").get<int>(), -32602);
    ASSERT_TRUE(signal_response.contains("error"));
    EXPECT_EQ(signal_response.at("error").at("code").get<int>(), -32602);
}

TEST(BridgeProtocolServerBridge, FailsClosedWhenIdempotencyCacheIsFull) {
    namespace proto = optionx::bridges::protocol_v1;

    auto config = test_config();
    config.enable_websocket = false;
    config.dedupe_cache_size = 1;

    proto::BridgeProtocolServerBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolServerConfig>(config)));

    std::atomic<int> signal_count{0};
    bridge.on_signal_id() = []() {
        static std::atomic<optionx::SignalId> next{40};
        return next.fetch_add(1);
    };
    bridge.on_trade_signal() = [&signal_count](std::unique_ptr<optionx::TradeSignal>) {
        ++signal_count;
    };

    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));

    const auto first = post_json(
        config,
        bridge.bound_http_port(),
        trade_command("trade-cache-1", "idem-cache-1"));
    const auto second = post_json(
        config,
        bridge.bound_http_port(),
        trade_command("trade-cache-2", "idem-cache-2"));
    const auto retry = post_json(
        config,
        bridge.bound_http_port(),
        trade_command("trade-cache-1-retry", "idem-cache-1"));
    bridge.shutdown();

    EXPECT_EQ(first.at("result").at("status").get<std::string>(), "accepted");
    EXPECT_EQ(second.at("result").at("status").get<std::string>(), "rejected");
    EXPECT_EQ(
        second.at("result").at("reason").at("code").get<std::string>(),
        "idempotency_cache_full");
    EXPECT_EQ(retry.at("result").at("signal_ref").at("signal_id").get<std::string>(), "40");
    EXPECT_EQ(signal_count.load(), 1);
}

TEST(BridgeProtocolServerBridge, DistinguishesStringAndNumericJsonRpcIds) {
    namespace proto = optionx::bridges::protocol_v1;

    auto config = test_config();
    config.enable_websocket = false;

    proto::BridgeProtocolServerBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolServerConfig>(config)));

    std::atomic<optionx::SignalId> next_signal_id{50};
    std::atomic<int> signal_count{0};
    bridge.on_signal_id() = [&next_signal_id]() {
        return next_signal_id.fetch_add(1);
    };
    bridge.on_trade_signal() = [&signal_count](std::unique_ptr<optionx::TradeSignal>) {
        ++signal_count;
    };

    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));

    auto string_id = trade_command("1", "idem-string-id");
    auto numeric_id = trade_command("unused", "idem-numeric-id");
    numeric_id["id"] = 1;
    const auto first = post_json(config, bridge.bound_http_port(), string_id);
    const auto second = post_json(config, bridge.bound_http_port(), numeric_id);
    bridge.shutdown();

    EXPECT_EQ(first.at("result").at("status").get<std::string>(), "accepted");
    EXPECT_EQ(second.at("result").at("status").get<std::string>(), "accepted");
    EXPECT_EQ(signal_count.load(), 2);
}

TEST(BridgeProtocolServerBridge, RegeneratesStreamIdOnRestart) {
    namespace proto = optionx::bridges::protocol_v1;

    auto config = test_config();
    config.enable_websocket = false;

    proto::BridgeProtocolServerBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolServerConfig>(config)));

    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));
    const auto first = post_json(
        config,
        bridge.bound_http_port(),
        nlohmann::json{
            {"jsonrpc", "2.0"},
            {"id", "hello-1"},
            {"method", "protocol.hello"},
            {"params", nlohmann::json::object()}
        });
    bridge.shutdown();

    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));
    const auto second = post_json(
        config,
        bridge.bound_http_port(),
        nlohmann::json{
            {"jsonrpc", "2.0"},
            {"id", "hello-2"},
            {"method", "protocol.hello"},
            {"params", nlohmann::json::object()}
        });
    bridge.shutdown();

    EXPECT_NE(
        first.at("result").at("session_id").get<std::string>(),
        second.at("result").at("session_id").get<std::string>());
}

TEST(BridgeProtocolServerBridge, DedupesConcurrentHttpAndWebSocketTradeRetry) {
    namespace proto = optionx::bridges::protocol_v1;

    auto config = test_config();

    proto::BridgeProtocolServerBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolServerConfig>(config)));

    std::atomic<optionx::SignalId> next_signal_id{60};
    std::atomic<int> signal_count{0};
    bridge.on_signal_id() = [&next_signal_id]() {
        return next_signal_id.fetch_add(1);
    };
    bridge.on_trade_signal() = [&signal_count](std::unique_ptr<optionx::TradeSignal>) {
        ++signal_count;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    };

    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));
    ASSERT_TRUE(wait_for_ws_port(bridge));

    std::mutex mutex;
    std::condition_variable cv;
    std::vector<nlohmann::json> ws_messages;
    bool opened = false;

    WsClient client(
        config.address + ":" +
        std::to_string(bridge.bound_websocket_port()) +
        config.websocket_path);
    client.config.header.emplace("X-OptionX-Secret", config.secret);
    client.on_open = [&](std::shared_ptr<WsClient::Connection> connection) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            opened = true;
        }
        cv.notify_all();
        connection->send(ws_trade_command("ws-duplicate", "idem-concurrent").dump(-1));
    };
    client.on_message = [&](std::shared_ptr<WsClient::Connection> connection,
                            std::shared_ptr<WsClient::InMessage> message) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            ws_messages.push_back(nlohmann::json::parse(message->string()));
        }
        connection->send_close(1000);
        cv.notify_all();
    };
    client.on_error = [&](std::shared_ptr<WsClient::Connection>,
                          const SimpleWeb::error_code&) {
        cv.notify_all();
    };

    std::thread client_thread([&client]() {
        client.start();
    });

    {
        std::unique_lock<std::mutex> lock(mutex);
        ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(5), [&opened]() {
            return opened;
        }));
    }

    const auto http_response = post_json(
        config,
        bridge.bound_http_port(),
        trade_command("http-duplicate", "idem-concurrent"));

    {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait_for(lock, std::chrono::seconds(5), [&ws_messages]() {
            return !ws_messages.empty();
        });
    }

    client.stop();
    if (client_thread.joinable()) {
        client_thread.join();
    }
    bridge.shutdown();

    ASSERT_EQ(ws_messages.size(), 1u);
    EXPECT_EQ(http_response.at("result").at("status").get<std::string>(), "accepted");
    EXPECT_EQ(ws_messages[0].at("result").at("status").get<std::string>(), "accepted");
    EXPECT_EQ(signal_count.load(), 1);
}

TEST(BridgeProtocolServerBridge, AcceptsWebSocketJsonRpcCommands) {
    namespace proto = optionx::bridges::protocol_v1;

    auto config = test_config();
    config.enable_http = false;

    proto::BridgeProtocolServerBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolServerConfig>(config)));
    bridge.run();
    ASSERT_TRUE(wait_for_ws_port(bridge));

    std::mutex mutex;
    std::condition_variable cv;
    std::vector<nlohmann::json> messages;
    bool opened = false;

    WsClient client(
        config.address + ":" +
        std::to_string(bridge.bound_websocket_port()) +
        config.websocket_path);
    client.config.header.emplace("X-OptionX-Secret", config.secret);
    client.on_open = [&](std::shared_ptr<WsClient::Connection> connection) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            opened = true;
        }
        connection->send(nlohmann::json{
            {"jsonrpc", "2.0"},
            {"id", "ws-hello"},
            {"method", "protocol.hello"},
            {"params", nlohmann::json::object()}
        }.dump(-1));
    };
    client.on_message = [&](std::shared_ptr<WsClient::Connection> connection,
                            std::shared_ptr<WsClient::InMessage> message) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            messages.push_back(nlohmann::json::parse(message->string()));
        }
        connection->send_close(1000);
        cv.notify_all();
    };
    client.on_error = [&](std::shared_ptr<WsClient::Connection>,
                          const SimpleWeb::error_code&) {
        cv.notify_all();
    };

    std::thread client_thread([&client]() {
        client.start();
    });

    {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait_for(lock, std::chrono::seconds(5), [&messages]() {
            return !messages.empty();
        });
    }

    client.stop();
    if (client_thread.joinable()) {
        client_thread.join();
    }
    bridge.shutdown();

    ASSERT_TRUE(opened);
    ASSERT_EQ(messages.size(), 1u);
    EXPECT_EQ(messages[0].at("id").get<std::string>(), "ws-hello");
    EXPECT_EQ(messages[0].at("result").at("selected_protocol_version").get<std::string>(), "1");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
