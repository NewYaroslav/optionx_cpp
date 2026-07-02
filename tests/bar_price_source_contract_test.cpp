#include <gtest/gtest.h>

#include "optionx_cpp/data.hpp"

#include <vector>

TEST(BarPriceSourceContract, HistoryRequestDefaultsToMidPrice) {
    optionx::BarHistoryRequest request("EUR/USD", 60, 1000, 2000);

    EXPECT_EQ(request.symbol, "EUR/USD");
    EXPECT_EQ(request.timeframe, 60);
    EXPECT_EQ(request.from_ts, 1000);
    EXPECT_EQ(request.to_ts, 2000);
    EXPECT_EQ(request.price_source, optionx::BarPriceSource::MID);
}

TEST(BarPriceSourceContract, HistoryRequestCanSelectBidOrAsk) {
    const optionx::BarHistoryRequest bid_request(
        "EUR/USD",
        60,
        1000,
        2000,
        optionx::BarPriceSource::BID);
    const optionx::BarHistoryRequest ask_request(
        "EUR/USD",
        60,
        1000,
        2000,
        optionx::BarPriceSource::ASK);

    EXPECT_EQ(bid_request.price_source, optionx::BarPriceSource::BID);
    EXPECT_EQ(ask_request.price_source, optionx::BarPriceSource::ASK);
}

TEST(BarPriceSourceContract, BarDataAndSequenceCarryPriceSource) {
    optionx::Bar bar(1.0, 1.2, 0.9, 1.1, 0.0, 123000);

    const optionx::BarData default_data(bar, "EUR/USD", "test", 60, 0, 5, 0);
    const optionx::BarData bid_data(
        bar,
        "EUR/USD",
        "test",
        60,
        0,
        5,
        0,
        optionx::BarPriceSource::BID);
    const optionx::BarSequence last_sequence(
        std::vector<optionx::Bar>{bar},
        "BTCUSDT",
        "test",
        60,
        0,
        2,
        0,
        optionx::BarPriceSource::LAST);

    EXPECT_EQ(default_data.price_source, optionx::BarPriceSource::MID);
    EXPECT_EQ(bid_data.price_source, optionx::BarPriceSource::BID);
    EXPECT_EQ(last_sequence.price_source, optionx::BarPriceSource::LAST);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
