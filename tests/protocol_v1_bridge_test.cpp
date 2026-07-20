#include <gtest/gtest.h>

#include <optionx_cpp/bridges.hpp>

#include <client_http.hpp>
#include <client_ws.hpp>

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
    bridge.shutdown();

    EXPECT_EQ(missing_option_response.at("error").at("code").get<int>(), -32602);
    EXPECT_EQ(missing_expiry_response.at("error").at("code").get<int>(), -32602);
    EXPECT_EQ(zero_duration_response.at("error").at("code").get<int>(), -32602);
    EXPECT_EQ(subsecond_duration_response.at("error").at("code").get<int>(), -32602);
    EXPECT_EQ(past_absolute_response.at("error").at("code").get<int>(), -32602);
    EXPECT_EQ(conflicting_expiry_response.at("error").at("code").get<int>(), -32602);
    EXPECT_EQ(mismatched_kind_response.at("error").at("code").get<int>(), -32602);
    EXPECT_EQ(ignored_top_level_alias_response.at("error").at("code").get<int>(), -32602);
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
        {"mode", "PERCENT"},
        {"step", 2},
        {"group_id", 77},
        {"group_hash", "mm-hash"},
        {"group_name", "group-a"}
    };
    const auto response = post_json(config, bridge.bound_http_port(), command);
    bridge.shutdown();

    EXPECT_EQ(response.at("result").at("status").get<std::string>(), "accepted");
    EXPECT_EQ(platform_type, optionx::PlatformType::INTRADE_BAR);
    EXPECT_EQ(mm_type, optionx::MmSystemType::PERCENT);
    EXPECT_EQ(mm_step, 2);
    EXPECT_EQ(mm_group_id, 77);
    EXPECT_EQ(mm_group_hash, "mm-hash");
    EXPECT_EQ(mm_group_name, "group-a");
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

TEST(BridgeProtocolServerBridge, ShutdownCallbacksAreReentrantSafe) {
    namespace proto = optionx::bridges::protocol_v1;

    auto config = test_config();

    proto::BridgeProtocolServerBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<proto::BridgeProtocolServerConfig>(config)));

    std::atomic<int> started_callbacks{0};
    std::atomic<int> stopped_callbacks{0};
    std::atomic<bool> restarted_once{false};
    bridge.on_status_update() = [&](const optionx::BridgeStatusUpdate& update) {
        if (update.status == optionx::BridgeStatus::SERVER_STARTED) {
            ++started_callbacks;
            bridge.shutdown();
        }
        if (update.status == optionx::BridgeStatus::SERVER_STOPPED) {
            ++stopped_callbacks;
            if (!restarted_once.exchange(true)) {
                bridge.shutdown();
                bridge.run();
            }
        }
    };

    EXPECT_NO_THROW(bridge.run());
    for (int i = 0; i < 200 && stopped_callbacks.load() == 0; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    EXPECT_NO_THROW(bridge.shutdown());

    EXPECT_GT(started_callbacks.load(), 0);
    EXPECT_GE(stopped_callbacks.load(), 1);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
