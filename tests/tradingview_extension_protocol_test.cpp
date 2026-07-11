#include <gtest/gtest.h>

#include <optionx_cpp/bridges/TradingView/detail/TradingViewExtensionProtocol.hpp>

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
        {"secret", "test-secret"},
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

TEST(TradingViewExtensionProtocol, ConfigRoundTripsJson) {
    TradingViewExtensionBridgeConfig config;
    config.address = "127.0.0.1";
    config.port = 0;
    config.signal_path = "/tv/signal";
    config.health_path = "/tv/health";
    config.bridge_id = 9;
    config.secret = "shared";
    config.sizing_mode = "balance_percent";
    config.balance_percent = 2.5;
    config.min_amount = 1.0;
    config.max_amount = 10.0;
    config.duration = 90;
    config.symbol_map["FX:EURUSD"] = "EURUSD";
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
    EXPECT_EQ(restored.sizing_mode, "balance_percent");
    EXPECT_DOUBLE_EQ(restored.balance_percent, 2.5);
    EXPECT_EQ(restored.level_alert_rules.size(), 1u);
    EXPECT_EQ(restored.level_alert_rules.front().action, "sell");
    EXPECT_TRUE(restored.validate().first);

    restored.bridge_id = 0;
    EXPECT_FALSE(restored.validate().first);
}

TEST(TradingViewExtensionProtocol, ParsesIndicatorBuySignal) {
    auto config = base_config();

    const nlohmann::json payload = {
        {"secret", "test-secret"},
        {"source", "tradingview"},
        {"signal_name", "noisy_rsi_test"},
        {"action", "buy"},
        {"symbol", "FX:EURUSD"},
        {"tickerid", "FX:EURUSD"},
        {"price", 1.14055},
        {"time", 1783476660000LL},
        {"event_id", "indicator:eurusd:1783476660000:buy"}
    };

    auto result = tv_protocol::parse_extension_payload(payload, config);

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
        config);

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
        config);

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.reason, "ignored_state_message");
    EXPECT_FALSE(result.signal);
}

TEST(TradingViewExtensionProtocol, RejectsInvalidSecret) {
    auto config = base_config();

    const nlohmann::json payload = {
        {"secret", "wrong-secret"},
        {"action", "buy"},
        {"symbol", "FX:EURUSD"}
    };

    auto result = tv_protocol::parse_extension_payload(payload, config);

    EXPECT_FALSE(result.accepted);
    EXPECT_FALSE(result.authorized);
    EXPECT_EQ(result.reason, "invalid_secret");
    EXPECT_FALSE(result.signal);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
