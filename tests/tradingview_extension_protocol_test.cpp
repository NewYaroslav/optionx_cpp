#include <gtest/gtest.h>

#include <optionx_cpp/bridges/TradingView/detail/TradingViewExtensionProtocol.hpp>
#include <optionx_cpp/utils/json_comments.hpp>

namespace {

using optionx::bridges::tradingview::TradingViewExtensionBridgeConfig;
using optionx::bridges::tradingview::TradingViewLevelAlertRule;
namespace tv_protocol = optionx::bridges::tradingview::detail;

TradingViewExtensionBridgeConfig base_config() {
    TradingViewExtensionBridgeConfig config;
    config.bridge_id = 77;
    config.secret = "test-secret";
    config.fixed_amount = 12.5;
    config.duration = 120;
    config.min_payout = 0.65;
    config.symbol_map["FX:EURUSD"] = "EURUSD";
    return config;
}

nlohmann::json price_alert_payload(
        const std::string& method,
        nlohmann::json payload) {
    return nlohmann::json{
        {"text", {
            {"channel", "pricealerts"},
            {"content", {
                {"m", method},
                {"p", std::move(payload)}
            }}
        }}
    };
}

} // namespace

TEST(JsonComments, StripsCommentsOutsideStrings) {
    const std::string text = R"json(
        {
          // endpoint comment
          "url": "http://127.0.0.1:6560/api//v1#fragment",
          "value": 7, /* block comment */
          # hash comment
          "hash": "value # not a comment"
        }
    )json";

    const auto parsed = optionx::utils::parse_json_with_comments(text);

    EXPECT_EQ(parsed.at("url"), "http://127.0.0.1:6560/api//v1#fragment");
    EXPECT_EQ(parsed.at("value"), 7);
    EXPECT_EQ(parsed.at("hash"), "value # not a comment");
}

TEST(TradingViewExtensionProtocol, ConfigRoundTripsJson) {
    TradingViewExtensionBridgeConfig config;
    config.address = "127.0.0.1";
    config.port = 0;
    config.signal_path = "/tv/signal";
    config.health_path = "/tv/health";
    config.bridge_id = 9;
    config.secret = "shared";
    config.allowed_origin = "chrome-extension://abc123";
    config.allow_body_secret_fallback = false;
    config.sizing_mode = "balance_percent";
    config.balance_percent = 2.5;
    config.min_amount = 1.0;
    config.max_amount = 10.0;
    config.duration = 90;
    config.symbol_map["FX:EURUSD"] = "EURUSD";
    config.use_default_action_keywords = false;
    config.buy_action_keywords = {"entry-long"};
    config.sell_action_keywords = {"entry-short"};
    config.level_alert_rules.push_back(
        TradingViewLevelAlertRule{
            "123",
            "FX:EURUSD",
            "crossing_down",
            {},
            {},
            "sell",
            "eurusd_level_sell"});

    nlohmann::json json;
    config.to_json(json);

    TradingViewExtensionBridgeConfig restored;
    restored.from_json(json);

    EXPECT_EQ(restored.bridge_type(), optionx::BridgeType::TRADING_VIEW_EXTENSION_HTTP);
    EXPECT_EQ(restored.port, 0);
    EXPECT_EQ(restored.signal_path, "/tv/signal");
    EXPECT_EQ(restored.allowed_origin, "chrome-extension://abc123");
    EXPECT_FALSE(restored.allow_body_secret_fallback);
    EXPECT_EQ(restored.sizing_mode, "balance_percent");
    EXPECT_DOUBLE_EQ(restored.balance_percent, 2.5);
    EXPECT_FALSE(restored.use_default_action_keywords);
    ASSERT_EQ(restored.buy_action_keywords.size(), 1u);
    EXPECT_EQ(restored.buy_action_keywords.front(), "entry-long");
    ASSERT_EQ(restored.sell_action_keywords.size(), 1u);
    EXPECT_EQ(restored.sell_action_keywords.front(), "entry-short");
    EXPECT_EQ(restored.level_alert_rules.size(), 1u);
    EXPECT_EQ(restored.level_alert_rules.front().action, "sell");
    EXPECT_TRUE(restored.validate().first);

    restored.bridge_id = 0;
    EXPECT_FALSE(restored.validate().first);
}

TEST(TradingViewExtensionProtocol, ConstantTimeSecretCompareMatchesEquality) {
    EXPECT_TRUE(tv_protocol::protocol::constant_time_equals("secret", "secret"));
    EXPECT_FALSE(tv_protocol::protocol::constant_time_equals("secret", "Secret"));
    EXPECT_FALSE(tv_protocol::protocol::constant_time_equals("secret", "secret1"));
    EXPECT_FALSE(tv_protocol::protocol::constant_time_equals("secret1", "secret"));
}

TEST(TradingViewExtensionProtocol, ParsesIndicatorBuySignal) {
    auto config = base_config();

    const nlohmann::json payload = {
        {"source", "tradingview"},
        {"signal_name", "noisy_rsi_test"},
        {"action", "buy"},
        {"symbol", "FX:EURUSD"},
        {"tickerid", "FX:EURUSD"},
        {"price", 1.14055},
        {"time", 1783476660000LL},
        {"event_id", "indicator:eurusd:1783476660000:buy"}
    };

    auto result =
        tv_protocol::parse_extension_payload(payload, "test-secret", config);

    ASSERT_TRUE(result.accepted);
    ASSERT_TRUE(result.signal);
    EXPECT_EQ(result.signal->bridge_id, 77u);
    EXPECT_EQ(result.signal->symbol, "EURUSD");
    EXPECT_EQ(result.signal->signal_name, "noisy_rsi_test");
    EXPECT_EQ(result.signal->order_type, optionx::OrderType::BUY);
    EXPECT_EQ(result.signal->option_type, optionx::OptionType::SPRINT);
    EXPECT_DOUBLE_EQ(result.signal->amount, 12.5);
    EXPECT_EQ(result.signal->mm_type, optionx::MmSystemType::FIXED);
    EXPECT_EQ(result.signal->duration, 120u);
    EXPECT_EQ(result.signal->min_payout, 0.65);
    EXPECT_NE(result.signal->user_data.find("\"source_kind\":\"tradingview\""), std::string::npos);
}

TEST(TradingViewExtensionProtocol, RejectsBodySecretByDefault) {
    auto config = base_config();

    const nlohmann::json payload = {
        {"secret", "test-secret"},
        {"action", "buy"},
        {"symbol", "FX:EURUSD"}
    };

    auto result = tv_protocol::parse_extension_payload(payload, config);

    EXPECT_FALSE(result.accepted);
    EXPECT_FALSE(result.authorized);
    EXPECT_EQ(result.reason, "invalid_secret");
    EXPECT_FALSE(result.signal);
}

TEST(TradingViewExtensionProtocol, AcceptsBodySecretOnlyWhenFallbackIsEnabled) {
    auto config = base_config();
    config.allow_body_secret_fallback = true;

    const nlohmann::json payload = {
        {"secret", "test-secret"},
        {"action", "buy"},
        {"symbol", "FX:EURUSD"}
    };

    auto result = tv_protocol::parse_extension_payload(payload, config);

    ASSERT_TRUE(result.accepted);
    ASSERT_TRUE(result.signal);
    EXPECT_EQ(result.signal->order_type, optionx::OrderType::BUY);
}

TEST(TradingViewExtensionProtocol, AppliesAlertIdRuleForLevelAlert) {
    auto config = base_config();
    config.level_alert_rules.push_back(
        TradingViewLevelAlertRule{
            "53256556946",
            {},
            {},
            {},
            {},
            "sell",
            "eurusd_level_sell"});

    auto result = tv_protocol::parse_extension_payload(
        price_alert_payload(
            "alert_fired",
            nlohmann::json{
                {"fire_id", 53256556946LL},
                {"symbol", "FX:EURUSD"},
                {"message", "EURUSD Crossing 1.14072"},
                {"price", 1.14072},
                {"time", 1783476705177LL}
            }),
        "test-secret",
        config);

    ASSERT_TRUE(result.accepted);
    ASSERT_TRUE(result.signal);
    EXPECT_EQ(result.signal->symbol, "EURUSD");
    EXPECT_EQ(result.signal->order_type, optionx::OrderType::SELL);
    EXPECT_EQ(result.signal->signal_name, "eurusd_level_sell");
    EXPECT_EQ(result.signal->unique_id, 53256556946LL);
    EXPECT_EQ(result.reason, "accepted");
}

TEST(TradingViewExtensionProtocol, AppliesConditionRuleForLevelAlert) {
    auto config = base_config();
    config.level_alert_rules.push_back(
        TradingViewLevelAlertRule{
            {},
            "FX:EURUSD",
            "crossing_up",
            {},
            {},
            "buy",
            "eurusd_crossing_up"});

    auto result = tv_protocol::parse_extension_payload(
        price_alert_payload(
            "alert_fired",
            nlohmann::json{
                {"fire_id", 53256556947LL},
                {"symbol", "FX:EURUSD"},
                {"message", "EURUSD Crossing Up 1.14072"}
            }),
        "test-secret",
        config);

    ASSERT_TRUE(result.accepted);
    ASSERT_TRUE(result.signal);
    EXPECT_EQ(result.signal->order_type, optionx::OrderType::BUY);
    EXPECT_EQ(result.signal->signal_name, "eurusd_crossing_up");
}

TEST(TradingViewExtensionProtocol, RejectsUnmappedLevelAlert) {
    auto config = base_config();

    auto result = tv_protocol::parse_extension_payload(
        price_alert_payload(
            "alert_fired",
            nlohmann::json{
                {"fire_id", 53256556948LL},
                {"symbol", "FX:EURUSD"},
                {"message", "EURUSD Crossing 1.14072"}
            }),
        "test-secret",
        config);

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.reason, "unmapped_level_alert");
    EXPECT_FALSE(result.signal);
}

TEST(TradingViewExtensionProtocol, MapsLevelAlertActionFromDefaultKeyword) {
    auto config = base_config();

    const nlohmann::json payload = {
        {"source_kind", "alert_toast_dom"},
        {"event_id", "toast:btcusd:buy"},
        {"action", "alert"},
        {"symbol", "BTCUSD"},
        {"message", "BTCUSD Crossing BUY 64,114.82"}
    };

    auto result =
        tv_protocol::parse_extension_payload(payload, "test-secret", config);

    ASSERT_TRUE(result.accepted);
    ASSERT_TRUE(result.signal);
    EXPECT_EQ(result.signal->symbol, "BTCUSD");
    EXPECT_EQ(result.signal->order_type, optionx::OrderType::BUY);
    EXPECT_EQ(result.signal->signal_name, "tradingview_level_alert");
}

TEST(TradingViewExtensionProtocol, ExtendsActionKeywordsWithCustomRussianTerm) {
    auto config = base_config();
    config.use_default_action_keywords = false;
    config.buy_action_keywords = {u8"\u043F\u043E\u043A\u0443\u043F\u0430"};

    const nlohmann::json payload = {
        {"source_kind", "alert_toast_dom"},
        {"event_id", "toast:btcusd:custom-buy"},
        {"action", "alert"},
        {"symbol", "BTCUSD"},
        {"message", u8"BTCUSD Crossing \u041F\u043E\u043A\u0443\u043F\u0430 64,114.82"}
    };

    auto result =
        tv_protocol::parse_extension_payload(payload, "test-secret", config);

    ASSERT_TRUE(result.accepted);
    ASSERT_TRUE(result.signal);
    EXPECT_EQ(result.signal->order_type, optionx::OrderType::BUY);
}

TEST(TradingViewExtensionProtocol, CanDisableDefaultActionKeywords) {
    auto config = base_config();
    config.use_default_action_keywords = false;

    const nlohmann::json payload = {
        {"source_kind", "alert_toast_dom"},
        {"event_id", "toast:btcusd:disabled-keyword"},
        {"action", "alert"},
        {"symbol", "BTCUSD"},
        {"message", "BTCUSD Crossing BUY 64,114.82"}
    };

    auto result =
        tv_protocol::parse_extension_payload(payload, "test-secret", config);

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.reason, "unmapped_level_alert");
    EXPECT_FALSE(result.signal);
}

TEST(TradingViewExtensionProtocol, IgnoresPriceAlertLifecycleMessages) {
    auto config = base_config();

    auto result = tv_protocol::parse_extension_payload(
        price_alert_payload(
            "alerts_updated",
            nlohmann::json::array({
                nlohmann::json{
                    {"alert_id", 123},
                    {"active", false},
                    {"message", "EURUSD Crossing 1.14072"}
                }
            })),
        "test-secret",
        config);

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.reason, "ignored_state_message");
    EXPECT_FALSE(result.signal);
}

TEST(TradingViewExtensionProtocol, RejectsInvalidSecret) {
    auto config = base_config();

    const nlohmann::json payload = {
        {"action", "buy"},
        {"symbol", "FX:EURUSD"}
    };

    auto result =
        tv_protocol::parse_extension_payload(payload, "wrong-secret", config);

    EXPECT_FALSE(result.accepted);
    EXPECT_FALSE(result.authorized);
    EXPECT_EQ(result.reason, "invalid_secret");
    EXPECT_FALSE(result.signal);
}

TEST(TradingViewExtensionProtocol, PreservesFingerprintAsDedupeFallback) {
    auto config = base_config();

    const nlohmann::json payload = {
        {"source_kind", "alert_toast_dom"},
        {"fingerprint", "fnv1a:abc123"},
        {"action", "buy"},
        {"symbol", "FX:EURUSD"}
    };

    auto result =
        tv_protocol::parse_extension_payload(payload, "test-secret", config);

    ASSERT_TRUE(result.accepted);
    ASSERT_TRUE(result.signal);
    EXPECT_EQ(result.dedupe_key, "fnv1a:abc123");
    EXPECT_NE(result.signal->user_data.find("\"fingerprint\":\"fnv1a:abc123\""), std::string::npos);
}

TEST(TradingViewExtensionProtocol, PrefersEventIdOverFingerprint) {
    auto config = base_config();

    const nlohmann::json payload = {
        {"source_kind", "alert_toast_dom"},
        {"event_id", "tv_toast:event-123"},
        {"fingerprint", "fnv1a:abc123"},
        {"action", "buy"},
        {"symbol", "FX:EURUSD"}
    };

    auto result =
        tv_protocol::parse_extension_payload(payload, "test-secret", config);

    ASSERT_TRUE(result.accepted);
    EXPECT_EQ(result.event_id, "tv_toast:event-123");
    EXPECT_EQ(result.dedupe_key, "tv_toast:event-123");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
