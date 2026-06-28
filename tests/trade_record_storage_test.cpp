#include <gtest/gtest.h>
#include <mdbx_containers/KeyValueTable.hpp>

#include <limits>

#include <optionx_cpp/data/trading.hpp>

namespace {

class TestMoneyManagementParams : public optionx::IMoneyManagementParams {
public:
    explicit TestMoneyManagementParams(int step = 0) : step(step) {}

    optionx::MmSystemType get_type() const override {
        return optionx::MmSystemType::MARTINGALE_SIGNAL;
    }

    std::unique_ptr<optionx::IMoneyManagementParams> clone() const override {
        return std::make_unique<TestMoneyManagementParams>(*this);
    }

    static std::unique_ptr<optionx::IMoneyManagementParams> from_json(const nlohmann::json&) {
        return nullptr;
    }

    nlohmann::json to_json() const override {
        return nlohmann::json{{"step", step}, {"group", "alpha"}};
    }

    int step = 0;
};

class TestDecisionParams : public optionx::ITradeDecisionParams {
public:
    explicit TestDecisionParams(double threshold = 0.0) : threshold(threshold) {}

    optionx::MmSystemType get_type() const override {
        return optionx::MmSystemType::KELLY_CRITERION;
    }

    std::unique_ptr<optionx::ITradeDecisionParams> clone() const override {
        return std::make_unique<TestDecisionParams>(*this);
    }

    static std::unique_ptr<optionx::ITradeDecisionParams> from_json(const nlohmann::json&) {
        return nullptr;
    }

    nlohmann::json to_json() const override {
        return nlohmann::json{{"threshold", threshold}};
    }

    double threshold = 0.0;
};

optionx::TradeRequest make_request() {
    optionx::TradeRequest request;
    request.symbol = "EURUSD";
    request.signal_name = "mean-reversion";
    request.user_data = R"({"source":"test"})";
    request.comment = "round trip";
    request.unique_hash = "request-hash";
    request.unique_id = 42;
    request.account_id = 7001;
    request.option_type = optionx::OptionType::CLASSIC;
    request.order_type = optionx::OrderType::BUY;
    request.account_type = optionx::AccountType::DEMO;
    request.currency = optionx::CurrencyType::USD;
    request.amount = 15.5;
    request.refund = 0.1;
    request.min_payout = 0.75;
    request.duration = 60;
    request.expiry_time = 1712345700;
    return request;
}

optionx::TradeResult make_result() {
    optionx::TradeResult result;
    result.trade_id = 9001;
    result.error_code = optionx::TradeErrorCode::SUCCESS;
    result.option_hash = "broker-hash";
    result.option_id = 123456;
    result.amount = 15.5;
    result.payout = 0.82;
    result.profit = 12.71;
    result.balance = 1012.71;
    result.open_price = 1.12345;
    result.close_price = 1.12400;
    result.delay = 30;
    result.ping = 15;
    result.place_date = 1712345600000;
    result.send_date = 1712345600050;
    result.open_date = 1712345600100;
    result.close_date = 1712345700000;
    result.trade_state = optionx::TradeState::WIN;
    result.live_state = optionx::TradeState::WIN;
    result.account_type = optionx::AccountType::DEMO;
    result.currency = optionx::CurrencyType::USD;
    result.platform_type = optionx::PlatformType::INTRADE_BAR;
    return result;
}

optionx::TradeRecord make_record() {
    auto record = optionx::TradeRecord::from_trade(make_request(), make_result());
    record.trade_id = 1;

    record.mm_type = optionx::MmSystemType::SKU_SYMBOL;
    record.mm_step = 4;
    record.mm_group_id = 73;
    record.mm_group_hash = "group-hash";
    record.mm_group_name = "EURUSD-demo";
    record.mm_params_json = R"({"mode":"sku","step":4})";
    record.decision_params_json = R"({"threshold":0.65})";
    record.metadata_json = R"({"note":"future fields"})";
    return record;
}

} // namespace

TEST(TradeRecordSerializationTest, RoundTripsBinaryV2) {
    const auto record = make_record();
    const auto bytes = record.to_bytes();
    const auto restored = optionx::TradeRecord::from_bytes(bytes.data(), bytes.size());

    EXPECT_EQ(restored, record);
}

TEST(TradeRecordSerializationTest, RejectsCorruptedPayloads) {
    const auto record = make_record();
    auto bytes = record.to_bytes();

    ASSERT_FALSE(bytes.empty());
    bytes[0] ^= 0xff;
    EXPECT_THROW(optionx::TradeRecord::from_bytes(bytes.data(), bytes.size()), std::runtime_error);

    bytes = record.to_bytes();
    bytes.pop_back();
    EXPECT_THROW(optionx::TradeRecord::from_bytes(bytes.data(), bytes.size()), std::runtime_error);
}

TEST(TradeRecordSerializationTest, RejectsTradeIdOutsideStorageFormat) {
    auto record = make_record();
    record.trade_id = static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()) + 1u;

    EXPECT_THROW(record.to_bytes(), std::overflow_error);
}

TEST(TradeRecordFactoryTest, AssignsRequestResultAndSignalData) {
    optionx::TradeSignal signal;
    signal.request = make_request();
    signal.set_money_management(std::make_unique<TestMoneyManagementParams>(3));
    signal.decision_params = std::make_unique<TestDecisionParams>(0.65);

    auto record = optionx::TradeRecord::from_trade(signal, make_result());
    record.trade_id = 7;

    EXPECT_EQ(record.trade_id, 7u);
    EXPECT_EQ(record.symbol, "EURUSD");
    EXPECT_EQ(record.signal_name, "mean-reversion");
    EXPECT_EQ(record.request_unique_id, 42);
    EXPECT_EQ(record.request_unique_hash, "request-hash");
    EXPECT_EQ(record.option_id, 123456);
    EXPECT_EQ(record.option_hash, "broker-hash");
    EXPECT_EQ(record.close_date, 1712345700000);
    EXPECT_EQ(record.mm_type, optionx::MmSystemType::MARTINGALE_SIGNAL);
    EXPECT_EQ(nlohmann::json::parse(record.mm_params_json).at("step"), 3);
    EXPECT_EQ(nlohmann::json::parse(record.decision_params_json).at("threshold"), 0.65);
}

TEST(TradeRecordFactoryTest, PreservesRequestContextWhenResultIsPartial) {
    auto request = make_request();
    request.trade_id = 321;
    request.account_type = optionx::AccountType::DEMO;
    request.currency = optionx::CurrencyType::USD;
    request.amount = 25.0;

    optionx::TradeResult partial_result;
    partial_result.option_id = 98765;
    partial_result.error_code = optionx::TradeErrorCode::PARSING_ERROR;
    partial_result.error_desc = "partial broker response";

    const auto record = optionx::TradeRecord::from_trade(request, partial_result);

    EXPECT_EQ(record.trade_id, 321u);
    EXPECT_EQ(record.option_id, 98765);
    EXPECT_EQ(record.amount, 25.0);
    EXPECT_EQ(record.account_type, optionx::AccountType::DEMO);
    EXPECT_EQ(record.currency, optionx::CurrencyType::USD);
    EXPECT_EQ(record.close_date, 1712345700000);
    EXPECT_EQ(record.error_code, optionx::TradeErrorCode::PARSING_ERROR);
    EXPECT_EQ(record.error_desc, "partial broker response");
}

TEST(TradeRecordFactoryTest, UsesCloseDateForPlannedAndKnownCloseTime) {
    const auto request = make_request();

    const auto request_record = optionx::TradeRecord::from_trade(request);
    EXPECT_EQ(request_record.close_date, 1712345700000);

    auto partial_result = make_result();
    partial_result.close_date = 0;
    const auto partial_record = optionx::TradeRecord::from_trade(request, partial_result);
    EXPECT_EQ(partial_record.close_date, 1712345700000);

    auto sprint_request = make_request();
    sprint_request.option_type = optionx::OptionType::SPRINT;
    sprint_request.expiry_time = 0;
    const auto sprint_record = optionx::TradeRecord::from_trade(sprint_request);
    EXPECT_EQ(sprint_record.close_date, 0);
}

TEST(TradeRecordIdentityTest, MatchesBrokerIdentityWithKnownContext) {
    auto first = make_record();
    auto second = make_record();
    second.trade_id = 2;

    EXPECT_TRUE(first.has_broker_identity());
    EXPECT_TRUE(first.same_broker_identity(second));

    second.symbol = "GBPUSD";
    EXPECT_FALSE(first.same_broker_identity(second));
}

TEST(TradeRecordStorageTest, StoresInMdbxKeyValueTable) {
    mdbxc::Config config;
    config.pathname = "data/trade_record_storage_test";
    config.max_dbs = 1;
    config.no_subdir = false;
    config.relative_to_exe = true;

    auto connection = mdbxc::Connection::create(config);
    mdbxc::KeyValueTable<std::uint64_t, optionx::TradeRecord> table(connection, "trade_records");
    table.clear();

    const auto record = make_record();
    table.insert_or_assign(record.trade_id, record);

#if __cplusplus >= 201703L
    const auto stored = table.find(record.trade_id);
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(*stored, record);
#else
    const auto stored = table.find_compat(record.trade_id);
    ASSERT_TRUE(stored.first);
    EXPECT_EQ(stored.second, record);
#endif
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
