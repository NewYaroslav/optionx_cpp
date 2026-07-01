#include <gtest/gtest.h>
#include <mdbx_containers/KeyValueTable.hpp>

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
    request.signal_id = 501;
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

optionx::TradeSignal make_signal() {
    optionx::TradeSignal signal;
    signal.signal_id = 7007;
    signal.unique_id = 42;
    signal.unique_hash = "signal-hash";
    signal.platform_type = optionx::PlatformType::INTRADE_BAR;
    signal.account_id = 7001;
    signal.account_type = optionx::AccountType::DEMO;
    signal.currency = optionx::CurrencyType::USD;
    signal.symbol = "EURUSD";
    signal.signal_name = "mean-reversion";
    signal.user_data = R"({"source":"test"})";
    signal.comment = "round trip";
    signal.option_type = optionx::OptionType::CLASSIC;
    signal.order_type = optionx::OrderType::BUY;
    signal.amount = 15.5;
    signal.refund = 0.1;
    signal.min_payout = 0.75;
    signal.duration = 60;
    signal.expiry_time = 1712345700;
    signal.mm_step = 3;
    signal.mm_group_id = 73;
    signal.mm_group_hash = "signal-group-hash";
    signal.mm_group_name = "signal-group";
    return signal;
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
    result.set_balance(1012.71);
    result.set_open_balance(997.21);
    result.set_close_balance(1012.71);
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

TEST(TradeRecordSerializationTest, RoundTripsCurrentBinaryFormat) {
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

TEST(TradeRecordFactoryTest, AssignsRequestResultAndSignalData) {
    auto signal = make_signal();
    signal.set_money_management(std::make_unique<TestMoneyManagementParams>(3));
    signal.decision_params = std::make_unique<TestDecisionParams>(0.65);

    auto record = optionx::TradeRecord::from_trade(signal, make_result());
    record.trade_id = 7;

    EXPECT_EQ(record.trade_id, 7u);
    EXPECT_EQ(record.signal_id, 7007u);
    EXPECT_EQ(record.symbol, "EURUSD");
    EXPECT_EQ(record.signal_name, "mean-reversion");
    EXPECT_EQ(record.unique_id, 42);
    EXPECT_EQ(record.unique_hash, "signal-hash");
    EXPECT_EQ(record.option_id, 123456);
    EXPECT_EQ(record.option_hash, "broker-hash");
    EXPECT_TRUE(record.has_open_balance());
    EXPECT_TRUE(record.has_close_balance());
    EXPECT_DOUBLE_EQ(record.open_balance, 997.21);
    EXPECT_DOUBLE_EQ(record.close_balance, 1012.71);
    EXPECT_EQ(record.close_date, 1712345700000);
    EXPECT_EQ(record.mm_type, optionx::MmSystemType::MARTINGALE_SIGNAL);
    EXPECT_EQ(record.mm_step, 3);
    EXPECT_EQ(record.mm_group_id, 73);
    EXPECT_EQ(record.mm_group_hash, "signal-group-hash");
    EXPECT_EQ(record.mm_group_name, "signal-group");
    EXPECT_EQ(nlohmann::json::parse(record.mm_params_json).at("step"), 3);
    EXPECT_EQ(nlohmann::json::parse(record.decision_params_json).at("threshold"), 0.65);
}

TEST(SignalRecordTest, AssignsSignalDataAndProducedTradeIds) {
    auto signal = make_signal();
    signal.set_money_management(std::make_unique<TestMoneyManagementParams>(3));
    signal.decision_params = std::make_unique<TestDecisionParams>(0.65);

    auto record = optionx::SignalRecord::from_signal(signal);
    record.status = optionx::SignalStatus::ACCEPTED;
    record.outcome = optionx::SignalOutcome::WIN;
    record.total_profit = 12.71;

    record.add_trade_id(0);
    record.add_trade_id(7);
    record.add_trade_id(7);
    record.add_trade_id(8);

    EXPECT_TRUE(record.has_signal_id());
    EXPECT_EQ(record.signal_id, 7007u);
    EXPECT_EQ(record.unique_id, 42);
    EXPECT_EQ(record.unique_hash, "signal-hash");
    EXPECT_EQ(record.platform_type, optionx::PlatformType::INTRADE_BAR);
    EXPECT_EQ(record.symbol, "EURUSD");
    EXPECT_EQ(record.signal_name, "mean-reversion");
    EXPECT_EQ(record.mm_type, optionx::MmSystemType::MARTINGALE_SIGNAL);
    EXPECT_EQ(record.mm_step, 3);
    EXPECT_EQ(record.mm_group_id, 73);
    EXPECT_EQ(record.mm_group_hash, "signal-group-hash");
    EXPECT_EQ(record.mm_group_name, "signal-group");
    EXPECT_EQ(nlohmann::json::parse(record.mm_params_json).at("step"), 3);
    EXPECT_EQ(nlohmann::json::parse(record.decision_params_json).at("threshold"), 0.65);
    ASSERT_EQ(record.trade_ids.size(), 2u);
    EXPECT_EQ(record.trade_ids[0], 7u);
    EXPECT_EQ(record.trade_ids[1], 8u);

    const nlohmann::json json_record = record;
    const auto restored = json_record.get<optionx::SignalRecord>();
    EXPECT_EQ(restored.signal_id, record.signal_id);
    EXPECT_EQ(restored.status, optionx::SignalStatus::ACCEPTED);
    EXPECT_EQ(restored.outcome, optionx::SignalOutcome::WIN);
    EXPECT_EQ(restored.trade_ids, record.trade_ids);
}

TEST(SignalRecordTest, BuildsTradeRequestFromFlatSignalData) {
    auto signal = make_signal();

    const auto request = signal.to_trade_request();

    EXPECT_EQ(request.signal_id, 7007u);
    EXPECT_EQ(request.unique_id, 42);
    EXPECT_EQ(request.unique_hash, "signal-hash");
    EXPECT_EQ(request.account_id, 7001);
    EXPECT_EQ(request.account_type, optionx::AccountType::DEMO);
    EXPECT_EQ(request.currency, optionx::CurrencyType::USD);
    EXPECT_EQ(request.symbol, "EURUSD");
    EXPECT_EQ(request.signal_name, "mean-reversion");
    EXPECT_EQ(request.option_type, optionx::OptionType::CLASSIC);
    EXPECT_EQ(request.order_type, optionx::OrderType::BUY);
    EXPECT_DOUBLE_EQ(request.amount, 15.5);
    EXPECT_DOUBLE_EQ(request.refund, 0.1);
    EXPECT_DOUBLE_EQ(request.min_payout, 0.75);
    EXPECT_EQ(request.duration, 60);
    EXPECT_EQ(request.expiry_time, 1712345700);
}

TEST(SignalRecordTest, ClonesFlatSignalDataAndDecisionParams) {
    auto signal = make_signal();
    signal.decision_params = std::make_unique<TestDecisionParams>(0.75);

    const auto clone = signal.clone();

    ASSERT_NE(clone, nullptr);
    EXPECT_EQ(clone->signal_id, 7007u);
    EXPECT_EQ(clone->unique_id, 42);
    EXPECT_EQ(clone->unique_hash, "signal-hash");
    EXPECT_EQ(clone->symbol, "EURUSD");
    EXPECT_EQ(clone->mm_group_hash, "signal-group-hash");
    ASSERT_NE(clone->decision_params, nullptr);
    EXPECT_EQ(clone->decision_params->to_json().at("threshold"), 0.75);
}

TEST(SignalRecordTest, BuildsFromDirectTradeRequest) {
    auto request = make_request();

    const auto record = optionx::SignalRecord::from_signal(request);

    EXPECT_EQ(record.signal_id, 501u);
    EXPECT_EQ(record.unique_id, 42);
    EXPECT_EQ(record.unique_hash, "request-hash");
    EXPECT_EQ(record.symbol, "EURUSD");
    EXPECT_EQ(record.signal_name, "mean-reversion");
}

TEST(TradeSignalTest, RoundTripsFlatJsonAndBuildsRequest) {
    auto signal = make_signal();

    const nlohmann::json json_signal = signal;
    const auto restored = json_signal.get<optionx::TradeSignal>();
    const auto request = restored.to_trade_request();

    EXPECT_EQ(restored.signal_id, 7007u);
    EXPECT_EQ(restored.unique_id, 42);
    EXPECT_EQ(restored.unique_hash, "signal-hash");
    EXPECT_EQ(restored.platform_type, optionx::PlatformType::INTRADE_BAR);
    EXPECT_EQ(restored.mm_group_hash, "signal-group-hash");
    EXPECT_EQ(request.signal_id, 7007u);
    EXPECT_EQ(request.unique_id, 42);
    EXPECT_EQ(request.unique_hash, "signal-hash");
    EXPECT_EQ(request.symbol, "EURUSD");
    EXPECT_EQ(request.signal_name, "mean-reversion");
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
    EXPECT_EQ(record.signal_id, 501u);
    EXPECT_EQ(record.option_id, 98765);
    EXPECT_EQ(record.amount, 25.0);
    EXPECT_EQ(record.account_type, optionx::AccountType::DEMO);
    EXPECT_EQ(record.currency, optionx::CurrencyType::USD);
    EXPECT_EQ(record.close_date, 1712345700000);
    EXPECT_EQ(record.error_code, optionx::TradeErrorCode::PARSING_ERROR);
    EXPECT_EQ(record.error_desc, "partial broker response");
}

TEST(TradeRecordFactoryTest, MergesResultPatchWithoutErasingKnownFields) {
    auto record = make_record();
    record.trade_id = 321;
    record.amount = 25.0;
    record.payout = 0.82;
    record.profit = 10.5;
    record.open_price = 1.2345;
    record.close_price = 1.2355;
    record.open_date = 1712345400000;
    record.close_date = 1712345700000;
    record.trade_state = optionx::TradeState::IN_PROGRESS;
    record.account_type = optionx::AccountType::DEMO;
    record.currency = optionx::CurrencyType::USD;

    optionx::TradeResult patch;
    patch.trade_state = optionx::TradeState::STANDOFF;
    patch.profit = 0.0;
    patch.error_desc = "resolved as standoff";

    ASSERT_TRUE(record.merge_result_patch(patch));

    EXPECT_EQ(record.trade_id, 321u);
    EXPECT_EQ(record.amount, 25.0);
    EXPECT_DOUBLE_EQ(record.payout, 0.82);
    EXPECT_DOUBLE_EQ(record.profit, 0.0);
    EXPECT_DOUBLE_EQ(record.open_price, 1.2345);
    EXPECT_DOUBLE_EQ(record.close_price, 1.2355);
    EXPECT_EQ(record.open_date, 1712345400000);
    EXPECT_EQ(record.close_date, 1712345700000);
    EXPECT_EQ(record.trade_state, optionx::TradeState::STANDOFF);
    EXPECT_EQ(record.account_type, optionx::AccountType::DEMO);
    EXPECT_EQ(record.currency, optionx::CurrencyType::USD);
    EXPECT_EQ(record.error_desc, "resolved as standoff");
}

TEST(TradeRecordFactoryTest, EmptyResultPatchDoesNotUpdateRecord) {
    auto record = make_record();
    const auto before = record;

    optionx::TradeResult patch;

    EXPECT_FALSE(record.merge_result_patch(patch));
    EXPECT_EQ(record, before);
}

TEST(TradeRecordFactoryTest, DoesNotTreatLatestBalanceAsCloseBalance) {
    auto result = make_result();
    result.balance_flags = 0;
    result.open_balance = 0.0;
    result.close_balance = 0.0;
    result.set_balance(1007.5);

    const auto record = optionx::TradeRecord::from_trade(make_request(), result);

    EXPECT_FALSE(record.has_open_balance());
    EXPECT_FALSE(record.has_close_balance());
    EXPECT_DOUBLE_EQ(record.open_balance, 0.0);
    EXPECT_DOUBLE_EQ(record.close_balance, 0.0);
}

TEST(TradeRecordFactoryTest, EstimatesCloseBalanceFromOpenBalanceAndProfit) {
    optionx::TradeResult result;
    result.set_open_balance(1000.0);
    result.profit = -15.5;
    result.trade_state = optionx::TradeState::LOSS;

    const auto record = optionx::TradeRecord::from_trade(make_request(), result);

    EXPECT_TRUE(record.has_open_balance());
    EXPECT_TRUE(record.has_close_balance());
    EXPECT_DOUBLE_EQ(record.open_balance, 1000.0);
    EXPECT_DOUBLE_EQ(record.close_balance, 984.5);
}

TEST(TradeRecordFactoryTest, PreservesExplicitZeroCloseBalance) {
    optionx::TradeResult result;
    result.set_open_balance(100.0);
    result.set_close_balance(0.0);
    result.profit = -100.0;
    result.trade_state = optionx::TradeState::LOSS;

    const auto record = optionx::TradeRecord::from_trade(make_request(), result);

    EXPECT_TRUE(record.has_open_balance());
    EXPECT_TRUE(record.has_close_balance());
    EXPECT_DOUBLE_EQ(record.open_balance, 100.0);
    EXPECT_DOUBLE_EQ(record.close_balance, 0.0);

    const auto bytes = record.to_bytes();
    const auto restored = optionx::TradeRecord::from_bytes(bytes.data(), bytes.size());
    EXPECT_TRUE(restored.has_open_balance());
    EXPECT_TRUE(restored.has_close_balance());
    EXPECT_DOUBLE_EQ(restored.close_balance, 0.0);
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
