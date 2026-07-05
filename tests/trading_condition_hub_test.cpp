#include <gtest/gtest.h>

#include <optionx_cpp/components.hpp>

#include <memory>
#include <string>
#include <vector>

namespace {

class RecordingTradingConditionSubscriber final
        : public optionx::components::ITradingConditionSubscriber {
public:
    void on_trading_condition(
            const optionx::TradingConditionUpdate& update) override {
        updates.push_back(update);
    }

    std::vector<optionx::TradingConditionUpdate> updates;
};

optionx::TradingConditionUpdate make_condition(
        std::string symbol,
        double payout) {
    optionx::TradingConditionUpdate update;
    update.symbol = std::move(symbol);
    update.platform_type = optionx::PlatformType::INTRADE_BAR;
    update.option_type = optionx::OptionType::SPRINT;
    update.payout = payout;
    update.market_open = true;
    return update;
}

} // namespace

TEST(TradingConditionUpdate, ReportsWhetherConditionFieldsArePresent) {
    optionx::TradingConditionUpdate update;
    update.symbol = "EUR/USD";
    EXPECT_TRUE(update.empty());

    update.payout = 0.82;
    EXPECT_FALSE(update.empty());
}

TEST(TradingConditionHub, RoutesUpdatesAndReplaysLatestCachedConditions) {
    optionx::components::TradingConditionHub hub;

    const auto first = std::make_shared<RecordingTradingConditionSubscriber>();
    hub.add_subscriber(first);

    hub.publish(make_condition("EUR/USD", 0.80));
    hub.publish(make_condition("EUR/USD", 0.82));
    hub.publish(make_condition("BTCUSDT", 0.70));

    ASSERT_EQ(first->updates.size(), 3u);
    EXPECT_DOUBLE_EQ(*first->updates[1].payout, 0.82);

    const auto late = std::make_shared<RecordingTradingConditionSubscriber>();
    hub.add_subscriber(late);

    ASSERT_EQ(late->updates.size(), 2u);
    EXPECT_EQ(late->updates[0].symbol, "EUR/USD");
    EXPECT_DOUBLE_EQ(*late->updates[0].payout, 0.82);
    EXPECT_EQ(late->updates[1].symbol, "BTCUSDT");
    EXPECT_DOUBLE_EQ(*late->updates[1].payout, 0.70);
}

TEST(TradingConditionHub, BindsToTradingConditionCallback) {
    optionx::components::TradingConditionHub hub;
    optionx::trading_condition_callback_t callback;

    const auto subscriber =
        std::make_shared<RecordingTradingConditionSubscriber>();
    hub.add_subscriber(subscriber);

    hub.bind_to(callback);
    ASSERT_TRUE(static_cast<bool>(callback));

    callback(make_condition("EUR/USD", 0.83));

    ASSERT_EQ(subscriber->updates.size(), 1u);
    EXPECT_EQ(subscriber->updates[0].symbol, "EUR/USD");
    EXPECT_DOUBLE_EQ(*subscriber->updates[0].payout, 0.83);

    hub.unbind_from(callback);
    EXPECT_FALSE(static_cast<bool>(callback));
}

TEST(BaseTradingConditionHandler, RoutesEventBusUpdatesToCallback) {
    optionx::utils::EventBus bus;
    optionx::components::BaseTradingConditionHandler handler(bus);
    std::vector<optionx::TradingConditionUpdate> received;

    handler.on_trading_condition() =
        [&received](const optionx::TradingConditionUpdate& update) {
            received.push_back(update);
        };

    bus.notify_async(std::make_unique<optionx::events::TradingConditionUpdateEvent>(
        make_condition("BTCUSDT", 0.71)));
    bus.process();

    ASSERT_EQ(received.size(), 1u);
    EXPECT_EQ(received[0].symbol, "BTCUSDT");
    EXPECT_DOUBLE_EQ(*received[0].payout, 0.71);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
