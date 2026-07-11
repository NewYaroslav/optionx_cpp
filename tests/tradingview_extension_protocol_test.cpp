#include <gtest/gtest.h>

#include <optionx_cpp/bridges/trading_view.hpp>
#include <optionx_cpp/utils/json_comments.hpp>

#include <client_http.hpp>

#include <chrono>
#include <mutex>
#include <thread>

namespace {

using optionx::bridges::tradingview::TradingViewExtensionBridgeConfig;
using optionx::bridges::tradingview::TradingViewLevelAlertRule;
using HttpClient = SimpleWeb::Client<SimpleWeb::HTTP>;
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

bool wait_for_port(const optionx::bridges::tradingview::TradingViewExtensionBridge& bridge) {
    for (int i = 0; i < 200; ++i) {
        if (bridge.bound_port() != 0) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

struct BridgeGuard {
    optionx::bridges::tradingview::TradingViewExtensionBridge& bridge;
    ~BridgeGuard() { bridge.shutdown(); }
};

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
    config.study_alert_mode = "confirmed_only";
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
    EXPECT_FALSE(json.contains("health_path"));
    EXPECT_EQ(restored.allowed_origin, "chrome-extension://abc123");
    EXPECT_FALSE(restored.allow_body_secret_fallback);
    EXPECT_EQ(restored.sizing_mode, "balance_percent");
    EXPECT_DOUBLE_EQ(restored.balance_percent, 2.5);
    EXPECT_FALSE(restored.use_default_action_keywords);
    ASSERT_EQ(restored.buy_action_keywords.size(), 1u);
    EXPECT_EQ(restored.buy_action_keywords.front(), "entry-long");
    ASSERT_EQ(restored.sell_action_keywords.size(), 1u);
    EXPECT_EQ(restored.sell_action_keywords.front(), "entry-short");
    EXPECT_EQ(restored.study_alert_mode, "confirmed_only");
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

TEST(TradingViewExtensionProtocol, ParsesChartStudyIndicatorSignal) {
    auto config = base_config();

    const nlohmann::json payload = {
        {"version", 1},
        {"source", "tradingview_extension"},
        {"source_kind", "private_chart_study_alert_messages"},
        {"method", "du.alertMessages"},
        {"event_id", "tv_study_alert:abc123"},
        {"dedupe_key", "tv_study_alert:abc123"},
        {"fingerprint", "abc123"},
        {"chart_session", "cs_test"},
        {"study_id", "8x94yO"},
        {"signal_name", "noisy_rsi_test"},
        {"action", "sell"},
        {"symbol", "BTCUSD"},
        {"tickerid", "CRYPTO:BTCUSD"},
        {"price", 64131.92},
        {"time", 1783763820000LL},
        {"bar_time", 1783763820000LL},
        {"update_time", 1783763881395LL},
        {"bar_state", "RT_CONFIRMED"},
        {"bar_state_source", "debug_idx"}
    };

    auto result =
        tv_protocol::parse_extension_payload(payload, "test-secret", config);

    ASSERT_TRUE(result.accepted);
    ASSERT_TRUE(result.signal);
    EXPECT_EQ(result.event_id, "tv_study_alert:abc123");
    EXPECT_EQ(result.signal->unique_hash, "tv_study_alert:abc123");
    EXPECT_EQ(result.signal->symbol, "BTCUSD");
    EXPECT_EQ(result.signal->signal_name, "noisy_rsi_test");
    EXPECT_EQ(result.signal->order_type, optionx::OrderType::SELL);
    EXPECT_EQ(result.parsed_payload.at("bar_state"), "RT_CONFIRMED");
    EXPECT_EQ(result.parsed_payload.at("bar_state_source"), "debug_idx");
    EXPECT_NE(
        result.signal->user_data.find("\"source_kind\":\"private_chart_study_alert_messages\""),
        std::string::npos);
    EXPECT_NE(
        result.signal->user_data.find("\"bar_state\":\"RT_CONFIRMED\""),
        std::string::npos);
}

TEST(TradingViewExtensionProtocol, RejectsRealtimeStudyAlertWhenConfirmedOnly) {
    auto config = base_config();
    config.study_alert_mode = "confirmed_only";

    const nlohmann::json payload = {
        {"source_kind", "private_chart_study_alert_messages"},
        {"method", "du.alertMessages"},
        {"event_id", "tv_study_alert:rt"},
        {"signal_name", "noisy_rsi_test"},
        {"action", "buy"},
        {"symbol", "BTCUSD"},
        {"price", 64130.49},
        {"time", 1783764000000LL},
        {"bar_state", "RT_CONFIRMED"},
        {"bar_state_source", "debug_idx"}
    };

    auto result =
        tv_protocol::parse_extension_payload(payload, "test-secret", config);

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.reason, "unconfirmed_study_alert");
    EXPECT_FALSE(result.signal);
    ASSERT_TRUE(result.parsed_payload.is_object());
    EXPECT_EQ(result.parsed_payload.at("bar_state"), "RT_CONFIRMED");
}

TEST(TradingViewExtensionProtocol, AcceptsHistoricStudyAlertWhenConfirmedOnly) {
    auto config = base_config();
    config.study_alert_mode = "confirmed_only";

    const nlohmann::json payload = {
        {"source_kind", "private_chart_study_alert_messages"},
        {"method", "du.alertMessages"},
        {"event_id", "tv_study_alert:hist"},
        {"signal_name", "noisy_rsi_test"},
        {"action", "buy"},
        {"symbol", "BTCUSD"},
        {"price", 64131.92},
        {"time", 1783763820000LL},
        {"bar_state", "HIST_CONFIRMED"},
        {"bar_state_source", "debug_last"}
    };

    auto result =
        tv_protocol::parse_extension_payload(payload, "test-secret", config);

    ASSERT_TRUE(result.accepted);
    ASSERT_TRUE(result.signal);
    EXPECT_EQ(result.signal->order_type, optionx::OrderType::BUY);
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
    ASSERT_TRUE(result.raw_payload.is_object());
    EXPECT_EQ(result.raw_payload.at("secret"), "[redacted]");
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
    EXPECT_EQ(result.parsed_payload.at("source_kind"), "private_pricealerts_ws");
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
    EXPECT_DOUBLE_EQ(result.parsed_payload.at("price").get<double>(), 1.14072);
}

TEST(TradingViewExtensionProtocol, ParsesForwardedPrivateFeedPayload) {
    auto config = base_config();
    config.level_alert_rules.push_back(
        TradingViewLevelAlertRule{
            {},
            "FX:EURUSD",
            "crossing",
            {},
            {},
            "buy",
            "private_pricealert"});

    auto payload = price_alert_payload(
        "alert_fired",
        nlohmann::json{
            {"fire_id", 53256556946LL},
            {"alert_id", 5099741779LL},
            {"symbol", "={\"symbol\":\"FX:EURUSD\",\"adjustment\":\"splits\"}"},
            {"message", "EURUSD Crossing 1.14072"},
            {"fire_time", "2026-07-08T02:36:22Z"}
        });
    payload["source_kind"] = "private_pricealerts_ws";
    payload["event_id"] = "tv_price_alert:53256556946";
    payload["dedupe_key"] = "tv_price_alert:53256556946";
    payload["fingerprint"] = "b9d62011";

    auto result =
        tv_protocol::parse_extension_payload(payload, "test-secret", config);

    ASSERT_TRUE(result.accepted);
    ASSERT_TRUE(result.signal);
    EXPECT_EQ(result.event_id, "tv_price_alert:53256556946");
    EXPECT_EQ(result.dedupe_key, "tv_price_alert:53256556946");
    EXPECT_EQ(result.signal->unique_hash, "tv_price_alert:53256556946");
    EXPECT_EQ(result.signal->symbol, "EURUSD");
    EXPECT_EQ(result.signal->signal_name, "private_pricealert");
    EXPECT_EQ(result.signal->order_type, optionx::OrderType::BUY);
    EXPECT_EQ(result.parsed_payload.at("source_kind"), "private_pricealerts_ws");
    EXPECT_DOUBLE_EQ(result.parsed_payload.at("price").get<double>(), 1.14072);
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
    ASSERT_TRUE(result.parsed_payload.is_object());
    EXPECT_EQ(result.parsed_payload.at("symbol"), "EURUSD");
    EXPECT_EQ(result.parsed_payload.at("condition_type"), "crossing");
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

TEST(TradingViewExtensionProtocol, DoesNotMapDirectionalLevelWordsByDefault) {
    auto config = base_config();

    const nlohmann::json payload = {
        {"source_kind", "alert_toast_dom"},
        {"event_id", "toast:eurusd:crossing-up"},
        {"action", "alert"},
        {"symbol", "EURUSD"},
        {"message", "EURUSD Crossing Up 1.1400"}
    };

    auto result =
        tv_protocol::parse_extension_payload(payload, "test-secret", config);

    EXPECT_FALSE(result.accepted);
    EXPECT_EQ(result.reason, "unmapped_level_alert");
    EXPECT_FALSE(result.signal);
}

TEST(TradingViewExtensionProtocol, MatchesDefaultRussianKeywordsCaseInsensitively) {
    auto config = base_config();

    const nlohmann::json payload = {
        {"source_kind", "alert_toast_dom"},
        {"event_id", "toast:btcusd:russian-buy"},
        {"action", "alert"},
        {"symbol", "BTCUSD"},
        {"message", u8"BTCUSD Crossing \u0411\u0410\u0419 64,143.35"}
    };

    auto result =
        tv_protocol::parse_extension_payload(payload, "test-secret", config);

    ASSERT_TRUE(result.accepted);
    ASSERT_TRUE(result.signal);
    EXPECT_EQ(result.signal->order_type, optionx::OrderType::BUY);
}

TEST(TradingViewExtensionProtocol, MatchesCustomKeywordsWithUnicodeCaseFolding) {
    auto config = base_config();
    config.use_default_action_keywords = false;
    config.buy_action_keywords = {"strasse"};

    const nlohmann::json payload = {
        {"source_kind", "alert_toast_dom"},
        {"event_id", "toast:btcusd:german-fold"},
        {"action", "alert"},
        {"symbol", "BTCUSD"},
        {"message", u8"BTCUSD Crossing Stra\u00DFe 64,143.35"}
    };

    auto result =
        tv_protocol::parse_extension_payload(payload, "test-secret", config);

    ASSERT_TRUE(result.accepted);
    ASSERT_TRUE(result.signal);
    EXPECT_EQ(result.signal->order_type, optionx::OrderType::BUY);
}

TEST(TradingViewExtensionProtocol, ParsesCommaPriceAndIsoTimeFromToastPayload) {
    auto config = base_config();

    const nlohmann::json payload = {
        {"source_kind", "alert_toast_dom"},
        {"event_id", "tv_toast:d8b7589f"},
        {"fingerprint", "d8b7589f"},
        {"action", "alert"},
        {"symbol", "BTCUSD"},
        {"message", "BTCUSD Crossing BUY 64,143.35"},
        {"price", "64,143.35"},
        {"time", "2026-07-11T08:09:45.650Z"}
    };

    auto result =
        tv_protocol::parse_extension_payload(payload, "test-secret", config);

    ASSERT_TRUE(result.accepted);
    ASSERT_TRUE(result.signal);
    EXPECT_EQ(result.signal->order_type, optionx::OrderType::BUY);
    EXPECT_DOUBLE_EQ(result.parsed_payload.at("price").get<double>(), 64143.35);
    EXPECT_EQ(result.parsed_payload.at("time").get<std::int64_t>(), 1783757385650LL);

    const auto user_data = nlohmann::json::parse(result.signal->user_data);
    EXPECT_DOUBLE_EQ(user_data.at("price").get<double>(), 64143.35);
    EXPECT_EQ(user_data.at("time").get<std::int64_t>(), 1783757385650LL);
}

TEST(TradingViewExtensionProtocol, PreservesAlertNameAndUsesItForKeywords) {
    auto config = base_config();

    const nlohmann::json payload = {
        {"source_kind", "alert_toast_dom"},
        {"event_id", "toast:btcusd:title-buy"},
        {"action", "alert"},
        {"symbol", "BTCUSD"},
        {"alert_name", "BUY Test99"},
        {"message", "BTCUSD Crossing 64,119.59"}
    };

    auto result =
        tv_protocol::parse_extension_payload(payload, "test-secret", config);

    ASSERT_TRUE(result.accepted);
    ASSERT_TRUE(result.signal);
    EXPECT_EQ(result.signal->order_type, optionx::OrderType::BUY);
    EXPECT_EQ(result.signal->signal_name, "BUY Test99");
    EXPECT_EQ(result.signal->comment, "BTCUSD Crossing 64,119.59");
    EXPECT_NE(result.signal->user_data.find("\"alert_name\":\"BUY Test99\""), std::string::npos);
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

TEST(TradingViewExtensionBridge, ReportsRejectedLevelAlertOverHttp) {
    auto config = base_config();
    config.port = 0;

    optionx::bridges::tradingview::TradingViewExtensionBridge bridge;
    BridgeGuard guard{bridge};
    ASSERT_TRUE(bridge.configure(std::make_unique<TradingViewExtensionBridgeConfig>(config)));

    bridge.on_signal_id() = [] {
        return optionx::SignalId{100};
    };

    std::mutex reports_mutex;
    std::vector<optionx::BridgeSignalReport> reports;
    bridge.on_signal_report() = [&](const optionx::BridgeSignalReport& report) {
        std::lock_guard<std::mutex> lock(reports_mutex);
        reports.push_back(report);
    };

    bridge.run();
    ASSERT_TRUE(wait_for_port(bridge));

    HttpClient client(config.address + ":" + std::to_string(bridge.bound_port()));
    SimpleWeb::CaseInsensitiveMultimap headers;
    headers.emplace("Content-Type", "application/json");
    headers.emplace("X-OptionX-Secret", config.secret);

    const nlohmann::json payload = {
        {"source_kind", "alert_toast_dom"},
        {"event_id", "toast:btcusd:unmapped"},
        {"action", "alert"},
        {"symbol", "BTCUSD"},
        {"message", "BTCUSD Crossing 64114.82"}
    };

    const auto response =
        client.request("POST", config.signal_path, payload.dump(), headers);
    const auto response_json = nlohmann::json::parse(response->content.string());
    EXPECT_FALSE(response_json.at("accepted").get<bool>());
    EXPECT_EQ(response_json.at("reason"), "unmapped_level_alert");

    std::vector<optionx::BridgeSignalReport> reports_snapshot;
    {
        std::lock_guard<std::mutex> lock(reports_mutex);
        reports_snapshot = reports;
    }

    ASSERT_EQ(reports_snapshot.size(), 1u);
    const auto& report = reports_snapshot.front();
    EXPECT_EQ(report.status, optionx::BridgeSignalReportStatus::REJECTED);
    EXPECT_EQ(report.reason_code, "unmapped_level_alert");
    EXPECT_EQ(report.bridge_id, config.bridge_id);
    EXPECT_EQ(report.bridge_type, optionx::BridgeType::TRADING_VIEW_EXTENSION_HTTP);
    EXPECT_EQ(report.event_id, "toast:btcusd:unmapped");
    EXPECT_EQ(report.dedupe_key, "toast:btcusd:unmapped");
    EXPECT_EQ(report.symbol, "BTCUSD");
    EXPECT_EQ(report.parsed_payload.at("condition_type"), "crossing");
    EXPECT_FALSE(report.candidate_signal);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
