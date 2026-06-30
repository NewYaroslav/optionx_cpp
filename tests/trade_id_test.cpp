#include <gtest/gtest.h>

#include <limits>

#include <optionx_cpp/utils/trade_id.hpp>

namespace {

    class TradeIdCounterGuard {
    public:
        TradeIdCounterGuard()
            : m_saved(optionx::utils::g_trade_id_counter.load(std::memory_order_relaxed)) {}

        ~TradeIdCounterGuard() {
            optionx::utils::g_trade_id_counter.store(m_saved, std::memory_order_relaxed);
        }

        TradeIdCounterGuard(const TradeIdCounterGuard&) = delete;
        TradeIdCounterGuard& operator=(const TradeIdCounterGuard&) = delete;

    private:
        optionx::utils::TradeId m_saved;
    };

} // namespace

TEST(TradeIdTest, RecoversFromInvalidCounterState) {
    TradeIdCounterGuard guard;
    optionx::utils::g_trade_id_counter.store(0, std::memory_order_relaxed);

    EXPECT_EQ(optionx::utils::make_trade_id(), 1u);
    EXPECT_EQ(optionx::utils::make_trade_id(), 2u);
}

TEST(TradeIdTest, WrapsAfterMaxValueWithoutReturningZero) {
    TradeIdCounterGuard guard;
    optionx::utils::g_trade_id_counter.store(
        (std::numeric_limits<optionx::utils::TradeId>::max)(),
        std::memory_order_relaxed);

    EXPECT_EQ(
        optionx::utils::make_trade_id(),
        (std::numeric_limits<optionx::utils::TradeId>::max)());
    EXPECT_EQ(optionx::utils::make_trade_id(), 1u);
    EXPECT_EQ(optionx::utils::make_trade_id(), 2u);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
