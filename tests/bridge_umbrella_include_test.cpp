#include <gtest/gtest.h>

#include <optionx_cpp/bridges.hpp>
#include <optionx_cpp/bridges/metatrader_file.hpp>
#include <optionx_cpp/bridges/named_pipe.hpp>
#include <optionx_cpp/bridges/protocol_v1.hpp>
#include <optionx_cpp/bridges/trading_view.hpp>

TEST(BridgeUmbrellaIncludeTest, ExposesNamedPipeBridgeFamily) {
    optionx::bridges::named_pipe::LegacyTradingBridgeConfig config;

    EXPECT_EQ(
        config.bridge_type(),
        optionx::BridgeType::LEGACY_TRADING_NAMED_PIPE);
}

TEST(BridgeUmbrellaIncludeTest, ExposesTradingViewBridgeFamily) {
    optionx::bridges::tradingview::TradingViewExtensionBridgeConfig config;

    EXPECT_EQ(
        config.bridge_type(),
        optionx::BridgeType::TRADING_VIEW_EXTENSION_HTTP);
}

TEST(BridgeUmbrellaIncludeTest, ExposesMetaTraderFileBridgeFamily) {
    optionx::bridges::metatrader_file::MetaTraderFileBridgeConfig config;
    optionx::bridges::metatrader_file::MetaTraderFileCommandWriter writer(config);
    optionx::bridges::metatrader_file::MetaTraderFileTradeCommand command;
    optionx::bridges::metatrader_file::MetaTraderFileBridge bridge;

    EXPECT_EQ(
        config.bridge_type(),
        optionx::BridgeType::METATRADER_FILE_TRANSPORT);
    EXPECT_EQ(writer.layout().commands_log().filename(), "commands.ndjson");
    EXPECT_EQ(command.amount_value, "1.00");
    EXPECT_EQ(bridge.client_root(), std::filesystem::path());
}

TEST(BridgeUmbrellaIncludeTest, ExposesBotBinaryAdapterHelpers) {
    const auto prepared = optionx::bridges::bot_binary::prepare_bot_binary_command(
        optionx::bridges::bot_binary::bot_binary_duration_command(
            "R_25",
            optionx::OrderType::BUY,
            "1.00",
            60,
            "umbrella-idem"));

    EXPECT_EQ(prepared.request_query_value, "R_25=CALL=1.00=duration=1=m=");

    const auto parsed = optionx::bridges::bot_binary::parse_bot_binary_request_value(
        prepared.request_query_value);
    EXPECT_EQ(parsed.symbol, "R_25");
    EXPECT_EQ(parsed.order_type, optionx::OrderType::BUY);
}

TEST(BridgeUmbrellaIncludeTest, ExposesBridgeProtocolV1ServerFamily) {
    optionx::bridges::protocol_v1::BridgeProtocolServerConfig config;
    optionx::bridges::protocol_v1::BridgeProtocolServerBridge bridge;

    EXPECT_EQ(
        config.bridge_type(),
        optionx::BridgeType::BRIDGE_PROTOCOL_V1_HTTP_WEBSOCKET);
    EXPECT_EQ(bridge.bound_http_port(), 0);
    EXPECT_EQ(bridge.bound_websocket_port(), 0);
}

TEST(BridgeUmbrellaIncludeTest, ExposesSignalReportApi) {
    optionx::BridgeSignalReport report;
    report.status = optionx::BridgeSignalReportStatus::REJECTED;
    report.reason_code = "test_rejection";

    EXPECT_EQ(std::string(optionx::to_str(report.status)), "REJECTED");
    EXPECT_EQ(report.reason_code, "test_rejection");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
