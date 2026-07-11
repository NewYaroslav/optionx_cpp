#include <gtest/gtest.h>

#include <optionx_cpp/bridges.hpp>
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

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
