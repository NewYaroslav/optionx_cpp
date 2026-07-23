#include <gtest/gtest.h>

#define OPTIONX_ENABLE_BRIDGE_PROTOCOL_TEST_HOOKS
#include <optionx_cpp/bridges.hpp>

#include <client_http.hpp>
#include <client_ws.hpp>
#if defined(_WIN32)
#include <SimpleNamedPipe/NamedPipeClient.hpp>
#endif

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
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

optionx::bridges::protocol_v1::BridgeProtocolNamedPipeConfig test_pipe_config() {
    optionx::bridges::protocol_v1::BridgeProtocolNamedPipeConfig config;
    const auto stamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    config.named_pipe = "OptionxBridgeProtocolV1PipeTest_" + std::to_string(stamp);
    config.bridge_id = 4;
    config.request_body_limit = 8192;
    config.dedupe_cache_size = 32;
    return config;
}

bool wait_for_http_port(
        const optionx::bridges::protocol_v1::BridgeProtocolServerBridge& bridge) {
    for (int i = 0; i < 500; ++i) {
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

nlohmann::json classic_duration_trade_command(
        std::string id,
        std::string idempotency_key) {
    auto request = trade_command(std::move(id), std::move(idempotency_key));
    request["params"]["trade"]["option_type"] = "CLASSIC";
    request["params"]["trade"]["expiry"] = {
        {"kind", "duration"},
        {"duration_ms", 900000}
    };
    return request;
}

nlohmann::json signal_command(std::string id, std::string idempotency_key) {
    auto request = trade_command(std::move(id), std::move(idempotency_key));
    request["method"] = "signal.submit";
    request["params"]["signal"] = request["params"]["trade"];
    request["params"].erase("trade");
    return request;
}

nlohmann::json post_json(
        const optionx::bridges::protocol_v1::BridgeProtocolServerConfig& config,
        const unsigned short port,
        const nlohmann::json& request,
        const long timeout_seconds = 0) {
    HttpClient client(config.address + ":" + std::to_string(port));
    client.config.timeout = timeout_seconds;
    client.config.timeout_connect = timeout_seconds;
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

#if defined(_WIN32)
nlohmann::json read_pipe_json(
        SimpleNamedPipe::NamedPipeClient& client,
        const std::chrono::seconds timeout = std::chrono::seconds(3)) {
    std::error_code ec;
    std::string buffered;
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        std::string chunk;
        if (!client.read(chunk, 250, &ec)) {
            continue;
        }
        buffered += chunk;
        const auto newline = buffered.find('\n');
        if (newline == std::string::npos) {
            continue;
        }
        return nlohmann::json::parse(buffered.substr(0, newline));
    }
    ADD_FAILURE() << "Named-pipe JSON frame was not received. buffered=" << buffered;
    throw std::runtime_error("Named-pipe JSON frame was not received.");
}

nlohmann::json pipe_json(
        SimpleNamedPipe::NamedPipeClient& client,
        const nlohmann::json& request) {
    std::error_code ec;
    EXPECT_TRUE(client.write(request.dump(-1), &ec)) << ec.message();
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
            const auto response = buffered.substr(0, newline);
            buffered.erase(0, newline + 1);
            nlohmann::json parsed;
            try {
                parsed = nlohmann::json::parse(response);
            } catch (...) {
                ADD_FAILURE() << "Invalid named-pipe response: " << response;
                throw;
            }
            if (parsed.contains("id") && parsed.at("id") == expected_id) {
                return parsed;
            }
        }
    }
    ADD_FAILURE() << "Buffered named-pipe response without matching JSON-RPC id: " << buffered;
    throw std::runtime_error("Named-pipe JSON-RPC response was not received.");
}
#endif

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
    EXPECT_FALSE(restored.allow_insecure_remote);
    EXPECT_FALSE(restored.allow_cors);
    EXPECT_GT(restored.max_jsonrpc_id_bytes, 0u);
    EXPECT_GT(restored.max_idempotency_key_bytes, 0u);
    EXPECT_GT(restored.max_operation_fingerprint_bytes, 0u);
    EXPECT_GT(restored.max_operation_cache_bytes, 0u);
    EXPECT_GT(restored.operation_cache_retention_ms, 0);
    EXPECT_FALSE(restored.websocket_subprotocol.empty());
    EXPECT_TRUE(restored.require_websocket_subprotocol);
    EXPECT_GT(restored.max_ws_pending_messages, 0u);
    EXPECT_GT(restored.max_ws_pending_bytes, 0u);
    EXPECT_GT(restored.content_timeout_seconds, 0);

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

    auto remote_secret = test_config();
    remote_secret.address = "0.0.0.0";
    EXPECT_FALSE(remote_secret.validate().first);
    remote_secret.allow_insecure_remote = true;
    EXPECT_TRUE(remote_secret.validate().first);

    unsafe.address = "127.0.0.1";
    unsafe.allow_cors = true;
    unsafe.allowed_origin = "*";
    EXPECT_FALSE(unsafe.validate().first);
}

TEST(BridgeProtocolNamedPipeConfig, RoundTripsAndValidates) {
    namespace proto = optionx::bridges::protocol_v1;

    proto::BridgeProtocolNamedPipeConfig config = test_pipe_config();
    nlohmann::json json;
    config.to_json(json);

    proto::BridgeProtocolNamedPipeConfig restored;
    restored.from_json(json);

    EXPECT_TRUE(restored.validate().first);
    EXPECT_EQ(restored.bridge_type(), optionx::BridgeType::BRIDGE_PROTOCOL_V1_NAMED_PIPE);
    EXPECT_EQ(restored.named_pipe, config.named_pipe);
    EXPECT_EQ(restored.bridge_id, 4u);
    EXPECT_GT(restored.buffer_size, 0u);
    EXPECT_GT(restored.pipe_timeout_ms, 0u);
    EXPECT_GT(restored.max_jsonrpc_id_bytes, 0u);
    EXPECT_GT(restored.max_idempotency_key_bytes, 0u);
    EXPECT_GT(restored.max_operation_fingerprint_bytes, 0u);
    EXPECT_GT(restored.max_operation_cache_bytes, 0u);
    EXPECT_GT(restored.operation_cache_retention_ms, 0);

    restored.named_pipe.clear();
    EXPECT_FALSE(restored.validate().first);
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
        optionx::AccountUpdateStatus::BALANCE_UPDATED,
        123));

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
    EXPECT_EQ(balance.at("result").at("account").at("account_id").get<std::string>(), "123");
    EXPECT_EQ(balance.at("result").at("account").at("user_id").get<std::string>(), "99");

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
    EXPECT_EQ(
        retry.at("result").at("operation_id").get<std::string>(),
        accepted.at("result").at("operation_id").get<std::string>());
    EXPECT_TRUE(retry.at("result").contains("trade_refs"));
    EXPECT_FALSE(retry.at("result").contains("signal_ref"));
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

TEST(BridgeProtocolServerBridge, AcceptsClassicTradeOpenWithDurationExpiry) {
    namespace proto = optionx::bridges::protocol_v1;

    auto config = test_config();
    config.enable_websocket = false;

    proto::BridgeProtocolServerBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolServerConfig>(config)));

    optionx::OptionType option_type = optionx::OptionType::UNKNOWN;
    std::uint32_t duration = 0;
    std::int64_t expiry_time = -1;
    std::atomic<int> signal_count{0};

    bridge.on_signal_id() = []() { return optionx::SignalId{11}; };
    bridge.on_trade_signal() =
        [&](std::unique_ptr<optionx::TradeSignal> signal) {
            ASSERT_TRUE(signal);
            option_type = signal->option_type;
            duration = signal->duration;
            expiry_time = signal->expiry_time;
            ++signal_count;
        };

    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));

    const auto accepted = post_json(
        config,
        bridge.bound_http_port(),
        classic_duration_trade_command("classic-duration", "idem-classic-duration"));
    bridge.shutdown();

    EXPECT_EQ(accepted.at("result").at("status").get<std::string>(), "accepted");
    EXPECT_EQ(signal_count.load(), 1);
    EXPECT_EQ(option_type, optionx::OptionType::CLASSIC);
    EXPECT_EQ(duration, 900u);
    EXPECT_EQ(expiry_time, 0);
}

TEST(BridgeProtocolServerBridge, OmitsUnspecifiedAccountAndUnknownUserIds) {
    namespace proto = optionx::bridges::protocol_v1;

    auto config = test_config();
    config.enable_websocket = false;

    proto::BridgeProtocolServerBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolServerConfig>(config)));

    auto account = std::make_shared<TestAccountInfo>();
    account->user_id = 0;
    bridge.update_account_info(optionx::AccountInfoUpdate(
        account,
        optionx::AccountUpdateStatus::BALANCE_UPDATED));

    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));

    const auto balance = post_json(
        config,
        bridge.bound_http_port(),
        nlohmann::json{
            {"jsonrpc", "2.0"},
            {"id", "balance-no-ids"},
            {"method", "account.balance.get"},
            {"params", nlohmann::json::object()}
        });
    bridge.shutdown();

    const auto& account_json = balance.at("result").at("account");
    EXPECT_FALSE(account_json.contains("account_id")) << account_json.dump(-1);
    EXPECT_FALSE(account_json.contains("user_id")) << account_json.dump(-1);
}

TEST(BridgeProtocolNamedPipeBridge, AcceptsJsonRpcCommands) {
    namespace proto = optionx::bridges::protocol_v1;

#if defined(_WIN32)
    auto config = test_pipe_config();
    proto::BridgeProtocolNamedPipeBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolNamedPipeConfig>(config)));

    std::mutex mutex;
    std::condition_variable cv;
    bool server_started = false;
    std::string status_error;
    std::atomic<optionx::SignalId> next_signal_id{50};
    std::atomic<int> signal_count{0};

    bridge.on_status_update() = [&](const optionx::BridgeStatusUpdate& update) {
        std::lock_guard<std::mutex> lock(mutex);
        if (update.status == optionx::BridgeStatus::SERVER_STARTED) {
            server_started = true;
        }
        if (update.status == optionx::BridgeStatus::SERVER_START_FAILED ||
            update.status == optionx::BridgeStatus::CONNECTION_ERROR) {
            status_error = update.message;
        }
        cv.notify_all();
    };
    bridge.on_signal_id() = [&next_signal_id]() {
        return next_signal_id.fetch_add(1);
    };
    bridge.on_trade_signal() = [&signal_count](std::unique_ptr<optionx::TradeSignal> signal) {
        ASSERT_EQ(signal->symbol, "EURUSD");
        ++signal_count;
    };
    bridge.update_account_info(optionx::AccountInfoUpdate(
        std::make_shared<TestAccountInfo>(),
        optionx::AccountUpdateStatus::BALANCE_UPDATED,
        123));

    bridge.run();
    {
        std::unique_lock<std::mutex> lock(mutex);
        ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(3), [&] {
            return server_started || !status_error.empty();
        })) << "Named-pipe protocol bridge did not start.";
        ASSERT_TRUE(status_error.empty()) << status_error;
    }

    SimpleNamedPipe::ClientConfig client_config(
        config.named_pipe,
        config.buffer_size,
        3000);
    SimpleNamedPipe::NamedPipeClient client(client_config);
    std::error_code ec;
    ASSERT_TRUE(client.connect(&ec)) << ec.message();

    const auto hello = pipe_json(
        client,
        nlohmann::json{
            {"jsonrpc", "2.0"},
            {"id", "pipe-hello"},
            {"method", "protocol.hello"},
            {"params", nlohmann::json::object()}
        });
    EXPECT_EQ(hello.at("result").at("selected_protocol_version").get<std::string>(), "1");

    const auto capabilities = pipe_json(
        client,
        nlohmann::json{
            {"jsonrpc", "2.0"},
            {"id", "pipe-capabilities"},
            {"method", "protocol.capabilities.get"},
            {"params", nlohmann::json::object()}
        });
    EXPECT_TRUE(capabilities.at("result").at("features").at("named_pipe").get<bool>());
    EXPECT_FALSE(capabilities.at("result").at("features").at("http").get<bool>());
    EXPECT_FALSE(capabilities.at("result").at("features").at("websocket").get<bool>());

    const auto balance = pipe_json(
        client,
        nlohmann::json{
            {"jsonrpc", "2.0"},
            {"id", "pipe-balance"},
            {"method", "account.balance.get"},
            {"params", nlohmann::json::object()}
        });
    EXPECT_EQ(balance.at("result").at("status").get<std::string>(), "completed");
    EXPECT_EQ(balance.at("result").at("account").at("account_id").get<std::string>(), "123");
    EXPECT_EQ(balance.at("result").at("account").at("user_id").get<std::string>(), "99");

    const auto accepted = pipe_json(client, trade_command("pipe-trade", "pipe-idem"));
    EXPECT_EQ(accepted.at("result").at("status").get<std::string>(), "accepted");
    EXPECT_EQ(signal_count.load(), 1);

    optionx::OptionType last_option_type = optionx::OptionType::UNKNOWN;
    std::uint32_t last_duration = 0;
    std::int64_t last_expiry_time = -1;
    bridge.on_trade_signal() =
        [&](std::unique_ptr<optionx::TradeSignal> signal) {
            ASSERT_TRUE(signal);
            last_option_type = signal->option_type;
            last_duration = signal->duration;
            last_expiry_time = signal->expiry_time;
            ++signal_count;
        };

    const auto classic_duration = pipe_json(
        client,
        classic_duration_trade_command("pipe-classic-duration", "pipe-classic-duration"));
    EXPECT_EQ(classic_duration.at("result").at("status").get<std::string>(), "accepted");
    EXPECT_EQ(signal_count.load(), 2);
    EXPECT_EQ(last_option_type, optionx::OptionType::CLASSIC);
    EXPECT_EQ(last_duration, 900u);
    EXPECT_EQ(last_expiry_time, 0);

    const auto retry = pipe_json(client, trade_command("pipe-trade-retry", "pipe-idem"));
    EXPECT_EQ(
        retry.at("result").at("operation_id").get<std::string>(),
        accepted.at("result").at("operation_id").get<std::string>());
    EXPECT_EQ(signal_count.load(), 2);

    const auto conflict = pipe_json(
        client,
        trade_command("pipe-trade-conflict", "pipe-idem", "GBPUSD"));
    EXPECT_EQ(conflict.at("result").at("status").get<std::string>(), "rejected");
    EXPECT_EQ(
        conflict.at("result").at("reason").at("code").get<std::string>(),
        "idempotency_conflict");

    client.close();
    bridge.shutdown();
#else
    GTEST_SKIP() << "Bridge Protocol v1 named-pipe transport is Windows-only.";
#endif
}

TEST(BridgeProtocolNamedPipeBridge, HandlesFramesLargerThanTransportBuffer) {
    namespace proto = optionx::bridges::protocol_v1;

#if defined(_WIN32)
    auto config = test_pipe_config();
    config.buffer_size = 64;
    config.request_body_limit = 64 * 1024;

    proto::BridgeProtocolNamedPipeBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolNamedPipeConfig>(config)));

    std::mutex mutex;
    std::condition_variable cv;
    bool server_started = false;
    std::string status_error;

    bridge.on_status_update() = [&](const optionx::BridgeStatusUpdate& update) {
        std::lock_guard<std::mutex> lock(mutex);
        if (update.status == optionx::BridgeStatus::SERVER_STARTED) {
            server_started = true;
        }
        if (update.status == optionx::BridgeStatus::SERVER_START_FAILED ||
            update.status == optionx::BridgeStatus::CONNECTION_ERROR) {
            status_error = update.message;
        }
        cv.notify_all();
    };
    bridge.on_signal_id() = []() { return optionx::SignalId{75}; };
    bridge.on_trade_signal() = [](std::unique_ptr<optionx::TradeSignal>) {};

    bridge.run();
    {
        std::unique_lock<std::mutex> lock(mutex);
        ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(3), [&] {
            return server_started || !status_error.empty();
        }));
        ASSERT_TRUE(status_error.empty()) << status_error;
    }

    SimpleNamedPipe::NamedPipeClient client(
        SimpleNamedPipe::ClientConfig(config.named_pipe, 37, 3000));
    std::error_code ec;
    ASSERT_TRUE(client.connect(&ec)) << ec.message();

    auto large_hello = nlohmann::json{
        {"jsonrpc", "2.0"},
        {"id", "pipe-large-hello"},
        {"method", "protocol.hello"},
        {"params", {
            {"requested_protocol_versions", nlohmann::json::array({"1"})},
            {"padding", std::string(2048, 'p')}
        }}
    };
    ASSERT_GT(large_hello.dump(-1).size(), config.buffer_size);
    const auto hello = pipe_json(client, large_hello);
    EXPECT_EQ(hello.at("result").at("selected_protocol_version").get<std::string>(), "1");
    EXPECT_GT(hello.dump(-1).size(), config.buffer_size);

    bridge.update_account_info(optionx::AccountInfoUpdate(
        std::make_shared<TestAccountInfo>(),
        optionx::AccountUpdateStatus::BALANCE_UPDATED,
        321));
    const auto notification = read_pipe_json(client);
    ASSERT_TRUE(notification.contains("method")) << notification.dump(-1);
    EXPECT_EQ(notification.at("method").get<std::string>(), "balance.updated");
    EXPECT_EQ(
        notification.at("params").at("payload").at("account_id").get<std::string>(),
        "321");
    EXPECT_EQ(
        notification.at("params").at("payload").at("user_id").get<std::string>(),
        "99");
    EXPECT_GT(notification.dump(-1).size(), config.buffer_size);

    auto account_without_ids = std::make_shared<TestAccountInfo>();
    account_without_ids->user_id = 0;
    bridge.update_account_info(optionx::AccountInfoUpdate(
        account_without_ids,
        optionx::AccountUpdateStatus::BALANCE_UPDATED));
    const auto anonymous_notification = read_pipe_json(client);
    ASSERT_TRUE(anonymous_notification.contains("method")) << anonymous_notification.dump(-1);
    EXPECT_EQ(anonymous_notification.at("method").get<std::string>(), "balance.updated");
    EXPECT_FALSE(anonymous_notification.at("params").at("subject").contains("account_id"));
    EXPECT_FALSE(anonymous_notification.at("params").at("subject").contains("user_id"));
    EXPECT_FALSE(anonymous_notification.at("params").at("payload").contains("account_id"));
    EXPECT_FALSE(anonymous_notification.at("params").at("payload").contains("user_id"));

    client.close();
    bridge.shutdown();
#else
    GTEST_SKIP() << "Bridge Protocol v1 named-pipe transport is Windows-only.";
#endif
}

TEST(BridgeProtocolNamedPipeBridge, AsyncStartupFailureReturnsToStoppedAndAllowsRestart) {
    namespace proto = optionx::bridges::protocol_v1;

#if defined(_WIN32)
    auto bad_config = test_pipe_config();
    bad_config.named_pipe = std::string(300, 'x');

    proto::BridgeProtocolNamedPipeBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolNamedPipeConfig>(bad_config)));

    std::mutex mutex;
    std::condition_variable cv;
    bool start_failed = false;
    bool server_started = false;
    std::string status_error;

    bridge.on_status_update() = [&](const optionx::BridgeStatusUpdate& update) {
        std::lock_guard<std::mutex> lock(mutex);
        if (update.status == optionx::BridgeStatus::SERVER_START_FAILED) {
            start_failed = true;
            status_error = update.message;
        }
        if (update.status == optionx::BridgeStatus::SERVER_STARTED) {
            server_started = true;
        }
        cv.notify_all();
    };

    bridge.run();
    {
        std::unique_lock<std::mutex> lock(mutex);
        ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(5), [&] {
            return start_failed;
        })) << "Expected async named-pipe startup failure.";
    }
    EXPECT_FALSE(status_error.empty());

    auto good_config = test_pipe_config();
    ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolNamedPipeConfig>(good_config)));
    bridge.run();
    {
        std::unique_lock<std::mutex> lock(mutex);
        ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(3), [&] {
            return server_started;
        })) << "Bridge did not restart after async startup failure.";
    }

    SimpleNamedPipe::NamedPipeClient client(
        SimpleNamedPipe::ClientConfig(good_config.named_pipe, good_config.buffer_size, 3000));
    std::error_code ec;
    ASSERT_TRUE(client.connect(&ec)) << ec.message();
    const auto hello = pipe_json(
        client,
        nlohmann::json{
            {"jsonrpc", "2.0"},
            {"id", "pipe-restart-hello"},
            {"method", "protocol.hello"},
            {"params", nlohmann::json::object()}
        });
    EXPECT_EQ(hello.at("result").at("selected_protocol_version").get<std::string>(), "1");

    client.close();
    bridge.shutdown();
#else
    GTEST_SKIP() << "Bridge Protocol v1 named-pipe transport is Windows-only.";
#endif
}

TEST(BridgeProtocolNamedPipeBridge, ShutdownFromServerStartedAllowsRestart) {
    namespace proto = optionx::bridges::protocol_v1;

#if defined(_WIN32)
    auto config = test_pipe_config();
    proto::BridgeProtocolNamedPipeBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolNamedPipeConfig>(config)));

    std::mutex mutex;
    std::condition_variable cv;
    int started_count = 0;
    bool stopped = false;

    bridge.on_status_update() = [&](const optionx::BridgeStatusUpdate& update) {
        if (update.status == optionx::BridgeStatus::SERVER_STARTED) {
            bool request_shutdown = false;
            {
                std::lock_guard<std::mutex> lock(mutex);
                ++started_count;
                request_shutdown = started_count == 1;
            }
            cv.notify_all();
            if (request_shutdown) {
                bridge.shutdown();
            }
            return;
        }
        if (update.status == optionx::BridgeStatus::SERVER_STOPPED) {
            std::lock_guard<std::mutex> lock(mutex);
            stopped = true;
            cv.notify_all();
        }
    };

    bridge.run();
    {
        std::unique_lock<std::mutex> lock(mutex);
        ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(5), [&] {
            return stopped;
        }));
    }

    bridge.run();
    {
        std::unique_lock<std::mutex> lock(mutex);
        ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(3), [&] {
            return started_count >= 2;
        }));
    }

    SimpleNamedPipe::NamedPipeClient client(
        SimpleNamedPipe::ClientConfig(config.named_pipe, config.buffer_size, 3000));
    std::error_code ec;
    ASSERT_TRUE(client.connect(&ec)) << ec.message();
    const auto hello = pipe_json(
        client,
        nlohmann::json{
            {"jsonrpc", "2.0"},
            {"id", "pipe-restarted-after-status-shutdown"},
            {"method", "protocol.hello"},
            {"params", nlohmann::json::object()}
        });
    EXPECT_EQ(hello.at("result").at("selected_protocol_version").get<std::string>(), "1");

    client.close();
    bridge.shutdown();
#else
    GTEST_SKIP() << "Bridge Protocol v1 named-pipe transport is Windows-only.";
#endif
}

TEST(BridgeProtocolNamedPipeBridge, UnexpectedServerStopAllowsRestart) {
    namespace proto = optionx::bridges::protocol_v1;

#if defined(_WIN32)
    auto config = test_pipe_config();
    proto::BridgeProtocolNamedPipeBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolNamedPipeConfig>(config)));

    std::mutex mutex;
    std::condition_variable cv;
    int started_count = 0;
    int stopped_count = 0;

    bridge.on_status_update() = [&](const optionx::BridgeStatusUpdate& update) {
        std::lock_guard<std::mutex> lock(mutex);
        if (update.status == optionx::BridgeStatus::SERVER_STARTED) {
            ++started_count;
        }
        if (update.status == optionx::BridgeStatus::SERVER_STOPPED) {
            ++stopped_count;
        }
        cv.notify_all();
    };

    bridge.run();
    {
        std::unique_lock<std::mutex> lock(mutex);
        ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(3), [&] {
            return started_count == 1;
        }));
    }

    bridge.simulate_unexpected_server_stop_for_test();
    {
        std::unique_lock<std::mutex> lock(mutex);
        ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(5), [&] {
            return stopped_count == 1;
        }));
    }

    bridge.run();
    {
        std::unique_lock<std::mutex> lock(mutex);
        ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(3), [&] {
            return started_count == 2;
        }));
    }

    SimpleNamedPipe::NamedPipeClient client(
        SimpleNamedPipe::ClientConfig(config.named_pipe, config.buffer_size, 3000));
    std::error_code ec;
    ASSERT_TRUE(client.connect(&ec)) << ec.message();
    const auto hello = pipe_json(
        client,
        nlohmann::json{
            {"jsonrpc", "2.0"},
            {"id", "pipe-restarted-after-unexpected-stop"},
            {"method", "protocol.hello"},
            {"params", nlohmann::json::object()}
        });
    EXPECT_EQ(hello.at("result").at("selected_protocol_version").get<std::string>(), "1");

    client.close();
    bridge.shutdown();
#else
    GTEST_SKIP() << "Bridge Protocol v1 named-pipe transport is Windows-only.";
#endif
}

TEST(BridgeProtocolNamedPipeBridge, ServerStoppedCallbackCanRestart) {
    namespace proto = optionx::bridges::protocol_v1;

#if defined(_WIN32)
    auto config = test_pipe_config();
    proto::BridgeProtocolNamedPipeBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolNamedPipeConfig>(config)));

    std::mutex mutex;
    std::condition_variable cv;
    int started_count = 0;
    bool restart_requested = false;

    bridge.on_status_update() = [&](const optionx::BridgeStatusUpdate& update) {
        bool should_restart = false;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (update.status == optionx::BridgeStatus::SERVER_STARTED) {
                ++started_count;
            }
            if (update.status == optionx::BridgeStatus::SERVER_STOPPED &&
                !restart_requested) {
                restart_requested = true;
                should_restart = true;
            }
        }
        cv.notify_all();
        if (should_restart) {
            bridge.run();
            cv.notify_all();
        }
    };

    bridge.run();
    {
        std::unique_lock<std::mutex> lock(mutex);
        ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(3), [&] {
            return started_count == 1;
        }));
    }

    bridge.shutdown();
    {
        std::unique_lock<std::mutex> lock(mutex);
        ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(5), [&] {
            return started_count == 2;
        }));
    }

    SimpleNamedPipe::NamedPipeClient client(
        SimpleNamedPipe::ClientConfig(config.named_pipe, config.buffer_size, 3000));
    std::error_code ec;
    ASSERT_TRUE(client.connect(&ec)) << ec.message();
    const auto hello = pipe_json(
        client,
        nlohmann::json{
            {"jsonrpc", "2.0"},
            {"id", "pipe-restarted-from-stopped-callback"},
            {"method", "protocol.hello"},
            {"params", nlohmann::json::object()}
        });
    EXPECT_EQ(hello.at("result").at("selected_protocol_version").get<std::string>(), "1");

    client.close();
    bridge.shutdown();
#else
    GTEST_SKIP() << "Bridge Protocol v1 named-pipe transport is Windows-only.";
#endif
}

TEST(BridgeProtocolServerBridge, ReturnsMethodSpecificAcceptedShapes) {
    namespace proto = optionx::bridges::protocol_v1;

    auto config = test_config();
    config.enable_websocket = false;

    proto::BridgeProtocolServerBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolServerConfig>(config)));

    std::atomic<optionx::SignalId> next_signal_id{100};
    bridge.on_signal_id() = [&next_signal_id]() {
        return next_signal_id.fetch_add(1);
    };
    bridge.on_trade_signal() = [](std::unique_ptr<optionx::TradeSignal>) {};

    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));

    const auto signal = post_json(
        config,
        bridge.bound_http_port(),
        signal_command("shape-signal", "shape-signal-key"));
    const auto trade = post_json(
        config,
        bridge.bound_http_port(),
        trade_command("shape-trade", "shape-trade-key"));
    bridge.shutdown();

    ASSERT_EQ(signal.at("result").at("status").get<std::string>(), "accepted");
    EXPECT_TRUE(signal.at("result").contains("signal_ref"));
    EXPECT_TRUE(signal.at("result").contains("trade_refs"));

    ASSERT_EQ(trade.at("result").at("status").get<std::string>(), "accepted");
    EXPECT_FALSE(trade.at("result").contains("signal_ref"));
    EXPECT_TRUE(trade.at("result").contains("trade_refs"));
    ASSERT_FALSE(trade.at("result").at("trade_refs").empty());
    EXPECT_EQ(
        trade.at("result").at("trade_refs").at(0).at("signal_id").get<std::string>(),
        "101");
}

TEST(BridgeProtocolServerBridge, RejectsInvalidEnvelopeAndUnsupportedHelloVersion) {
    namespace proto = optionx::bridges::protocol_v1;

    auto config = test_config();
    config.enable_websocket = false;

    proto::BridgeProtocolServerBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolServerConfig>(config)));
    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));

    const auto bad_jsonrpc = post_json(
        config,
        bridge.bound_http_port(),
        nlohmann::json{
            {"jsonrpc", 2},
            {"id", "bad-jsonrpc"},
            {"method", "protocol.hello"},
            {"params", nlohmann::json::object()}
        });
    const auto bad_id = post_json(
        config,
        bridge.bound_http_port(),
        nlohmann::json{
            {"jsonrpc", "2.0"},
            {"id", nlohmann::json::array({"not", "valid"})},
            {"method", "protocol.hello"},
            {"params", nlohmann::json::object()}
        });
    const auto bad_params = post_json(
        config,
        bridge.bound_http_port(),
        nlohmann::json{
            {"jsonrpc", "2.0"},
            {"id", "bad-params"},
            {"method", "protocol.hello"},
            {"params", nlohmann::json::array()}
        });
    const auto unsupported_hello = post_json(
        config,
        bridge.bound_http_port(),
        nlohmann::json{
            {"jsonrpc", "2.0"},
            {"id", "hello-v2"},
            {"method", "protocol.hello"},
            {"params", {
                {"requested_protocol_versions", nlohmann::json::array({"2"})}
            }}
        });
    bridge.shutdown();

    EXPECT_EQ(bad_jsonrpc.at("error").at("code").get<int>(), -32600);
    EXPECT_EQ(bad_id.at("error").at("code").get<int>(), -32600);
    EXPECT_EQ(bad_params.at("error").at("code").get<int>(), -32602);
    EXPECT_EQ(unsupported_hello.at("error").at("code").get<int>(), -32010);
    EXPECT_EQ(
        unsupported_hello.at("error").at("data").at("code").get<std::string>(),
        "unsupported_protocol_version");
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

TEST(BridgeProtocolServerBridge, RejectsUnknownAndNonFiniteTradeFields) {
    namespace proto = optionx::bridges::protocol_v1;

    auto config = test_config();
    config.enable_websocket = false;

    proto::BridgeProtocolServerBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolServerConfig>(config)));
    bridge.on_signal_id() = []() { return optionx::SignalId{35}; };
    bridge.on_trade_signal() = [](std::unique_ptr<optionx::TradeSignal>) {};

    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));

    auto typo = trade_command("trade-typo", "idem-typo");
    typo["params"]["trade"]["min_payuot"] = "0.80";
    const auto typo_response = post_json(config, bridge.bound_http_port(), typo);

    auto nan_amount = trade_command("trade-nan", "idem-nan");
    nan_amount["params"]["trade"]["amount"]["value"] = "NaN";
    const auto nan_response = post_json(config, bridge.bound_http_port(), nan_amount);

    auto partial_amount = trade_command("trade-partial", "idem-partial");
    partial_amount["params"]["trade"]["amount"]["value"] = "1.00junk";
    const auto partial_response = post_json(config, bridge.bound_http_port(), partial_amount);

    auto huge_duration = trade_command("trade-huge-duration", "idem-huge-duration");
    huge_duration["params"]["trade"]["expiry"]["duration_ms"] =
        std::to_string(
            static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()) *
            1000ull +
            1000ull);
    const auto huge_duration_response = post_json(config, bridge.bound_http_port(), huge_duration);
    bridge.shutdown();

    EXPECT_EQ(typo_response.at("error").at("code").get<int>(), -32602);
    EXPECT_EQ(nan_response.at("error").at("code").get<int>(), -32602);
    EXPECT_EQ(partial_response.at("error").at("code").get<int>(), -32602);
    EXPECT_EQ(huge_duration_response.at("error").at("code").get<int>(), -32602);
}

TEST(BridgeProtocolServerBridge, RejectsInvalidDirectTradeExpiry) {
    namespace proto = optionx::bridges::protocol_v1;

    auto config = test_config();
    config.enable_websocket = false;

    proto::BridgeProtocolServerBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolServerConfig>(config)));
    bridge.on_signal_id() = []() { return optionx::SignalId{36}; };
    bridge.on_trade_signal() = [](std::unique_ptr<optionx::TradeSignal>) {};

    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));

    auto missing_option_type = trade_command("missing-option-type", "idem-missing-option");
    missing_option_type["params"]["trade"].erase("option_type");
    const auto missing_option_response =
        post_json(config, bridge.bound_http_port(), missing_option_type);

    auto missing_expiry = trade_command("missing-expiry", "idem-missing-expiry");
    missing_expiry["params"]["trade"].erase("expiry");
    const auto missing_expiry_response =
        post_json(config, bridge.bound_http_port(), missing_expiry);

    auto zero_duration = trade_command("zero-duration", "idem-zero-duration");
    zero_duration["params"]["trade"]["expiry"]["duration_ms"] = 0;
    const auto zero_duration_response =
        post_json(config, bridge.bound_http_port(), zero_duration);

    auto subsecond_duration = trade_command("subsecond-duration", "idem-subsecond-duration");
    subsecond_duration["params"]["trade"]["expiry"]["duration_ms"] = 500;
    const auto subsecond_duration_response =
        post_json(config, bridge.bound_http_port(), subsecond_duration);

    auto past_absolute = trade_command("past-absolute", "idem-past-absolute");
    past_absolute["params"]["trade"]["expiry"] = {
        {"kind", "absolute"},
        {"expires_at_ms", 1000}
    };
    const auto past_absolute_response =
        post_json(config, bridge.bound_http_port(), past_absolute);

    auto conflicting_expiry = trade_command("conflicting-expiry", "idem-conflicting-expiry");
    conflicting_expiry["params"]["trade"]["expiry"]["expires_at_ms"] =
        optionx::bridges::metatrader_file::detail::unix_time_ms() + 60000;
    const auto conflicting_expiry_response =
        post_json(config, bridge.bound_http_port(), conflicting_expiry);

    auto mismatched_kind = trade_command("mismatched-kind", "idem-mismatched-kind");
    mismatched_kind["params"]["trade"]["expiry"] = {
        {"kind", "absolute"},
        {"duration_ms", 60000}
    };
    const auto mismatched_kind_response =
        post_json(config, bridge.bound_http_port(), mismatched_kind);

    auto ignored_top_level_alias =
        trade_command("ignored-top-level-alias", "idem-ignored-top-level-alias");
    ignored_top_level_alias["params"]["trade"]["duration_ms"] = 0;
    const auto ignored_top_level_alias_response =
        post_json(config, bridge.bound_http_port(), ignored_top_level_alias);

    auto duration_and_duration_sec =
        trade_command("duration-and-duration-sec", "idem-duration-and-duration-sec");
    duration_and_duration_sec["params"]["trade"].erase("expiry");
    duration_and_duration_sec["params"]["trade"]["duration"] = 60;
    duration_and_duration_sec["params"]["trade"]["duration_sec"] = 120;
    const auto duration_and_duration_sec_response =
        post_json(config, bridge.bound_http_port(), duration_and_duration_sec);

    auto duration_ms_and_duration =
        trade_command("duration-ms-and-duration", "idem-duration-ms-and-duration");
    duration_ms_and_duration["params"]["trade"].erase("expiry");
    duration_ms_and_duration["params"]["trade"]["duration_ms"] = 60000;
    duration_ms_and_duration["params"]["trade"]["duration"] = 60;
    const auto duration_ms_and_duration_response =
        post_json(config, bridge.bound_http_port(), duration_ms_and_duration);

    auto expires_at_ms_and_expiry_time =
        trade_command("expires-at-ms-and-expiry-time", "idem-expires-at-ms-and-expiry-time");
    expires_at_ms_and_expiry_time["params"]["trade"].erase("expiry");
    expires_at_ms_and_expiry_time["params"]["trade"]["expires_at_ms"] =
        optionx::bridges::metatrader_file::detail::unix_time_ms() + 60000;
    expires_at_ms_and_expiry_time["params"]["trade"]["expiry_time"] =
        optionx::bridges::metatrader_file::detail::unix_time_ms() / 1000 + 120;
    const auto expires_at_ms_and_expiry_time_response =
        post_json(config, bridge.bound_http_port(), expires_at_ms_and_expiry_time);

    auto expiry_duration_ms_and_duration_sec =
        trade_command("expiry-duration-ms-and-duration-sec",
                      "idem-expiry-duration-ms-and-duration-sec");
    expiry_duration_ms_and_duration_sec["params"]["trade"]["duration_sec"] = 120;
    const auto expiry_duration_ms_and_duration_sec_response =
        post_json(config, bridge.bound_http_port(), expiry_duration_ms_and_duration_sec);

    auto expiry_expires_at_ms_and_expiry_time =
        trade_command("expiry-expires-at-ms-and-expiry-time",
                      "idem-expiry-expires-at-ms-and-expiry-time");
    expiry_expires_at_ms_and_expiry_time["params"]["trade"]["expiry"] = {
        {"kind", "absolute"},
        {"expires_at_ms", optionx::bridges::metatrader_file::detail::unix_time_ms() + 60000}
    };
    expiry_expires_at_ms_and_expiry_time["params"]["trade"]["expiry_time"] =
        optionx::bridges::metatrader_file::detail::unix_time_ms() / 1000 + 120;
    const auto expiry_expires_at_ms_and_expiry_time_response =
        post_json(config, bridge.bound_http_port(), expiry_expires_at_ms_and_expiry_time);

    auto unsupported_kind_and_duration_sec =
        trade_command("unsupported-kind-and-duration-sec",
                      "idem-unsupported-kind-and-duration-sec");
    unsupported_kind_and_duration_sec["params"]["trade"]["expiry"] = {
        {"kind", "weekly"}
    };
    unsupported_kind_and_duration_sec["params"]["trade"]["duration_sec"] = 60;
    const auto unsupported_kind_and_duration_sec_response =
        post_json(config, bridge.bound_http_port(), unsupported_kind_and_duration_sec);

    auto unsupported_kind_and_expiry_time =
        trade_command("unsupported-kind-and-expiry-time",
                      "idem-unsupported-kind-and-expiry-time");
    unsupported_kind_and_expiry_time["params"]["trade"]["expiry"] = {
        {"kind", "weekly"}
    };
    unsupported_kind_and_expiry_time["params"]["trade"]["expiry_time"] =
        optionx::bridges::metatrader_file::detail::unix_time_ms() / 1000 + 120;
    const auto unsupported_kind_and_expiry_time_response =
        post_json(config, bridge.bound_http_port(), unsupported_kind_and_expiry_time);
    bridge.shutdown();

    EXPECT_EQ(missing_option_response.at("error").at("code").get<int>(), -32602);
    EXPECT_EQ(missing_expiry_response.at("error").at("code").get<int>(), -32602);
    EXPECT_EQ(zero_duration_response.at("error").at("code").get<int>(), -32602);
    EXPECT_EQ(subsecond_duration_response.at("error").at("code").get<int>(), -32602);
    EXPECT_EQ(past_absolute_response.at("error").at("code").get<int>(), -32602);
    EXPECT_EQ(conflicting_expiry_response.at("error").at("code").get<int>(), -32602);
    EXPECT_EQ(mismatched_kind_response.at("error").at("code").get<int>(), -32602);
    EXPECT_EQ(ignored_top_level_alias_response.at("error").at("code").get<int>(), -32602);
    EXPECT_EQ(duration_and_duration_sec_response.at("error").at("code").get<int>(), -32602);
    EXPECT_EQ(duration_ms_and_duration_response.at("error").at("code").get<int>(), -32602);
    EXPECT_EQ(expires_at_ms_and_expiry_time_response.at("error").at("code").get<int>(), -32602);
    EXPECT_EQ(
        expiry_duration_ms_and_duration_sec_response.at("error").at("code").get<int>(),
        -32602);
    EXPECT_EQ(
        expiry_expires_at_ms_and_expiry_time_response.at("error").at("code").get<int>(),
        -32602);
    EXPECT_EQ(
        unsupported_kind_and_duration_sec_response.at("error").at("code").get<int>(),
        -32602);
    EXPECT_EQ(
        unsupported_kind_and_expiry_time_response.at("error").at("code").get<int>(),
        -32602);
}

TEST(BridgeProtocolServerBridge, AcceptsEmptyExpiryObjectWithSingleTopLevelAlias) {
    namespace proto = optionx::bridges::protocol_v1;

    auto config = test_config();
    config.enable_websocket = false;

    proto::BridgeProtocolServerBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolServerConfig>(config)));

    std::vector<std::uint32_t> durations;
    std::vector<std::int64_t> expiry_times;

    bridge.on_signal_id() = []() { return optionx::SignalId{38}; };
    bridge.on_trade_signal() =
        [&](std::unique_ptr<optionx::TradeSignal> signal) {
            durations.push_back(signal->duration);
            expiry_times.push_back(signal->expiry_time);
        };

    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));

    auto duration_alias =
        trade_command("empty-expiry-duration", "idem-empty-expiry-duration");
    duration_alias["params"]["trade"]["expiry"] = nlohmann::json::object();
    duration_alias["params"]["trade"]["duration_sec"] = 60;
    const auto duration_response =
        post_json(config, bridge.bound_http_port(), duration_alias);

    const auto expiry_seconds =
        optionx::bridges::metatrader_file::detail::unix_time_ms() / 1000 + 120;
    auto absolute_alias =
        trade_command("empty-expiry-absolute", "idem-empty-expiry-absolute");
    absolute_alias["params"]["trade"]["expiry"] = nlohmann::json::object();
    absolute_alias["params"]["trade"]["expiry_time"] = expiry_seconds;
    const auto absolute_response =
        post_json(config, bridge.bound_http_port(), absolute_alias);
    bridge.shutdown();

    EXPECT_EQ(duration_response.at("result").at("status").get<std::string>(), "accepted");
    EXPECT_EQ(absolute_response.at("result").at("status").get<std::string>(), "accepted");

    ASSERT_EQ(durations.size(), 2u);
    ASSERT_EQ(expiry_times.size(), 2u);
    EXPECT_EQ(durations[0], 60u);
    EXPECT_EQ(expiry_times[0], 0);
    EXPECT_EQ(expiry_times[1], expiry_seconds);
}

TEST(BridgeProtocolServerBridge, AcceptsDocumentedSizingAndRoutingPlatformType) {
    namespace proto = optionx::bridges::protocol_v1;

    auto config = test_config();
    config.enable_websocket = false;

    proto::BridgeProtocolServerBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolServerConfig>(config)));

    optionx::PlatformType platform_type = optionx::PlatformType::UNKNOWN;
    optionx::MmSystemType mm_type = optionx::MmSystemType::NONE;
    std::int32_t mm_step = 0;
    std::int64_t mm_group_id = 0;
    std::string mm_group_hash;
    std::string mm_group_name;

    bridge.on_signal_id() = []() { return optionx::SignalId{37}; };
    bridge.on_trade_signal() =
        [&](std::unique_ptr<optionx::TradeSignal> signal) {
            platform_type = signal->platform_type;
            mm_type = signal->mm_type;
            mm_step = signal->mm_step;
            mm_group_id = signal->mm_group_id;
            mm_group_hash = signal->mm_group_hash;
            mm_group_name = signal->mm_group_name;
        };

    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));

    auto command = trade_command("sizing-routing", "idem-sizing-routing");
    command["params"]["routing"]["platform_type"] = "INTRADE_BAR";
    command["params"]["sizing"] = {
        {"mode", "FIXED"},
        {"step", 2},
        {"group_id", 77},
        {"group_hash", "mm-hash"},
        {"group_name", "group-a"}
    };
    const auto response = post_json(config, bridge.bound_http_port(), command);
    bridge.shutdown();

    EXPECT_EQ(response.at("result").at("status").get<std::string>(), "accepted");
    EXPECT_EQ(platform_type, optionx::PlatformType::INTRADE_BAR);
    EXPECT_EQ(mm_type, optionx::MmSystemType::FIXED);
    EXPECT_EQ(mm_step, 2);
    EXPECT_EQ(mm_group_id, 77);
    EXPECT_EQ(mm_group_hash, "mm-hash");
    EXPECT_EQ(mm_group_name, "group-a");
}

TEST(BridgeProtocolServerBridge, RejectsInvalidBusinessDtoValues) {
    namespace proto = optionx::bridges::protocol_v1;

    auto config = test_config();
    config.enable_websocket = false;

    proto::BridgeProtocolServerBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolServerConfig>(config)));
    bridge.on_signal_id() = []() { return optionx::SignalId{39}; };
    bridge.on_trade_signal() = [](std::unique_ptr<optionx::TradeSignal>) {};

    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));

    auto negative_signal_amount =
        signal_command("negative-signal-amount", "idem-negative-signal-amount");
    negative_signal_amount["params"]["signal"]["amount"]["value"] = "-1.00";
    const auto negative_signal_amount_response =
        post_json(config, bridge.bound_http_port(), negative_signal_amount);

    auto invalid_min_payout = trade_command("invalid-min-payout", "idem-invalid-min-payout");
    invalid_min_payout["params"]["trade"]["min_payout"] = "1.50";
    const auto invalid_min_payout_response =
        post_json(config, bridge.bound_http_port(), invalid_min_payout);

    auto invalid_refund = trade_command("invalid-refund", "idem-invalid-refund");
    invalid_refund["params"]["trade"]["refund"] = "-0.10";
    const auto invalid_refund_response =
        post_json(config, bridge.bound_http_port(), invalid_refund);

    auto unsupported_balance_percent =
        signal_command("unsupported-balance-percent", "idem-unsupported-balance-percent");
    unsupported_balance_percent["params"]["signal"].erase("amount");
    unsupported_balance_percent["params"]["sizing"] = {
        {"mode", "balance_percent"},
        {"balance_percent", "0.10"}
    };
    const auto unsupported_balance_percent_response =
        post_json(config, bridge.bound_http_port(), unsupported_balance_percent);

    auto unsupported_balance_percent_mode =
        signal_command("unsupported-balance-percent-mode", "idem-unsupported-balance-percent-mode");
    unsupported_balance_percent_mode["params"]["signal"].erase("amount");
    unsupported_balance_percent_mode["params"]["sizing"] = {
        {"mode", "balance_percent"}
    };
    const auto unsupported_balance_percent_mode_response =
        post_json(config, bridge.bound_http_port(), unsupported_balance_percent_mode);

    auto unsupported_percent_mode =
        signal_command("unsupported-percent-mode", "idem-unsupported-percent-mode");
    unsupported_percent_mode["params"]["signal"].erase("amount");
    unsupported_percent_mode["params"]["sizing"] = {
        {"mode", "percent"}
    };
    const auto unsupported_percent_mode_response =
        post_json(config, bridge.bound_http_port(), unsupported_percent_mode);

    auto unsupported_ignore_signal_amount =
        signal_command("unsupported-ignore-signal-amount", "idem-unsupported-ignore-signal-amount");
    unsupported_ignore_signal_amount["params"]["sizing"] = {
        {"mode", "ignore_signal_amount"}
    };
    const auto unsupported_ignore_signal_amount_response =
        post_json(config, bridge.bound_http_port(), unsupported_ignore_signal_amount);

    auto unsupported_sizing_params =
        signal_command("unsupported-sizing-params", "idem-unsupported-sizing-params");
    unsupported_sizing_params["params"]["sizing"] = {
        {"mode", "risk_manager"},
        {"system", "kelly"},
        {"params", {{"fraction", "0.25"}}}
    };
    const auto unsupported_sizing_params_response =
        post_json(config, bridge.bound_http_port(), unsupported_sizing_params);

    auto fixed_amount_without_amount =
        signal_command("fixed-without-amount", "idem-fixed-without-amount");
    fixed_amount_without_amount["params"]["signal"].erase("amount");
    fixed_amount_without_amount["params"]["sizing"] = {
        {"mode", "fixed_amount"}
    };
    const auto fixed_amount_without_amount_response =
        post_json(config, bridge.bound_http_port(), fixed_amount_without_amount);

    auto duplicate_amount =
        signal_command("duplicate-amount", "idem-duplicate-amount");
    duplicate_amount["params"]["sizing"] = {
        {"mode", "fixed_amount"},
        {"amount", {
            {"value", "2.00"},
            {"currency", "USD"}
        }}
    };
    const auto duplicate_amount_response =
        post_json(config, bridge.bound_http_port(), duplicate_amount);
    bridge.shutdown();

    EXPECT_EQ(negative_signal_amount_response.at("error").at("code").get<int>(), -32602);
    EXPECT_EQ(invalid_min_payout_response.at("error").at("code").get<int>(), -32602);
    EXPECT_EQ(invalid_refund_response.at("error").at("code").get<int>(), -32602);
    EXPECT_EQ(unsupported_balance_percent_response.at("error").at("code").get<int>(), -32602);
    EXPECT_EQ(unsupported_balance_percent_mode_response.at("error").at("code").get<int>(), -32602);
    EXPECT_EQ(unsupported_percent_mode_response.at("error").at("code").get<int>(), -32602);
    EXPECT_EQ(unsupported_ignore_signal_amount_response.at("error").at("code").get<int>(), -32602);
    EXPECT_EQ(unsupported_sizing_params_response.at("error").at("code").get<int>(), -32602);
    EXPECT_EQ(fixed_amount_without_amount_response.at("error").at("code").get<int>(), -32602);
    EXPECT_EQ(duplicate_amount_response.at("error").at("code").get<int>(), -32602);
}

TEST(BridgeProtocolServerBridge, AcceptsProtocolSizingModeAliasesThatDtoCanRepresent) {
    namespace proto = optionx::bridges::protocol_v1;

    auto config = test_config();
    config.enable_websocket = false;

    proto::BridgeProtocolServerBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolServerConfig>(config)));

    optionx::MmSystemType mm_type = optionx::MmSystemType::NONE;
    double amount = 0.0;

    bridge.on_signal_id() = []() { return optionx::SignalId{40}; };
    bridge.on_trade_signal() =
        [&](std::unique_ptr<optionx::TradeSignal> signal) {
            mm_type = signal->mm_type;
            amount = signal->amount;
        };

    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));

    auto command = signal_command("fixed-amount-alias", "idem-fixed-amount-alias");
    command["params"]["signal"].erase("amount");
    command["params"]["sizing"] = {
        {"mode", "fixed_amount"},
        {"amount", {
            {"value", "2.50"},
            {"currency", "USD"}
        }}
    };
    const auto response = post_json(config, bridge.bound_http_port(), command);
    bridge.shutdown();

    EXPECT_EQ(response.at("result").at("status").get<std::string>(), "accepted");
    EXPECT_EQ(mm_type, optionx::MmSystemType::FIXED);
    EXPECT_DOUBLE_EQ(amount, 2.50);
}

TEST(BridgeProtocolServerBridge, EvictsCompletedOperationsWhenIdempotencyCacheIsFull) {
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
    bridge.shutdown();

    EXPECT_EQ(first.at("result").at("status").get<std::string>(), "accepted");
    EXPECT_EQ(second.at("result").at("status").get<std::string>(), "accepted");
    EXPECT_EQ(signal_count.load(), 2);
}

TEST(BridgeProtocolServerBridge, FailsClosedWhenOnlyInFlightOperationsCanBeEvicted) {
    namespace proto = optionx::bridges::protocol_v1;

    auto config = test_config();
    config.enable_websocket = false;
    config.dedupe_cache_size = 1;

    proto::BridgeProtocolServerBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolServerConfig>(config)));

    std::mutex mutex;
    std::condition_variable cv;
    bool callback_entered = false;
    bool release_callback = false;
    std::atomic<int> signal_count{0};
    bridge.on_signal_id() = []() { return optionx::SignalId{41}; };
    bridge.on_trade_signal() = [&](std::unique_ptr<optionx::TradeSignal>) {
        ++signal_count;
        {
            std::lock_guard<std::mutex> lock(mutex);
            callback_entered = true;
        }
        cv.notify_all();
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait_for(lock, std::chrono::seconds(5), [&release_callback]() {
            return release_callback;
        });
    };

    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));

    nlohmann::json first;
    std::thread first_thread([&]() {
        first = post_json(
            config,
            bridge.bound_http_port(),
            trade_command("trade-cache-1", "idem-cache-1"));
    });

    {
        std::unique_lock<std::mutex> lock(mutex);
        ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(5), [&callback_entered]() {
            return callback_entered;
        }));
    }

    const auto second = post_json(
        config,
        bridge.bound_http_port(),
        trade_command("trade-cache-2", "idem-cache-2"));

    {
        std::lock_guard<std::mutex> lock(mutex);
        release_callback = true;
    }
    cv.notify_all();
    if (first_thread.joinable()) {
        first_thread.join();
    }
    bridge.shutdown();

    EXPECT_EQ(first.at("result").at("status").get<std::string>(), "accepted");
    EXPECT_EQ(second.at("result").at("status").get<std::string>(), "rejected");
    EXPECT_EQ(
        second.at("result").at("reason").at("code").get<std::string>(),
        "idempotency_cache_full");
    EXPECT_EQ(signal_count.load(), 1);
}

TEST(BridgeProtocolServerBridge, ExpiredOperationIsDispatchedAgain) {
    namespace proto = optionx::bridges::protocol_v1;

    auto config = test_config();
    config.enable_websocket = false;
    config.operation_cache_retention_ms = 1;

    proto::BridgeProtocolServerBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolServerConfig>(config)));

    std::atomic<optionx::SignalId> next_signal_id{42};
    std::atomic<int> signal_count{0};
    bridge.on_signal_id() = [&next_signal_id]() {
        return next_signal_id.fetch_add(1);
    };
    bridge.on_trade_signal() = [&signal_count](std::unique_ptr<optionx::TradeSignal>) {
        ++signal_count;
    };

    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));

    const auto first = post_json(
        config,
        bridge.bound_http_port(),
        trade_command("trade-expired-1", "idem-expired"));
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    const auto second = post_json(
        config,
        bridge.bound_http_port(),
        trade_command("trade-expired-2", "idem-expired"));
    bridge.shutdown();

    EXPECT_EQ(first.at("result").at("status").get<std::string>(), "accepted");
    EXPECT_EQ(second.at("result").at("status").get<std::string>(), "accepted");
    EXPECT_EQ(signal_count.load(), 2);
}

TEST(BridgeProtocolServerBridge, CacheHitRefreshesLruPosition) {
    namespace proto = optionx::bridges::protocol_v1;

    auto config = test_config();
    config.enable_websocket = false;
    config.dedupe_cache_size = 2;

    proto::BridgeProtocolServerBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolServerConfig>(config)));

    std::atomic<optionx::SignalId> next_signal_id{60};
    std::atomic<int> signal_count{0};
    bridge.on_signal_id() = [&next_signal_id]() {
        return next_signal_id.fetch_add(1);
    };
    bridge.on_trade_signal() = [&signal_count](std::unique_ptr<optionx::TradeSignal>) {
        ++signal_count;
    };

    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));

    static_cast<void>(post_json(
        config,
        bridge.bound_http_port(),
        trade_command("trade-lru-1", "idem-lru-1")));
    static_cast<void>(post_json(
        config,
        bridge.bound_http_port(),
        trade_command("trade-lru-2", "idem-lru-2")));
    static_cast<void>(post_json(
        config,
        bridge.bound_http_port(),
        trade_command("trade-lru-1-retry", "idem-lru-1")));
    static_cast<void>(post_json(
        config,
        bridge.bound_http_port(),
        trade_command("trade-lru-3", "idem-lru-3")));
    static_cast<void>(post_json(
        config,
        bridge.bound_http_port(),
        trade_command("trade-lru-1-retry-2", "idem-lru-1")));
    static_cast<void>(post_json(
        config,
        bridge.bound_http_port(),
        trade_command("trade-lru-2-retry", "idem-lru-2")));
    bridge.shutdown();

    EXPECT_EQ(signal_count.load(), 4);
}

TEST(BridgeProtocolServerBridge, CompletedResultCannotExceedGlobalByteBudget) {
    namespace proto = optionx::bridges::protocol_v1;

    auto config = test_config();
    config.enable_websocket = false;
    config.max_operation_cache_bytes = 512;

    proto::BridgeProtocolServerBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolServerConfig>(config)));

    std::atomic<optionx::SignalId> next_signal_id{70};
    std::atomic<int> signal_count{0};
    bridge.on_signal_id() = [&next_signal_id]() {
        return next_signal_id.fetch_add(1);
    };
    bridge.on_trade_signal() = [&signal_count](std::unique_ptr<optionx::TradeSignal>) {
        if (++signal_count == 1) {
            throw std::runtime_error(std::string(900, 'x'));
        }
    };

    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));

    const auto failed = post_json(
        config,
        bridge.bound_http_port(),
        trade_command("trade-oversized-result-1", "idem-oversized-result-1"));
    const auto accepted = post_json(
        config,
        bridge.bound_http_port(),
        trade_command("trade-oversized-result-2", "idem-oversized-result-2"));
    bridge.shutdown();

    EXPECT_EQ(failed.at("result").at("status").get<std::string>(), "rejected");
    EXPECT_EQ(
        failed.at("result").at("reason").at("code").get<std::string>(),
        "dispatch_failed");
    EXPECT_EQ(accepted.at("result").at("status").get<std::string>(), "accepted");
    EXPECT_EQ(signal_count.load(), 2);
}

TEST(BridgeProtocolServerBridge, FailsClosedWhenOperationCacheBytesAreFull) {
    namespace proto = optionx::bridges::protocol_v1;

    auto config = test_config();
    config.enable_websocket = false;
    config.max_operation_cache_bytes = 32;

    proto::BridgeProtocolServerBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolServerConfig>(config)));

    std::atomic<int> signal_count{0};
    bridge.on_signal_id() = []() { return optionx::SignalId{45}; };
    bridge.on_trade_signal() = [&signal_count](std::unique_ptr<optionx::TradeSignal>) {
        ++signal_count;
    };

    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));

    const auto response = post_json(
        config,
        bridge.bound_http_port(),
        trade_command("trade-byte-cache", "idem-byte-cache"));
    bridge.shutdown();

    EXPECT_EQ(response.at("result").at("status").get<std::string>(), "rejected");
    EXPECT_EQ(
        response.at("result").at("reason").at("code").get<std::string>(),
        "idempotency_cache_full");
    EXPECT_EQ(signal_count.load(), 0);
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

TEST(BridgeProtocolServerBridge, AllowsIndependentOperationsToReuseJsonRpcId) {
    namespace proto = optionx::bridges::protocol_v1;

    auto config = test_config();
    config.enable_websocket = false;

    proto::BridgeProtocolServerBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolServerConfig>(config)));

    std::atomic<optionx::SignalId> next_signal_id{55};
    std::atomic<int> signal_count{0};
    bridge.on_signal_id() = [&next_signal_id]() {
        return next_signal_id.fetch_add(1);
    };
    bridge.on_trade_signal() = [&signal_count](std::unique_ptr<optionx::TradeSignal>) {
        ++signal_count;
    };

    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));

    const auto first = post_json(
        config,
        bridge.bound_http_port(),
        trade_command("1", "idem-json-id-a"));
    const auto second = post_json(
        config,
        bridge.bound_http_port(),
        trade_command("1", "idem-json-id-b", "GBPUSD"));
    bridge.shutdown();

    EXPECT_EQ(first.at("result").at("status").get<std::string>(), "accepted");
    EXPECT_EQ(second.at("result").at("status").get<std::string>(), "accepted");
    EXPECT_EQ(signal_count.load(), 2);
}

TEST(BridgeProtocolServerBridge, DistinguishesSmallScientificDecimalsInFingerprint) {
    namespace proto = optionx::bridges::protocol_v1;

    auto config = test_config();
    config.enable_websocket = false;

    proto::BridgeProtocolServerBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolServerConfig>(config)));
    bridge.on_signal_id() = []() { return optionx::SignalId{56}; };
    bridge.on_trade_signal() = [](std::unique_ptr<optionx::TradeSignal>) {};

    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));

    auto first_command = trade_command("tiny-decimal-a", "idem-tiny-decimal");
    first_command["params"]["trade"]["amount"]["value"] = "4e-13";
    auto second_command = trade_command("tiny-decimal-b", "idem-tiny-decimal");
    second_command["params"]["trade"]["amount"]["value"] = "5e-13";

    const auto first = post_json(config, bridge.bound_http_port(), first_command);
    const auto second = post_json(config, bridge.bound_http_port(), second_command);
    bridge.shutdown();

    EXPECT_EQ(first.at("result").at("status").get<std::string>(), "accepted");
    EXPECT_EQ(second.at("result").at("status").get<std::string>(), "rejected");
    EXPECT_EQ(
        second.at("result").at("reason").at("code").get<std::string>(),
        "idempotency_conflict");
}

TEST(BridgeProtocolServerBridge, CanonicalizesEquivalentDecimalRepresentations) {
    namespace proto = optionx::bridges::protocol_v1;

    auto config = test_config();
    config.enable_websocket = false;

    proto::BridgeProtocolServerBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolServerConfig>(config)));
    bridge.on_signal_id() = []() { return optionx::SignalId{57}; };
    std::atomic<int> signal_count{0};
    bridge.on_trade_signal() = [&signal_count](std::unique_ptr<optionx::TradeSignal>) {
        ++signal_count;
    };

    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));

    auto first_command = trade_command("decimal-a", "idem-decimal");
    first_command["params"]["trade"]["amount"]["value"] = "0.10";
    auto second_command = trade_command("decimal-b", "idem-decimal");
    second_command["params"]["trade"]["amount"]["value"] = 0.1;
    auto third_command = trade_command("decimal-c", "idem-decimal-one");
    third_command["params"]["trade"]["amount"]["value"] = "1e0";
    auto fourth_command = trade_command("decimal-d", "idem-decimal-one");
    fourth_command["params"]["trade"]["amount"]["value"] = 1.0;

    const auto first = post_json(config, bridge.bound_http_port(), first_command);
    const auto second = post_json(config, bridge.bound_http_port(), second_command);
    const auto third = post_json(config, bridge.bound_http_port(), third_command);
    const auto fourth = post_json(config, bridge.bound_http_port(), fourth_command);
    bridge.shutdown();

    EXPECT_EQ(first.at("result").at("status").get<std::string>(), "accepted");
    EXPECT_EQ(second.at("result").at("status").get<std::string>(), "accepted");
    EXPECT_EQ(
        second.at("result").at("operation_id").get<std::string>(),
        first.at("result").at("operation_id").get<std::string>());
    EXPECT_EQ(third.at("result").at("status").get<std::string>(), "accepted");
    EXPECT_EQ(
        fourth.at("result").at("operation_id").get<std::string>(),
        third.at("result").at("operation_id").get<std::string>());
    EXPECT_EQ(signal_count.load(), 2);
}

TEST(BridgeProtocolServerBridge, CanonicalizesBusinessAliasesAndIdentifiersInFingerprint) {
    namespace proto = optionx::bridges::protocol_v1;

    auto config = test_config();
    config.enable_websocket = false;

    proto::BridgeProtocolServerBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolServerConfig>(config)));
    bridge.on_signal_id() = []() { return optionx::SignalId{59}; };
    std::atomic<int> signal_count{0};
    double dispatched_amount = 0.0;
    optionx::CurrencyType dispatched_currency = optionx::CurrencyType::UNKNOWN;
    optionx::OrderType dispatched_order_type = optionx::OrderType::UNKNOWN;
    optionx::OptionType dispatched_option_type = optionx::OptionType::UNKNOWN;
    std::uint32_t dispatched_duration = 0;
    std::int64_t dispatched_unique_id = 0;
    bridge.on_trade_signal() =
        [&](std::unique_ptr<optionx::TradeSignal> signal) {
            dispatched_amount = signal->amount;
            dispatched_currency = signal->currency;
            dispatched_order_type = signal->order_type;
            dispatched_option_type = signal->option_type;
            dispatched_duration = signal->duration;
            dispatched_unique_id = signal->unique_id;
            ++signal_count;
        };

    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));

    auto first_command = trade_command("alias-a", "idem-alias");
    first_command["params"]["identity"]["unique_id"] = 123;
    first_command["params"]["trade"].erase("order_type");
    first_command["params"]["trade"]["direction"] = "buy";
    first_command["params"]["trade"]["option_type"] = "sprint";
    first_command["params"]["trade"]["amount"] = "1.00";
    first_command["params"]["trade"]["currency"] = "usd";
    first_command["params"]["trade"].erase("expiry");
    first_command["params"]["trade"]["duration_sec"] = 60;

    auto second_command = trade_command("alias-b", "idem-alias");
    second_command["params"]["identity"]["unique_id"] = "123";
    second_command["params"]["trade"]["order_type"] = "BUY";
    second_command["params"]["trade"]["option_type"] = "SPRINT";
    second_command["params"]["trade"]["amount"] = {
        {"value", 1},
        {"currency", "USD"}
    };
    second_command["params"]["trade"]["expiry"] = {
        {"kind", "duration"},
        {"duration_ms", "60000"}
    };

    const auto first = post_json(config, bridge.bound_http_port(), first_command);
    const auto second = post_json(config, bridge.bound_http_port(), second_command);
    bridge.shutdown();

    EXPECT_EQ(first.at("result").at("status").get<std::string>(), "accepted");
    EXPECT_EQ(second.at("result").at("status").get<std::string>(), "accepted");
    EXPECT_EQ(
        second.at("result").at("operation_id").get<std::string>(),
        first.at("result").at("operation_id").get<std::string>());
    EXPECT_EQ(signal_count.load(), 1);
    EXPECT_DOUBLE_EQ(dispatched_amount, 1.0);
    EXPECT_EQ(dispatched_currency, optionx::CurrencyType::USD);
    EXPECT_EQ(dispatched_order_type, optionx::OrderType::BUY);
    EXPECT_EQ(dispatched_option_type, optionx::OptionType::SPRINT);
    EXPECT_EQ(dispatched_duration, 60u);
    EXPECT_EQ(dispatched_unique_id, 123);
}

TEST(BridgeProtocolServerBridge, KeepsDurationAndAbsoluteExpiryDistinctInFingerprint) {
    namespace proto = optionx::bridges::protocol_v1;

    auto config = test_config();
    config.enable_websocket = false;

    proto::BridgeProtocolServerBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolServerConfig>(config)));
    bridge.on_signal_id() = []() { return optionx::SignalId{60}; };
    std::atomic<int> signal_count{0};
    bridge.on_trade_signal() = [&signal_count](std::unique_ptr<optionx::TradeSignal>) {
        ++signal_count;
    };

    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));

    auto duration_command = trade_command("expiry-kind-a", "idem-expiry-kind");
    duration_command["params"]["trade"]["expiry"] = {
        {"kind", "duration"},
        {"duration_ms", 60000}
    };
    auto absolute_command = trade_command("expiry-kind-b", "idem-expiry-kind");
    absolute_command["params"]["trade"]["expiry"] = {
        {"kind", "absolute"},
        {"expires_at_ms", optionx::bridges::metatrader_file::detail::unix_time_ms() + 60000}
    };

    const auto first = post_json(config, bridge.bound_http_port(), duration_command);
    const auto second = post_json(config, bridge.bound_http_port(), absolute_command);
    bridge.shutdown();

    EXPECT_EQ(first.at("result").at("status").get<std::string>(), "accepted");
    EXPECT_EQ(second.at("result").at("status").get<std::string>(), "rejected");
    EXPECT_EQ(
        second.at("result").at("reason").at("code").get<std::string>(),
        "idempotency_conflict");
    EXPECT_EQ(signal_count.load(), 1);
}

TEST(BridgeProtocolServerBridge, CanonicalizesRoutingPlatformTypeInFingerprint) {
    namespace proto = optionx::bridges::protocol_v1;

    auto config = test_config();
    config.enable_websocket = false;

    proto::BridgeProtocolServerBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolServerConfig>(config)));
    bridge.on_signal_id() = []() { return optionx::SignalId{58}; };
    std::atomic<int> signal_count{0};
    bridge.on_trade_signal() = [&signal_count](std::unique_ptr<optionx::TradeSignal>) {
        ++signal_count;
    };

    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));

    auto first_command = trade_command("platform-a", "idem-platform");
    first_command["params"]["routing"]["platform_type"] = "intrade.bar";
    auto second_command = trade_command("platform-b", "idem-platform");
    second_command["params"]["routing"]["platform_type"] = "INTRADE_BAR";
    auto third_command = trade_command("platform-c", "idem-platform");
    third_command["params"]["routing"]["platform_type"] = "TRADEUP";

    const auto first = post_json(config, bridge.bound_http_port(), first_command);
    const auto second = post_json(config, bridge.bound_http_port(), second_command);
    const auto third = post_json(config, bridge.bound_http_port(), third_command);
    bridge.shutdown();

    EXPECT_EQ(first.at("result").at("status").get<std::string>(), "accepted");
    EXPECT_EQ(second.at("result").at("status").get<std::string>(), "accepted");
    EXPECT_EQ(
        second.at("result").at("operation_id").get<std::string>(),
        first.at("result").at("operation_id").get<std::string>());
    EXPECT_EQ(third.at("result").at("status").get<std::string>(), "rejected");
    EXPECT_EQ(
        third.at("result").at("reason").at("code").get<std::string>(),
        "idempotency_conflict");
    EXPECT_EQ(signal_count.load(), 1);
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

TEST(BridgeProtocolServerBridge, DuplicateRetryReturnsProcessingDuringDispatchFailure) {
    namespace proto = optionx::bridges::protocol_v1;

    auto config = test_config();

    proto::BridgeProtocolServerBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolServerConfig>(config)));

    std::mutex mutex;
    std::condition_variable cv;
    bool callback_entered = false;
    bool release_callback = false;
    std::atomic<int> signal_count{0};

    bridge.on_signal_id() = []() { return optionx::SignalId{70}; };
    bridge.on_trade_signal() =
        [&](std::unique_ptr<optionx::TradeSignal>) {
            ++signal_count;
            {
                std::lock_guard<std::mutex> lock(mutex);
                callback_entered = true;
            }
            cv.notify_all();
            std::unique_lock<std::mutex> lock(mutex);
            cv.wait_for(lock, std::chrono::seconds(5), [&release_callback]() {
                return release_callback;
            });
            throw std::runtime_error("dispatch boom");
        };

    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));
    ASSERT_TRUE(wait_for_ws_port(bridge));

    nlohmann::json http_response;
    std::thread http_thread([&]() {
        http_response = post_json(
            config,
            bridge.bound_http_port(),
            trade_command("dispatch-fail-http", "idem-dispatch-fail"));
    });

    {
        std::unique_lock<std::mutex> lock(mutex);
        ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(5), [&callback_entered]() {
            return callback_entered;
        }));
    }

    std::vector<nlohmann::json> ws_messages;
    bool opened = false;
    WsClient client(
        config.address + ":" +
        std::to_string(bridge.bound_websocket_port()) +
        config.websocket_path);
    client.config.header.emplace("Sec-WebSocket-Protocol", config.websocket_subprotocol);
    client.config.header.emplace("X-OptionX-Secret", config.secret);
    client.on_open = [&](std::shared_ptr<WsClient::Connection> connection) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            opened = true;
        }
        cv.notify_all();
        connection->send(ws_trade_command("dispatch-fail-ws", "idem-dispatch-fail").dump(-1));
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
        ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(5), [&ws_messages]() {
            return !ws_messages.empty();
        }));
        release_callback = true;
    }
    cv.notify_all();

    if (http_thread.joinable()) {
        http_thread.join();
    }
    client.stop();
    if (client_thread.joinable()) {
        client_thread.join();
    }
    bridge.shutdown();

    ASSERT_EQ(ws_messages.size(), 1u);
    EXPECT_EQ(
        http_response.at("result").at("reason").at("code").get<std::string>(),
        "dispatch_failed");
    EXPECT_EQ(
        ws_messages[0].at("result").at("reason").at("code").get<std::string>(),
        "operation_in_progress");
    EXPECT_EQ(signal_count.load(), 1);
}

TEST(BridgeProtocolServerBridge, DedupesConcurrentHttpAndWebSocketTradeRetry) {
    namespace proto = optionx::bridges::protocol_v1;

    auto config = test_config();

    proto::BridgeProtocolServerBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolServerConfig>(config)));

    std::atomic<optionx::SignalId> next_signal_id{60};
    std::atomic<int> signal_count{0};
    std::mutex mutex;
    std::condition_variable cv;
    bool callback_entered = false;
    bool release_callback = false;
    bridge.on_signal_id() = [&next_signal_id]() {
        return next_signal_id.fetch_add(1);
    };
    bridge.on_trade_signal() = [&](std::unique_ptr<optionx::TradeSignal>) {
        ++signal_count;
        {
            std::lock_guard<std::mutex> lock(mutex);
            callback_entered = true;
        }
        cv.notify_all();
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait_for(lock, std::chrono::seconds(5), [&release_callback]() {
            return release_callback;
        });
    };

    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));
    ASSERT_TRUE(wait_for_ws_port(bridge));

    std::vector<nlohmann::json> ws_messages;
    bool opened = false;

    WsClient client(
        config.address + ":" +
        std::to_string(bridge.bound_websocket_port()) +
        config.websocket_path);
    client.config.header.emplace("Sec-WebSocket-Protocol", config.websocket_subprotocol);
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
        ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(5), [&callback_entered]() {
            return callback_entered;
        }));
    }

    const auto http_response = post_json(
        config,
        bridge.bound_http_port(),
        trade_command("http-duplicate", "idem-concurrent"));

    {
        std::unique_lock<std::mutex> lock(mutex);
        release_callback = true;
    }
    cv.notify_all();

    {
        std::unique_lock<std::mutex> lock(mutex);
        ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(5), [&ws_messages]() {
            return !ws_messages.empty();
        }));
    }

    client.stop();
    if (client_thread.joinable()) {
        client_thread.join();
    }
    bridge.shutdown();

    ASSERT_EQ(ws_messages.size(), 1u);
    EXPECT_EQ(http_response.at("result").at("status").get<std::string>(), "processing");
    EXPECT_EQ(
        http_response.at("result").at("reason").at("code").get<std::string>(),
        "operation_in_progress");
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
    client.config.header.emplace("Sec-WebSocket-Protocol", config.websocket_subprotocol);
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

TEST(BridgeProtocolServerBridge, RejectsUnsupportedWebSocketSubprotocol) {
    namespace proto = optionx::bridges::protocol_v1;

    auto config = test_config();
    config.enable_http = false;

    proto::BridgeProtocolServerBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolServerConfig>(config)));
    bridge.run();
    ASSERT_TRUE(wait_for_ws_port(bridge));

    std::mutex mutex;
    std::condition_variable cv;
    bool opened = false;
    bool failed = false;

    WsClient client(
        config.address + ":" +
        std::to_string(bridge.bound_websocket_port()) +
        config.websocket_path);
    client.config.header.emplace("Sec-WebSocket-Protocol", "wrong.protocol");
    client.config.header.emplace("X-OptionX-Secret", config.secret);
    client.on_open = [&](std::shared_ptr<WsClient::Connection>) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            opened = true;
        }
        cv.notify_all();
    };
    client.on_error = [&](std::shared_ptr<WsClient::Connection>,
                          const SimpleWeb::error_code&) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            failed = true;
        }
        cv.notify_all();
    };

    std::thread client_thread([&client]() {
        client.start();
    });

    {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait_for(lock, std::chrono::seconds(5), [&]() {
            return opened || failed;
        });
    }

    client.stop();
    if (client_thread.joinable()) {
        client_thread.join();
    }
    bridge.shutdown();

    EXPECT_FALSE(opened);
    EXPECT_TRUE(failed);
}

TEST(BridgeProtocolServerBridge, RunWhileAlreadyRunningReturnsImmediately) {
    namespace proto = optionx::bridges::protocol_v1;

    auto config = test_config();
    config.enable_websocket = false;

    proto::BridgeProtocolServerBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolServerConfig>(config)));

    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));

    std::atomic<bool> returned{false};
    std::thread run_thread([&]() {
        bridge.run();
        returned.store(true);
    });

    for (int i = 0; i < 50 && !returned.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (!returned.load()) {
        bridge.shutdown();
    }
    if (run_thread.joinable()) {
        run_thread.join();
    }
    bridge.shutdown();

    EXPECT_TRUE(returned.load());
}

TEST(BridgeProtocolServerBridge, StartFailureDoesNotLeaveRuntimeMarkedRunning) {
    namespace proto = optionx::bridges::protocol_v1;

    auto config = test_config();
    config.enable_websocket = false;
    config.address = "192.0.2.1";
    config.allow_insecure_remote = true;

    proto::BridgeProtocolServerBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolServerConfig>(config)));

    std::mutex mutex;
    std::condition_variable cv;
    bool failed = false;
    bridge.on_status_update() = [&](const optionx::BridgeStatusUpdate& update) {
        if (update.status == optionx::BridgeStatus::SERVER_START_FAILED) {
            {
                std::lock_guard<std::mutex> lock(mutex);
                failed = true;
            }
            cv.notify_all();
        }
    };

    bridge.run();
    {
        std::unique_lock<std::mutex> lock(mutex);
        ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(5), [&failed]() {
            return failed;
        }));
    }
    for (int i = 0; i < 200 && bridge.bound_http_port() != 0; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_EQ(bridge.bound_http_port(), 0);

    bridge.shutdown();
    config.address = "127.0.0.1";
    config.allow_insecure_remote = false;
    ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolServerConfig>(config)));
    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));
    bridge.shutdown();
}

TEST(BridgeProtocolServerBridge, PostReadyStartExceptionWaitsForTransportThreadExit) {
    namespace proto = optionx::bridges::protocol_v1;

    auto config = test_config();
    config.enable_websocket = false;

    proto::BridgeProtocolServerBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolServerConfig>(config)));

    std::mutex mutex;
    std::condition_variable cv;
    bool rollback_hook_entered = false;
    bool release_transport_thread = false;
    std::atomic<bool> failed{false};
    std::atomic<int> started_port{0};

    proto::BridgeProtocolServerBridge::TestHooks hooks;
    hooks.after_http_ready = [&] {
        bridge.simulate_http_post_ready_start_failure_for_test(
            "post-ready start failure");
    };
    hooks.after_http_post_ready_failure_rollback = [&] {
        std::unique_lock<std::mutex> lock(mutex);
        rollback_hook_entered = true;
        cv.notify_all();
        cv.wait(lock, [&release_transport_thread]() {
            return release_transport_thread;
        });
    };
    bridge.set_test_hooks(std::move(hooks));

    bridge.on_status_update() = [&](const optionx::BridgeStatusUpdate& update) {
        if (update.status == optionx::BridgeStatus::SERVER_STARTED) {
            started_port.store(static_cast<int>(bridge.bound_http_port()));
        }
        if (update.status == optionx::BridgeStatus::SERVER_START_FAILED) {
            failed.store(true);
        }
    };

    bridge.run();
    {
        std::unique_lock<std::mutex> lock(mutex);
        ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(5), [&rollback_hook_entered]() {
            return rollback_hook_entered;
        }));
    }

    std::atomic<bool> shutdown_returned{false};
    std::thread shutdown_thread([&] {
        bridge.shutdown();
        shutdown_returned.store(true);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_FALSE(shutdown_returned.load());

    {
        std::lock_guard<std::mutex> lock(mutex);
        release_transport_thread = true;
    }
    cv.notify_all();
    shutdown_thread.join();

    ASSERT_TRUE(failed.load());
    const auto restart_port = static_cast<unsigned short>(started_port.load());
    ASSERT_NE(restart_port, 0);

    bridge.on_status_update() = {};
    bridge.set_test_hooks({});
    config.http_port = restart_port;
    ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolServerConfig>(config)));
    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));
    EXPECT_EQ(bridge.bound_http_port(), restart_port);
    bridge.shutdown();
}

TEST(BridgeProtocolServerBridge, DelayedFailedStartRollbackCannotStopNewGeneration) {
    namespace proto = optionx::bridges::protocol_v1;

    auto config = test_config();
    config.enable_websocket = false;

    proto::BridgeProtocolServerBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolServerConfig>(config)));

    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));
    const auto old_generation = bridge.lifecycle_generation_for_test();
    bridge.shutdown();

    ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolServerConfig>(config)));
    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));
    ASSERT_NE(bridge.lifecycle_generation_for_test(), old_generation);

    bridge.simulate_failed_start_rollback_for_test(old_generation);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    const auto response = post_json(
        config,
        bridge.bound_http_port(),
        {
            {"jsonrpc", "2.0"},
            {"id", "hello-after-stale-rollback"},
            {"method", "protocol.hello"},
            {"params", {{"version", "1.0"}}}
        });
    EXPECT_EQ(
        response.at("result").at("selected_protocol_version").get<std::string>(),
        "1");

    bridge.shutdown();
}

TEST(BridgeProtocolServerBridge, RunDuringShutdownDoesNotCreateZombieRuntime) {
    namespace proto = optionx::bridges::protocol_v1;

    auto config = test_config();
    config.enable_websocket = false;

    proto::BridgeProtocolServerBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolServerConfig>(config)));

    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));

    std::atomic<bool> shutdown_entered{false};
    std::thread shutdown_thread([&]() {
        shutdown_entered.store(true);
        bridge.shutdown();
    });
    for (int i = 0; i < 200 && !shutdown_entered.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    std::atomic<bool> run_returned{false};
    std::thread run_thread([&]() {
        bridge.run();
        run_returned.store(true);
    });

    if (shutdown_thread.joinable()) {
        shutdown_thread.join();
    }
    if (run_thread.joinable()) {
        run_thread.join();
    }
    EXPECT_TRUE(run_returned.load());

    bridge.shutdown();
    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));
    bridge.shutdown();
}

TEST(BridgeProtocolServerBridge, ConcurrentShutdownsDoNotPublishStoppedEarly) {
    namespace proto = optionx::bridges::protocol_v1;

    auto config = test_config();
    config.enable_websocket = false;

    proto::BridgeProtocolServerBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolServerConfig>(config)));

    std::mutex mutex;
    std::condition_variable cv;
    bool callback_entered = false;
    bool release_callback = false;
    bridge.on_signal_id() = []() { return optionx::SignalId{91}; };
    bridge.on_trade_signal() = [&](std::unique_ptr<optionx::TradeSignal>) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            callback_entered = true;
        }
        cv.notify_all();
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait_for(lock, std::chrono::seconds(5), [&release_callback]() {
            return release_callback;
        });
    };

    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));

    nlohmann::json response;
    std::atomic<bool> request_failed{false};
    std::thread request_thread([&]() {
        try {
            response = post_json(
                config,
                bridge.bound_http_port(),
                trade_command("trade-concurrent-shutdown", "idem-concurrent-shutdown"));
        } catch (...) {
            request_failed.store(true);
        }
    });
    {
        std::unique_lock<std::mutex> lock(mutex);
        ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(5), [&callback_entered]() {
            return callback_entered;
        }));
    }

    std::thread shutdown_one([&]() {
        bridge.shutdown();
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    std::thread shutdown_two([&]() {
        bridge.shutdown();
    });

    std::atomic<bool> run_returned{false};
    std::thread run_thread([&]() {
        bridge.run();
        run_returned.store(true);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_FALSE(run_returned.load());

    {
        std::lock_guard<std::mutex> lock(mutex);
        release_callback = true;
    }
    cv.notify_all();

    if (request_thread.joinable()) {
        request_thread.join();
    }
    if (shutdown_one.joinable()) {
        shutdown_one.join();
    }
    if (shutdown_two.joinable()) {
        shutdown_two.join();
    }
    if (run_thread.joinable()) {
        run_thread.join();
    }

    EXPECT_TRUE(run_returned.load());
    ASSERT_TRUE(wait_for_http_port(bridge));
    bridge.shutdown();
}

TEST(BridgeProtocolServerBridge, ReentrantLifecycleFromHttpWorkerDuringShutdownDoesNotDeadlock) {
    namespace proto = optionx::bridges::protocol_v1;

    auto config = test_config();
    config.enable_websocket = false;

    proto::BridgeProtocolServerBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolServerConfig>(config)));

    std::mutex mutex;
    std::condition_variable cv;
    bool callback_entered = false;
    bool external_shutdown_started = false;
    std::atomic<bool> reentrant_returned{false};
    std::atomic<bool> request_failed{false};
    bridge.on_signal_id() = []() { return optionx::SignalId{92}; };
    bridge.on_trade_signal() = [&](std::unique_ptr<optionx::TradeSignal>) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            callback_entered = true;
        }
        cv.notify_all();
        {
            std::unique_lock<std::mutex> lock(mutex);
            cv.wait_for(lock, std::chrono::seconds(5), [&external_shutdown_started]() {
                return external_shutdown_started;
            });
        }
        bridge.shutdown();
        bridge.run();
        reentrant_returned.store(true);
    };

    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));

    std::thread request_thread([&]() {
        try {
            static_cast<void>(post_json(
                config,
                bridge.bound_http_port(),
                trade_command("trade-reentrant-shutdown", "idem-reentrant-shutdown")));
        } catch (...) {
            request_failed.store(true);
        }
    });
    {
        std::unique_lock<std::mutex> lock(mutex);
        ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(5), [&callback_entered]() {
            return callback_entered;
        }));
    }

    std::thread shutdown_thread([&]() {
        {
            std::lock_guard<std::mutex> lock(mutex);
            external_shutdown_started = true;
        }
        cv.notify_all();
        bridge.shutdown();
    });

    if (request_thread.joinable()) {
        request_thread.join();
    }
    if (shutdown_thread.joinable()) {
        shutdown_thread.join();
    }

    EXPECT_TRUE(reentrant_returned.load());
    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));
    bridge.shutdown();
}

TEST(BridgeProtocolServerBridge, ShutdownInitiatedByHttpWorkerDoesNotDeadlock) {
    namespace proto = optionx::bridges::protocol_v1;

    auto config = test_config();
    config.enable_websocket = false;

    proto::BridgeProtocolServerBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolServerConfig>(config)));

    std::atomic<bool> shutdown_returned{false};
    std::atomic<bool> request_failed{false};
    bridge.on_signal_id() = []() { return optionx::SignalId{93}; };
    bridge.on_trade_signal() = [&](std::unique_ptr<optionx::TradeSignal>) {
        bridge.shutdown();
        shutdown_returned.store(true);
    };

    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));

    nlohmann::json response;
    std::thread request_thread([&]() {
        try {
            response = post_json(
                config,
                bridge.bound_http_port(),
                trade_command("trade-worker-shutdown", "idem-worker-shutdown"));
        } catch (...) {
            request_failed.store(true);
        }
    });
    if (request_thread.joinable()) {
        request_thread.join();
    }

    EXPECT_TRUE(shutdown_returned.load());
    EXPECT_FALSE(request_failed.load());
    if (!request_failed.load()) {
        EXPECT_EQ(response.at("result").at("status").get<std::string>(), "accepted");
    }

    bridge.shutdown();
    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));
    bridge.shutdown();
}

#if !defined(_WIN32)
TEST(BridgeProtocolServerBridge, NewHttpCallbackCannotEnterWhilePendingShutdownDrains) {
    namespace proto = optionx::bridges::protocol_v1;

    auto config = test_config();
    config.enable_websocket = false;

    proto::BridgeProtocolServerBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolServerConfig>(config)));

    std::mutex mutex;
    std::condition_variable cv;
    bool first_callback_entered = false;
    bool release_first_callback = false;
    std::atomic<int> signal_callbacks{0};
    bridge.on_signal_id() = []() { return optionx::SignalId{94}; };
    bridge.on_trade_signal() = [&](std::unique_ptr<optionx::TradeSignal>) {
        const auto count = ++signal_callbacks;
        if (count != 1) {
            return;
        }
        {
            std::lock_guard<std::mutex> lock(mutex);
            first_callback_entered = true;
        }
        cv.notify_all();
        bridge.shutdown();
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait_for(lock, std::chrono::seconds(5), [&release_first_callback]() {
            return release_first_callback;
        });
    };

    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));

    nlohmann::json first_response;
    std::thread first_request([&]() {
        first_response = post_json(
            config,
            bridge.bound_http_port(),
            trade_command("trade-pending-shutdown-a", "idem-pending-shutdown-a"));
    });

    {
        std::unique_lock<std::mutex> lock(mutex);
        ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(5), [&first_callback_entered]() {
            return first_callback_entered;
        }));
    }

    nlohmann::json rejected;
    std::atomic<bool> second_done{false};
    std::atomic<bool> second_failed{false};
    std::thread second_request([&]() {
        try {
            rejected = post_json(
                config,
                bridge.bound_http_port(),
                trade_command("trade-pending-shutdown-b", "idem-pending-shutdown-b"),
                1);
        } catch (...) {
            second_failed.store(true);
        }
        second_done.store(true);
    });

    for (int i = 0; i < 20 && !second_done.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    EXPECT_EQ(signal_callbacks.load(), 1);
    if (second_done.load() && !second_failed.load()) {
        EXPECT_EQ(rejected.at("error").at("data").at("code").get<std::string>(), "server_stopping");
    }

    {
        std::lock_guard<std::mutex> lock(mutex);
        release_first_callback = true;
    }
    cv.notify_all();

    if (first_request.joinable()) {
        first_request.join();
    }
    if (second_request.joinable()) {
        second_request.join();
    }

    EXPECT_EQ(first_response.at("result").at("status").get<std::string>(), "accepted");
    EXPECT_EQ(signal_callbacks.load(), 1);
    bridge.shutdown();
    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));
    bridge.shutdown();
}
#endif

TEST(BridgeProtocolServerBridge, ShutdownRacingStartupLeavesNoRunningTransport) {
    namespace proto = optionx::bridges::protocol_v1;

    auto config = test_config();
    config.enable_websocket = false;

    for (int attempt = 0; attempt < 20; ++attempt) {
        proto::BridgeProtocolServerBridge bridge;
        ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolServerConfig>(config)));

        std::atomic<bool> run_entered{false};
        std::thread run_thread([&]() {
            run_entered.store(true);
            bridge.run();
        });
        for (int i = 0; i < 50 && !run_entered.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));

        bridge.shutdown();
        if (run_thread.joinable()) {
            run_thread.join();
        }
        for (int i = 0; i < 50 && bridge.bound_http_port() != 0; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        EXPECT_EQ(bridge.bound_http_port(), 0);

        bridge.run();
        ASSERT_TRUE(wait_for_http_port(bridge));
        bridge.shutdown();
    }
}

TEST(BridgeProtocolServerBridge, StatusCallbackExceptionDoesNotStopLifecycle) {
    namespace proto = optionx::bridges::protocol_v1;

    auto config = test_config();
    config.enable_websocket = false;

    proto::BridgeProtocolServerBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolServerConfig>(config)));
    bridge.on_status_update() = [](const optionx::BridgeStatusUpdate&) {
        throw std::runtime_error("status callback boom");
    };

    EXPECT_NO_THROW(bridge.run());
    ASSERT_TRUE(wait_for_http_port(bridge));
    EXPECT_NO_THROW(bridge.shutdown());
}

TEST(BridgeProtocolServerBridge, ShutdownCanBeRequestedFromStatusCallback) {
    namespace proto = optionx::bridges::protocol_v1;

    auto config = test_config();
    config.enable_websocket = false;

    proto::BridgeProtocolServerBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolServerConfig>(config)));

    std::atomic<bool> requested_shutdown{false};
    bridge.on_status_update() = [&](const optionx::BridgeStatusUpdate& update) {
        if (update.status == optionx::BridgeStatus::SERVER_STARTED &&
            !requested_shutdown.exchange(true)) {
            bridge.shutdown();
        }
    };

    bridge.run();
    for (int i = 0; i < 200 && !requested_shutdown.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    bridge.shutdown();

    EXPECT_TRUE(requested_shutdown.load());
}

TEST(BridgeProtocolServerBridge, ShutdownFromServerStartedCallbackJoinsBeforeRestart) {
    namespace proto = optionx::bridges::protocol_v1;

    for (int attempt = 0; attempt < 25; ++attempt) {
        auto config = test_config();
        config.enable_websocket = false;

        proto::BridgeProtocolServerBridge bridge;
        ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolServerConfig>(config)));

        std::atomic<int> started_port{0};
        std::atomic<bool> requested_shutdown{false};
        bridge.on_status_update() = [&](const optionx::BridgeStatusUpdate& update) {
            if (update.status == optionx::BridgeStatus::SERVER_STARTED &&
                !requested_shutdown.exchange(true)) {
                started_port.store(static_cast<int>(bridge.bound_http_port()));
                bridge.shutdown();
            }
        };

        bridge.run();
        for (int i = 0; i < 500 && !requested_shutdown.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        bridge.shutdown();

        ASSERT_TRUE(requested_shutdown.load());
        const auto restart_port = static_cast<unsigned short>(started_port.load());
        ASSERT_NE(restart_port, 0);

        bridge.on_status_update() = {};
        config.http_port = restart_port;
        ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolServerConfig>(config)));
        bridge.run();
        ASSERT_TRUE(wait_for_http_port(bridge));
        EXPECT_EQ(bridge.bound_http_port(), restart_port);
        bridge.shutdown();
    }
}

TEST(BridgeProtocolServerBridge, StoppedCallbackShutdownIsNoopAndRunStartsNewGeneration) {
    namespace proto = optionx::bridges::protocol_v1;

    auto config = test_config();
    config.enable_websocket = false;

    proto::BridgeProtocolServerBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolServerConfig>(config)));

    std::atomic<int> started_callbacks{0};
    std::atomic<int> stopped_callbacks{0};
    std::atomic<bool> restarted_from_stopped{false};
    bridge.on_status_update() = [&](const optionx::BridgeStatusUpdate& update) {
        if (update.status == optionx::BridgeStatus::SERVER_STARTED) {
            const auto started = ++started_callbacks;
            if (started == 1) {
                bridge.shutdown();
            }
        }
        if (update.status == optionx::BridgeStatus::SERVER_STOPPED) {
            const auto stopped = ++stopped_callbacks;
            if (stopped == 1) {
                bridge.shutdown();
                bridge.run();
                restarted_from_stopped = true;
            }
        }
    };

    EXPECT_NO_THROW(bridge.run());
    for (int i = 0;
         i < 500 &&
         (!restarted_from_stopped.load() || started_callbacks.load() < 2);
         ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    ASSERT_TRUE(restarted_from_stopped.load());
    ASSERT_GE(started_callbacks.load(), 2);
    ASSERT_TRUE(wait_for_http_port(bridge));
    EXPECT_NO_THROW(bridge.shutdown());

    EXPECT_EQ(started_callbacks.load(), 2);
    EXPECT_GE(stopped_callbacks.load(), 1);

    bridge.on_status_update() = {};
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
