#include <gtest/gtest.h>

#include <optionx_cpp/bridges.hpp>
#include <optionx_cpp/bridges/metatrader_file.hpp>
#include <optionx_cpp/bridges/named_pipe.hpp>
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
