#include <gtest/gtest.h>

#include <optionx_cpp/data.hpp>

using namespace optionx;

TEST(TradeResultQuery, DetectsLocalAndBrokerIdentities) {
    TradeResultQuery query;

    EXPECT_FALSE(query.has_local_identity());
    EXPECT_FALSE(query.has_broker_identity());
    EXPECT_EQ(query.retry_attempts, 15);

    query.trade_id = 42;
    EXPECT_TRUE(query.has_local_identity());
    EXPECT_FALSE(query.has_broker_identity());

    query.option_id = 123;
    EXPECT_TRUE(query.has_broker_identity());

    query.option_id = 0;
    query.option_hash = "broker-hash";
    EXPECT_TRUE(query.has_broker_identity());
}

TEST(TradeResultQuery, RoundTripsJson) {
    TradeResultQuery query;
    query.trade_id = 42;
    query.option_id = 123;
    query.option_hash = "hash";
    query.retry_attempts = 3;

    nlohmann::json json = query;
    const auto restored = json.get<TradeResultQuery>();

    EXPECT_EQ(restored.trade_id, query.trade_id);
    EXPECT_EQ(restored.option_id, query.option_id);
    EXPECT_EQ(restored.option_hash, query.option_hash);
    EXPECT_EQ(restored.retry_attempts, query.retry_attempts);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
