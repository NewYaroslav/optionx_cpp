#include <gtest/gtest.h>

#include <optionx_cpp/data.hpp>
#include <optionx_cpp/platforms.hpp>
#include <optionx_cpp/platforms/TradeUpPlatform/AuthData.hpp>

using namespace optionx;

namespace {

class TestBridgeConfig final : public IBridgeConfig {
public:
    void to_json(nlohmann::json&) const override {}

    void from_json(const nlohmann::json&) override {}

    std::pair<bool, std::string> validate() const override {
        return {true, std::string()};
    }

    std::unique_ptr<IBridgeConfig> clone_unique() const override {
        return std::make_unique<TestBridgeConfig>(*this);
    }

    std::shared_ptr<IBridgeConfig> clone_shared() const override {
        return std::make_shared<TestBridgeConfig>(*this);
    }

    BridgeType bridge_type() const override {
        return BridgeType::UNKNOWN;
    }
};

} // namespace

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

TEST(TradeHistoryRequest, ValidatesRangeModes) {
    TradeHistoryRequest ranged;
    EXPECT_FALSE(ranged.has_valid_range());

    ranged.start_ms = 1000;
    ranged.stop_ms = 2000;
    EXPECT_TRUE(ranged.has_valid_range());

    ranged.range_mode = TimeRangeMode::HALF_OPEN;
    EXPECT_TRUE(ranged.has_valid_range());

    const auto all = TradeHistoryRequest::all();
    EXPECT_EQ(all.range_mode, TimeRangeMode::NONE);
    EXPECT_TRUE(all.has_valid_range());
}

TEST(TradeHistoryRequest, RoundTripsCommentJson) {
    TradeHistoryRequest request;
    request.start_ms = 1000;
    request.stop_ms = 2000;
    request.comment = "account-history-export";

    nlohmann::json json = request;
    const auto restored = json.get<TradeHistoryRequest>();

    EXPECT_EQ(restored.start_ms, request.start_ms);
    EXPECT_EQ(restored.stop_ms, request.stop_ms);
    EXPECT_EQ(restored.comment, request.comment);
}

TEST(DtoClone, TradeRequestSnapshotsDoNotCopyCallbacks) {
    auto request = std::make_shared<TradeRequest>();
    auto result = std::make_shared<TradeResult>();
    int callback_calls = 0;

    request->add_callback(
        [&callback_calls](
                std::unique_ptr<TradeRequest>,
                std::unique_ptr<TradeResult>) {
            ++callback_calls;
        });

    auto unique_clone = request->clone_unique();
    std::shared_ptr<TradeRequest> unique_clone_shared(
        std::move(unique_clone));
    unique_clone_shared->dispatch_callbacks(unique_clone_shared, result);
    EXPECT_EQ(callback_calls, 0);

    auto shared_clone = request->clone_shared();
    shared_clone->dispatch_callbacks(shared_clone, result);
    EXPECT_EQ(callback_calls, 0);

    auto copied = std::make_shared<TradeRequest>(*request);
    copied->dispatch_callbacks(copied, result);
    EXPECT_EQ(callback_calls, 0);

    request->dispatch_callbacks(request, result);
    EXPECT_EQ(callback_calls, 1);
}

TEST(DtoClone, AuthDataSnapshotsDoNotCopyCallbacks) {
    platforms::intrade_bar::AuthData auth;
    int callback_calls = 0;

    auth.add_callback(
        [&callback_calls](bool, const std::string&) {
            ++callback_calls;
        });

    auto unique_clone = auth.clone_unique();
    unique_clone->dispatch_callbacks(true, "unique clone");
    EXPECT_EQ(callback_calls, 0);

    auto shared_clone = auth.clone_shared();
    shared_clone->dispatch_callbacks(true, "shared clone");
    EXPECT_EQ(callback_calls, 0);

    platforms::intrade_bar::AuthData copied(auth);
    copied.dispatch_callbacks(true, "copied");
    EXPECT_EQ(callback_calls, 0);

    auth.dispatch_callbacks(true, "original");
    EXPECT_EQ(callback_calls, 1);
}

TEST(DtoClone, TradeUpAuthDataSnapshotsDoNotCopyCallbacks) {
    platforms::tradeup::AuthData auth;
    int callback_calls = 0;

    auth.add_callback(
        [&callback_calls](bool, const std::string&) {
            ++callback_calls;
        });

    auto unique_clone = auth.clone_unique();
    unique_clone->dispatch_callbacks(true, "unique clone");
    EXPECT_EQ(callback_calls, 0);

    auto shared_clone = auth.clone_shared();
    shared_clone->dispatch_callbacks(true, "shared clone");
    EXPECT_EQ(callback_calls, 0);

    platforms::tradeup::AuthData copied(auth);
    copied.dispatch_callbacks(true, "copied");
    EXPECT_EQ(callback_calls, 0);

    auth.dispatch_callbacks(true, "original");
    EXPECT_EQ(callback_calls, 1);
}

TEST(DtoClone, BridgeConfigSnapshotsCanDropCallbacks) {
    TestBridgeConfig config;
    int callback_calls = 0;

    config.add_callback(
        [&callback_calls](bool, const std::string&) {
            ++callback_calls;
        });

    auto unique_clone = config.clone_unique();
    unique_clone->dispatch_callbacks(true, "unique clone");
    EXPECT_EQ(callback_calls, 0);

    auto shared_clone = config.clone_shared();
    shared_clone->dispatch_callbacks(true, "shared clone");
    EXPECT_EQ(callback_calls, 0);

    TestBridgeConfig copied(config);
    copied.dispatch_callbacks(true, "copied");
    EXPECT_EQ(callback_calls, 0);

    config.dispatch_callbacks(true, "original");
    EXPECT_EQ(callback_calls, 1);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
