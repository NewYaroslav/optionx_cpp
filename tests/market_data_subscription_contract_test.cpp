#include <gtest/gtest.h>

#include <type_traits>

#include <optionx_cpp/market_data.hpp>

using namespace optionx;
using namespace optionx::market_data;

TEST(TickSubscriptionRequest, BuildsAndValidatesTickRequests) {
    const TickSubscriptionRequest request("EUR/USD", MarketDataTransport::WEBSOCKET);

    EXPECT_TRUE(request.valid());
    EXPECT_EQ(request.symbol, "EUR/USD");
    EXPECT_EQ(request.transport, MarketDataTransport::WEBSOCKET);
    EXPECT_EQ(to_str(request.transport), std::string("WEBSOCKET"));

    EXPECT_FALSE(TickSubscriptionRequest("").valid());
}

TEST(BarSubscriptionRequest, BuildsAndValidatesBarRequests) {
    const BarSubscriptionRequest request(
        "BTCUSDT",
        86400,
        BarPriceSource::LAST,
        MarketDataTransport::HYBRID);

    EXPECT_TRUE(request.valid());
    EXPECT_EQ(request.symbol, "BTCUSDT");
    EXPECT_EQ(request.timeframe, 86400);
    EXPECT_EQ(request.price_source, BarPriceSource::LAST);
    EXPECT_EQ(request.transport, MarketDataTransport::HYBRID);

    EXPECT_FALSE(BarSubscriptionRequest("EUR/USD", 0).valid());
    EXPECT_FALSE(BarSubscriptionRequest("EUR/USD", -16).valid());
    EXPECT_FALSE(BarSubscriptionRequest("", 60).valid());
}

TEST(MarketDataSubscriptionHandle, BuildsTickAndBarHandlesAndReportsValidity) {
    const TickSubscriptionRequest tick_request("EUR/USD", MarketDataTransport::POLLING);
    const auto tick_handle =
        MarketDataSubscriptionHandle::from_tick_request(11, 42, tick_request);

    EXPECT_TRUE(tick_handle.valid());
    EXPECT_EQ(tick_handle.provider_id, 11u);
    EXPECT_EQ(tick_handle.id, 42u);
    EXPECT_EQ(tick_handle.symbol, tick_request.symbol);
    EXPECT_EQ(tick_handle.stream_type, MarketDataStreamType::TICKS);
    EXPECT_EQ(tick_handle.timeframe, 0);
    EXPECT_EQ(tick_handle.transport, tick_request.transport);

    const BarSubscriptionRequest bar_request("BTCUSDT", 86400, BarPriceSource::LAST);
    const auto bar_handle =
        MarketDataSubscriptionHandle::from_bar_request(11, 43, bar_request);

    EXPECT_TRUE(bar_handle.valid());
    EXPECT_EQ(bar_handle.provider_id, 11u);
    EXPECT_EQ(bar_handle.id, 43u);
    EXPECT_EQ(bar_handle.symbol, bar_request.symbol);
    EXPECT_EQ(bar_handle.stream_type, MarketDataStreamType::BARS);
    EXPECT_EQ(bar_handle.timeframe, 86400);
    EXPECT_EQ(bar_handle.price_source, bar_request.price_source);

    EXPECT_FALSE(MarketDataSubscriptionHandle{}.valid());
}

TEST(MarketDataSubscriptionResult, DerivesSuccessFromStatus) {
    const TickSubscriptionRequest request("EUR/USD");
    const auto handle =
        MarketDataSubscriptionHandle::from_tick_request(3, 7, request);

    const auto subscribed = MarketDataSubscriptionResult::subscribed(handle);
    EXPECT_TRUE(subscribed);
    EXPECT_TRUE(subscribed.success());
    EXPECT_EQ(subscribed.status, MarketDataSubscriptionStatus::SUBSCRIBED);

    const auto failed = MarketDataSubscriptionResult::failed(
        handle,
        MarketDataSubscriptionStatus::FAILED,
        "network error");
    EXPECT_FALSE(failed);
    EXPECT_FALSE(failed.success());
    EXPECT_EQ(failed.status, MarketDataSubscriptionStatus::FAILED);
}

TEST(MarketDataSubscriptionResult, CannotSubscribeWithInvalidHandle) {
    const auto result =
        MarketDataSubscriptionResult::subscribed(MarketDataSubscriptionHandle{});

    EXPECT_FALSE(result);
    EXPECT_EQ(result.status, MarketDataSubscriptionStatus::INVALID_REQUEST);
    EXPECT_NE(result.error_message.find("invalid"), std::string::npos);
}

TEST(MarketDataSubscriptionResult, FailureFactoryDoesNotAcceptSuccessStatus) {
    const TickSubscriptionRequest request("EUR/USD");
    const auto result = MarketDataSubscriptionResult::failed(
        request,
        MarketDataSubscriptionStatus::SUBSCRIBED,
        "not actually subscribed");

    EXPECT_FALSE(result);
    EXPECT_EQ(result.status, MarketDataSubscriptionStatus::FAILED);
}

TEST(BaseMarketDataProvider, DefaultProviderRejectsTickSubscriptionsWithTypedResult) {
    BaseMarketDataProvider provider;
    MarketDataSubscriptionResult result;
    int callback_count = 0;

    const bool accepted = provider.subscribe_ticks(
        TickSubscriptionRequest("EUR/USD"),
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
        BarSubscriptionRequest("EUR/USD", 0),
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

    const TickSubscriptionRequest request("BTCUSDT");
    const auto handle =
        MarketDataSubscriptionHandle::from_tick_request(provider.provider_id(), 7, request);

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
    EXPECT_EQ(result.subscription.provider_id, handle.provider_id);
    EXPECT_EQ(result.subscription.id, handle.id);
}

TEST(BaseMarketDataProvider, RejectsHandleFromAnotherProvider) {
    BaseMarketDataProvider provider_a;
    BaseMarketDataProvider provider_b;
    MarketDataSubscriptionResult result;

    const TickSubscriptionRequest request("BTCUSDT");
    const auto handle_from_a =
        MarketDataSubscriptionHandle::from_tick_request(provider_a.provider_id(), 9, request);

    const bool accepted = provider_b.unsubscribe(
        handle_from_a,
        [&result](MarketDataSubscriptionResult subscription_result) {
            result = std::move(subscription_result);
        });

    EXPECT_FALSE(accepted);
    EXPECT_FALSE(result);
    EXPECT_EQ(result.status, MarketDataSubscriptionStatus::WRONG_PROVIDER);
    EXPECT_EQ(to_str(result.status), std::string("WRONG_PROVIDER"));
}

TEST(BaseMarketDataProvider, IsNotCopyableOrMovable) {
    static_assert(!std::is_copy_constructible<BaseMarketDataProvider>::value,
                  "provider identity must not be copy-constructible");
    static_assert(!std::is_copy_assignable<BaseMarketDataProvider>::value,
                  "provider identity must not be copy-assignable");
    static_assert(!std::is_move_constructible<BaseMarketDataProvider>::value,
                  "provider identity must not be move-constructible");
    static_assert(!std::is_move_assignable<BaseMarketDataProvider>::value,
                  "provider identity must not be move-assignable");
    SUCCEED();
}

TEST(BarTimeframe, SupportsDailyTimeframeWithoutTruncation) {
    const Bar bar(1.0, 2.0, 0.5, 1.5, 10.0, 1000);
    const SingleBar single_bar(
        bar,
        "EURUSD",
        "test",
        86400,
        0,
        5,
        0);

    EXPECT_EQ(single_bar.timeframe, 86400);

    BarSequence sequence;
    sequence.timeframe = 86400;
    EXPECT_EQ(sequence.timeframe, 86400);

    const BarHistoryRequest history_request("EURUSD", 86400, 1000, 2000);
    EXPECT_EQ(history_request.timeframe, 86400);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
