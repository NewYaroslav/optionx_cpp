#include <gtest/gtest.h>

#include <optionx_cpp/market_data.hpp>

using namespace optionx;
using namespace optionx::market_data;

TEST(MarketDataSubscriptionRequest, BuildsAndValidatesTickAndBarRequests) {
    const auto tick_request =
        MarketDataSubscriptionRequest::ticks("EUR/USD", MarketDataTransport::WEBSOCKET);

    EXPECT_TRUE(tick_request.valid());
    EXPECT_EQ(tick_request.symbol, "EUR/USD");
    EXPECT_EQ(tick_request.stream_type, MarketDataStreamType::TICKS);
    EXPECT_EQ(tick_request.transport, MarketDataTransport::WEBSOCKET);
    EXPECT_EQ(to_str(tick_request.stream_type), std::string("TICKS"));
    EXPECT_EQ(to_str(tick_request.transport), std::string("WEBSOCKET"));

    const auto bar_request =
        MarketDataSubscriptionRequest::bars("BTCUSDT", 60, BarPriceSource::LAST, MarketDataTransport::HYBRID);

    EXPECT_TRUE(bar_request.valid());
    EXPECT_EQ(bar_request.symbol, "BTCUSDT");
    EXPECT_EQ(bar_request.stream_type, MarketDataStreamType::BARS);
    EXPECT_EQ(bar_request.timeframe, 60);
    EXPECT_EQ(bar_request.price_source, BarPriceSource::LAST);
    EXPECT_EQ(bar_request.transport, MarketDataTransport::HYBRID);

    auto invalid_bar = MarketDataSubscriptionRequest::bars("EUR/USD", 0);
    EXPECT_FALSE(invalid_bar.valid());

    auto invalid_tick = MarketDataSubscriptionRequest::ticks("");
    EXPECT_FALSE(invalid_tick.valid());
}

TEST(MarketDataSubscriptionHandle, BuildsFromRequestAndReportsValidity) {
    const auto request =
        MarketDataSubscriptionRequest::bars("EUR/USD", 300, BarPriceSource::BID);

    const auto handle = MarketDataSubscriptionHandle::from_request(42, request);

    EXPECT_TRUE(handle.valid());
    EXPECT_EQ(handle.id, 42u);
    EXPECT_EQ(handle.symbol, request.symbol);
    EXPECT_EQ(handle.stream_type, request.stream_type);
    EXPECT_EQ(handle.timeframe, request.timeframe);
    EXPECT_EQ(handle.price_source, request.price_source);

    EXPECT_FALSE(MarketDataSubscriptionHandle{}.valid());
}

TEST(BaseMarketDataProvider, DefaultProviderRejectsTickSubscriptionsWithTypedResult) {
    BaseMarketDataProvider provider;
    MarketDataSubscriptionResult result;
    int callback_count = 0;

    const bool accepted = provider.subscribe_ticks(
        MarketDataSubscriptionRequest::ticks("EUR/USD"),
        [&result, &callback_count](MarketDataSubscriptionResult subscription_result) {
            result = std::move(subscription_result);
            ++callback_count;
        });

    EXPECT_FALSE(accepted);
    EXPECT_EQ(callback_count, 1);
    EXPECT_FALSE(result);
    EXPECT_EQ(result.status, MarketDataSubscriptionStatus::UNSUPPORTED);
    EXPECT_EQ(result.subscription.stream_type, MarketDataStreamType::TICKS);
    EXPECT_EQ(result.subscription.symbol, "EUR/USD");
    EXPECT_NE(result.error_message.find("Tick subscriptions"), std::string::npos);
}

TEST(BaseMarketDataProvider, DefaultProviderRejectsInvalidBarSubscriptions) {
    BaseMarketDataProvider provider;
    MarketDataSubscriptionResult result;
    int callback_count = 0;

    const bool accepted = provider.subscribe_bars(
        MarketDataSubscriptionRequest::bars("EUR/USD", 0),
        [&result, &callback_count](MarketDataSubscriptionResult subscription_result) {
            result = std::move(subscription_result);
            ++callback_count;
        });

    EXPECT_FALSE(accepted);
    EXPECT_EQ(callback_count, 1);
    EXPECT_FALSE(result);
    EXPECT_EQ(result.status, MarketDataSubscriptionStatus::INVALID_REQUEST);
    EXPECT_EQ(result.subscription.stream_type, MarketDataStreamType::BARS);
    EXPECT_EQ(to_str(result.status), std::string("INVALID_REQUEST"));
}

TEST(BaseMarketDataProvider, DefaultProviderRejectsUnsupportedUnsubscribe) {
    BaseMarketDataProvider provider;
    MarketDataSubscriptionResult result;
    int callback_count = 0;

    const auto request = MarketDataSubscriptionRequest::ticks("BTCUSDT");
    const auto handle = MarketDataSubscriptionHandle::from_request(7, request);

    const bool accepted = provider.unsubscribe(
        handle,
        [&result, &callback_count](MarketDataSubscriptionResult subscription_result) {
            result = std::move(subscription_result);
            ++callback_count;
        });

    EXPECT_FALSE(accepted);
    EXPECT_EQ(callback_count, 1);
    EXPECT_FALSE(result);
    EXPECT_EQ(result.status, MarketDataSubscriptionStatus::UNSUPPORTED);
    EXPECT_EQ(result.subscription.id, handle.id);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
