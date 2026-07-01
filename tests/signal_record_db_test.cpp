#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <thread>
#include <vector>

#include <optionx_cpp/storages.hpp>

namespace {

using optionx::SignalRecord;
using optionx::storage::SignalRecordDB;
using optionx::storage::SignalRecordDBStatus;
using optionx::storage::signal_detail::selected_timestamp_ms;

std::string unique_db_path(const std::string& name) {
    static std::atomic<std::uint64_t> counter{0};
    const auto stamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    return "data/" + name + "_" + std::to_string(stamp) + "_" +
        std::to_string(counter.fetch_add(1));
}

mdbxc::Config make_config(const std::string& name) {
    mdbxc::Config config;
    config.pathname = unique_db_path(name);
    config.max_dbs = 4;
    config.no_subdir = false;
    config.relative_to_exe = true;
    return config;
}

SignalRecord make_record(
        std::int64_t uid,
        std::int64_t create_date,
        std::uint32_t first_trade_id = 1001) {
    SignalRecord record;
    record.unique_id = uid;
    record.unique_hash = "signal-" + std::to_string(uid);
    record.account_id = 7001;
    record.platform_type = optionx::PlatformType::INTRADE_BAR;
    record.account_type = optionx::AccountType::DEMO;
    record.currency = optionx::CurrencyType::USD;
    record.symbol = "EURUSD";
    record.signal_name = "mean-reversion";
    record.user_data = "user-data";
    record.comment = "signal-db-test";
    record.option_type = optionx::OptionType::SPRINT;
    record.order_type = optionx::OrderType::BUY;
    record.amount = 15.0;
    record.refund = 0.1;
    record.min_payout = 0.75;
    record.duration = 180;
    record.expiry_time = 1712345900;
    record.status = optionx::SignalStatus::ACCEPTED;
    record.reject_code = optionx::SignalRejectCode::NONE;
    record.reject_desc = "";
    record.outcome = optionx::SignalOutcome::WIN;
    record.trade_state = optionx::TradeState::WIN;
    record.total_amount = 15.0;
    record.total_profit = 12.3;
    record.create_date = create_date;
    record.accept_date = create_date + 10;
    record.complete_date = create_date + 180000;
    record.mm_type = optionx::MmSystemType::MARTINGALE_SIGNAL;
    record.mm_step = 2;
    record.mm_group_id = 10;
    record.mm_group_hash = "group-hash";
    record.mm_group_name = "EURUSD-demo";
    record.mm_params_json = R"({"step":2})";
    record.decision_params_json = R"({"threshold":0.6})";
    record.metadata_json = R"({"source":"test"})";
    record.add_trade_id(first_trade_id);
    record.add_trade_id(first_trade_id + 1);
    return record;
}

bool contains_uid(const std::vector<SignalRecord>& records, std::int64_t uid) {
    return std::any_of(records.begin(), records.end(), [uid](const SignalRecord& record) {
        return record.unique_id == uid;
    });
}

} // namespace

TEST(SignalRecordSerializationTest, RoundTripsCurrentBinaryFormat) {
    auto record = make_record(42, 1712345600100);
    record.signal_id = 77;

    const auto bytes = record.to_bytes();
    const auto restored = SignalRecord::from_bytes(bytes.data(), bytes.size());

    EXPECT_EQ(restored, record);
    EXPECT_EQ(selected_timestamp_ms(restored), record.create_date);
}

TEST(SignalRecordSerializationTest, RejectsCorruptedPayloads) {
    auto record = make_record(42, 1712345600100);
    record.signal_id = 77;

    auto bytes = record.to_bytes();
    ASSERT_FALSE(bytes.empty());

    bytes.resize(bytes.size() - 1);
    EXPECT_THROW(
        static_cast<void>(SignalRecord::from_bytes(bytes.data(), bytes.size())),
        std::runtime_error);

    const std::uint32_t bad_magic = 0;
    EXPECT_THROW(
        static_cast<void>(SignalRecord::from_bytes(&bad_magic, sizeof(bad_magic))),
        std::runtime_error);
}

TEST(SignalRecordSerializationTest, RejectsCorruptedTradeIdVectorLengthBeforeReserve) {
    auto record = make_record(42, 1712345600100);
    record.signal_id = 77;

    auto bytes = record.to_bytes();
    ASSERT_GT(bytes.size(), sizeof(std::uint32_t) * (record.trade_ids.size() + 1));

    const auto vector_length_offset =
        bytes.size() - sizeof(std::uint32_t) * (record.trade_ids.size() + 1);
    const auto bad_length = (std::numeric_limits<std::uint32_t>::max)();
    std::memcpy(bytes.data() + vector_length_offset, &bad_length, sizeof(bad_length));

    EXPECT_THROW(
        static_cast<void>(SignalRecord::from_bytes(bytes.data(), bytes.size())),
        std::runtime_error);
}

TEST(SignalRecordDBTest, ReservesPersistentSignalIdAndAssignsCarriers) {
    const auto config = make_config("signal_record_db_ids");

    {
        SignalRecordDB db(config);
        ASSERT_TRUE(db.is_open());

        EXPECT_EQ(db.get_signal_id(), 1u);

        optionx::TradeSignal signal;
        ASSERT_TRUE(db.assign_signal_id(signal));
        EXPECT_EQ(signal.signal_id, 2u);

        SignalRecord record;
        ASSERT_TRUE(db.assign_signal_id(record));
        EXPECT_EQ(record.signal_id, 3u);

        optionx::TradeRequest request;
        ASSERT_TRUE(db.assign_signal_id(request));
        EXPECT_EQ(request.signal_id, 4u);
    }

    {
        SignalRecordDB reopened(config);
        ASSERT_TRUE(reopened.is_open());
        EXPECT_EQ(reopened.get_signal_id(), 5u);
    }
}

TEST(SignalRecordDBTest, UpsertFindMigrateEraseClearAndCount) {
    const auto config = make_config("signal_record_db_direct");
    SignalRecordDB db(config);
    ASSERT_TRUE(db.is_open());

    const std::int64_t ts = 1712345600100;
    auto first = make_record(42, ts, 1001);
    auto write = db.upsert(first);
    ASSERT_TRUE(write.ok()) << write.message;
    EXPECT_EQ(write.record.signal_id, 1u);
    EXPECT_EQ(db.count(), 1u);

    const auto first_id = write.record.signal_id;
    auto by_id = db.find_by_signal_id(first_id);
    ASSERT_TRUE(by_id.ok()) << by_id.message;
    EXPECT_EQ(by_id.record.unique_id, 42);

    auto by_uid = db.find_by_uid(42);
    ASSERT_TRUE(by_uid.ok()) << by_uid.message;
    ASSERT_EQ(by_uid.records.size(), 1u);
    EXPECT_EQ(by_uid.records[0].signal_id, first_id);

    auto by_trade = db.find_by_trade_id(1001);
    ASSERT_TRUE(by_trade.ok()) << by_trade.message;
    EXPECT_EQ(by_trade.record.signal_id, first_id);

    auto updated = make_record(42, ts + 1, 2001);
    updated.signal_id = first_id;
    updated.total_profit = 20.5;
    auto update = db.upsert(updated);
    ASSERT_TRUE(update.ok()) << update.message;
    EXPECT_EQ(update.record.signal_id, first_id);
    EXPECT_DOUBLE_EQ(db.find_by_signal_id(first_id).record.total_profit, 20.5);
    EXPECT_EQ(db.find_by_trade_id(1001).status, SignalRecordDBStatus::NOT_FOUND);
    ASSERT_TRUE(db.find_by_trade_id(2001).ok());

    auto second = make_record(43, ts + 2, 3001);
    auto second_write = db.upsert(second);
    ASSERT_TRUE(second_write.ok()) << second_write.message;
    EXPECT_EQ(second_write.record.signal_id, 2u);
    EXPECT_EQ(db.count(), 2u);

    auto by_timestamp = db.find_by_timestamp(ts + 1);
    ASSERT_TRUE(by_timestamp.ok()) << by_timestamp.message;
    ASSERT_EQ(by_timestamp.records.size(), 1u);
    EXPECT_EQ(by_timestamp.records[0].unique_id, 42);

    auto range = db.find_range(ts, ts + 2);
    ASSERT_TRUE(range.ok()) << range.message;
    EXPECT_EQ(range.records.size(), 2u);
    EXPECT_TRUE(contains_uid(range.records, 42));
    EXPECT_TRUE(contains_uid(range.records, 43));

    EXPECT_EQ(db.erase_by_signal_id(first_id), SignalRecordDBStatus::SUCCESS);
    auto missing_uid = db.find_by_uid(42);
    ASSERT_TRUE(missing_uid.ok()) << missing_uid.message;
    EXPECT_TRUE(missing_uid.records.empty());
    EXPECT_EQ(db.find_by_trade_id(2001).status, SignalRecordDBStatus::NOT_FOUND);
    EXPECT_EQ(db.count(), 1u);

    EXPECT_EQ(db.clear(), SignalRecordDBStatus::SUCCESS);
    EXPECT_EQ(db.count(), 0u);
    EXPECT_EQ(db.get_signal_id(), 1u);
}

TEST(SignalRecordDBTest, WriteRemovesStaleIndexesWhenFieldsChange) {
    const auto config = make_config("signal_record_db_stale_index");
    SignalRecordDB db(config);
    ASSERT_TRUE(db.is_open());

    auto original = make_record(50, 1712345600125, 1501);
    original.signal_id = 10;
    ASSERT_TRUE(db.write(original).ok());

    auto updated = make_record(51, 1712345600125, 2501);
    updated.signal_id = 10;
    ASSERT_TRUE(db.write(updated).ok());

    auto old_uid = db.find_by_uid(50);
    ASSERT_TRUE(old_uid.ok()) << old_uid.message;
    EXPECT_TRUE(old_uid.records.empty());
    EXPECT_EQ(db.find_by_trade_id(1501).status, SignalRecordDBStatus::NOT_FOUND);
    auto new_uid = db.find_by_uid(51);
    ASSERT_TRUE(new_uid.ok()) << new_uid.message;
    ASSERT_EQ(new_uid.records.size(), 1u);
    ASSERT_TRUE(db.find_by_trade_id(2501).ok());
}

TEST(SignalRecordDBTest, WriteMovesRecordWhenCanonicalTimestampChanges) {
    const auto config = make_config("signal_record_db_composite_move");
    SignalRecordDB db(config);
    ASSERT_TRUE(db.is_open());

    const std::int64_t old_ts = 1712345600000;
    const std::int64_t new_ts = old_ts + 120000;

    auto original = make_record(60, old_ts, 1601);
    original.signal_id = 12;
    ASSERT_TRUE(db.write(original).ok());

    auto old_range = db.find_range(old_ts, old_ts + 999);
    ASSERT_TRUE(old_range.ok()) << old_range.message;
    ASSERT_EQ(old_range.records.size(), 1u);
    EXPECT_EQ(old_range.records[0].signal_id, 12u);

    auto updated = make_record(60, new_ts, 1601);
    updated.signal_id = 12;
    updated.total_profit = 44.0;
    ASSERT_TRUE(db.write(updated).ok());

    old_range = db.find_range(old_ts, old_ts + 999);
    ASSERT_TRUE(old_range.ok()) << old_range.message;
    EXPECT_TRUE(old_range.records.empty());

    const auto new_range = db.find_range(new_ts, new_ts + 999);
    ASSERT_TRUE(new_range.ok()) << new_range.message;
    ASSERT_EQ(new_range.records.size(), 1u);
    EXPECT_EQ(new_range.records[0].signal_id, 12u);
    EXPECT_DOUBLE_EQ(new_range.records[0].total_profit, 44.0);
    EXPECT_EQ(db.count(), 1u);
    auto by_uid = db.find_by_uid(60);
    ASSERT_TRUE(by_uid.ok()) << by_uid.message;
    ASSERT_EQ(by_uid.records.size(), 1u);
    ASSERT_TRUE(db.find_by_trade_id(1601).ok());
}

TEST(SignalRecordDBTest, FindByUidReturnsAllMatchingUserIdsWithoutMerging) {
    const auto config = make_config("signal_record_db_duplicate_uid");
    SignalRecordDB db(config);
    ASSERT_TRUE(db.is_open());

    constexpr std::int64_t first_ts = 1712345600000;
    constexpr std::int64_t second_ts = 1712345660000;

    auto first = make_record(77, first_ts, 7701);
    auto second = make_record(77, second_ts, 7801);

    auto first_write = db.upsert(first);
    ASSERT_TRUE(first_write.ok()) << first_write.message;
    auto second_write = db.upsert(second);
    ASSERT_TRUE(second_write.ok()) << second_write.message;

    EXPECT_NE(first_write.record.signal_id, second_write.record.signal_id);

    auto by_uid = db.find_by_uid(77);
    ASSERT_TRUE(by_uid.ok()) << by_uid.message;
    ASSERT_EQ(by_uid.records.size(), 2u);
    EXPECT_TRUE(contains_uid(by_uid.records, 77));
    EXPECT_TRUE(db.find_by_signal_id(first_write.record.signal_id).ok());
    EXPECT_TRUE(db.find_by_signal_id(second_write.record.signal_id).ok());

    auto first_range = db.find_by_uid(77, first_ts, first_ts + 999);
    ASSERT_TRUE(first_range.ok()) << first_range.message;
    ASSERT_EQ(first_range.records.size(), 1u);
    EXPECT_EQ(first_range.records[0].signal_id, first_write.record.signal_id);

    auto second_range = db.find_by_uid(77, second_ts, second_ts + 999);
    ASSERT_TRUE(second_range.ok()) << second_range.message;
    ASSERT_EQ(second_range.records.size(), 1u);
    EXPECT_EQ(second_range.records[0].signal_id, second_write.record.signal_id);

    auto invalid_range = db.find_by_uid(77, second_ts, first_ts);
    EXPECT_EQ(invalid_range.status, SignalRecordDBStatus::INVALID_ARGUMENT);
}

TEST(SignalRecordDBTest, FindByUidAllowsZeroAndNegativeUserIds) {
    const auto config = make_config("signal_record_db_zero_negative_uid");
    SignalRecordDB db(config);
    ASSERT_TRUE(db.is_open());

    auto zero = make_record(0, 1712345600000, 7901);
    auto negative = make_record(-7, 1712345660000, 7903);

    auto zero_write = db.upsert(zero);
    ASSERT_TRUE(zero_write.ok()) << zero_write.message;
    auto negative_write = db.upsert(negative);
    ASSERT_TRUE(negative_write.ok()) << negative_write.message;

    auto zero_uid = db.find_by_uid(0);
    ASSERT_TRUE(zero_uid.ok()) << zero_uid.message;
    ASSERT_EQ(zero_uid.records.size(), 1u);
    EXPECT_EQ(zero_uid.records[0].signal_id, zero_write.record.signal_id);

    auto negative_uid = db.find_by_uid(-7);
    ASSERT_TRUE(negative_uid.ok()) << negative_uid.message;
    ASSERT_EQ(negative_uid.records.size(), 1u);
    EXPECT_EQ(negative_uid.records[0].signal_id, negative_write.record.signal_id);
}

TEST(SignalRecordDBTest, WriteRejectsTradeIdOwnedByAnotherSignal) {
    const auto config = make_config("signal_record_db_trade_conflict_write");
    SignalRecordDB db(config);
    ASSERT_TRUE(db.is_open());

    auto first = make_record(81, 1712345600000, 8101);
    first.signal_id = 1;
    ASSERT_TRUE(db.write(first).ok());

    auto second = make_record(82, 1712345601000, 8101);
    second.signal_id = 2;
    auto conflict = db.write(second);
    EXPECT_EQ(conflict.status, SignalRecordDBStatus::CONFLICT);

    auto owner = db.find_by_trade_id(8101);
    ASSERT_TRUE(owner.ok()) << owner.message;
    EXPECT_EQ(owner.record.signal_id, 1u);
}

TEST(SignalRecordDBTest, UpsertRejectsAmbiguousTradeIdOwners) {
    const auto config = make_config("signal_record_db_trade_conflict_upsert");
    SignalRecordDB db(config);
    ASSERT_TRUE(db.is_open());

    auto first = make_record(91, 1712345600000, 9101);
    first.signal_id = 1;
    ASSERT_TRUE(db.write(first).ok());

    auto second = make_record(92, 1712345601000, 9201);
    second.signal_id = 2;
    ASSERT_TRUE(db.write(second).ok());

    auto ambiguous = make_record(93, 1712345602000, 9301);
    ambiguous.trade_ids.clear();
    ambiguous.add_trade_id(9101);
    ambiguous.add_trade_id(9201);

    auto conflict = db.upsert(ambiguous);
    EXPECT_EQ(conflict.status, SignalRecordDBStatus::CONFLICT);
    EXPECT_EQ(db.count(), 2u);

    auto first_owner = db.find_by_trade_id(9101);
    ASSERT_TRUE(first_owner.ok()) << first_owner.message;
    EXPECT_EQ(first_owner.record.signal_id, 1u);

    auto second_owner = db.find_by_trade_id(9201);
    ASSERT_TRUE(second_owner.ok()) << second_owner.message;
    EXPECT_EQ(second_owner.record.signal_id, 2u);

    auto conflict_uid = db.find_by_uid(93);
    ASSERT_TRUE(conflict_uid.ok()) << conflict_uid.message;
    EXPECT_TRUE(conflict_uid.records.empty());
}

TEST(SignalRecordDBTest, ProcessRunsQueuedWorkOnCallerThread) {
    const auto config = make_config("signal_record_db_queue_process");
    SignalRecordDB db(config);
    ASSERT_TRUE(db.is_open());

    constexpr std::int64_t record_ts = 1712345600100;
    std::atomic<int> callback_count{0};
    std::uint32_t callback_signal_id = 0;
    ASSERT_EQ(
        db.enqueue_upsert(
            make_record(70, record_ts, 7001),
            [&](optionx::storage::SignalRecordDBWriteResult result) {
                ASSERT_TRUE(result.ok()) << result.message;
                callback_signal_id = result.record.signal_id;
                ++callback_count;
            }),
        SignalRecordDBStatus::SUCCESS);

    EXPECT_EQ(callback_count.load(), 0);
    db.process();
    EXPECT_EQ(callback_count.load(), 1);
    EXPECT_TRUE(db.find_by_signal_id(callback_signal_id).ok());

    std::size_t range_count = 0;
    ASSERT_EQ(
        db.enqueue_find_by_uid(
            70,
            record_ts,
            record_ts + 999,
            [&](optionx::storage::SignalRecordDBListResult result) {
                ASSERT_TRUE(result.ok()) << result.message;
                range_count = result.records.size();
                ++callback_count;
            }),
        SignalRecordDBStatus::SUCCESS);

    db.process();
    EXPECT_EQ(callback_count.load(), 2);
    EXPECT_EQ(range_count, 1u);
}

TEST(SignalRecordDBTest, WorkerFlushDeliversCallbacksOnFlushThread) {
    const auto config = make_config("signal_record_db_queue_worker");
    SignalRecordDB db(config);
    ASSERT_TRUE(db.is_open());
    ASSERT_TRUE(db.run());

    std::atomic<int> callback_count{0};
    ASSERT_EQ(
        db.enqueue_upsert(
            make_record(80, 1712345600100, 8001),
            [&](optionx::storage::SignalRecordDBWriteResult result) {
                ASSERT_TRUE(result.ok()) << result.message;
                ++callback_count;
            }),
        SignalRecordDBStatus::SUCCESS);

    db.flush();
    EXPECT_EQ(callback_count.load(), 1);
}

TEST(SignalRecordDBTest, ShutdownRejectsQueuedWork) {
    const auto config = make_config("signal_record_db_shutdown");
    SignalRecordDB db(config);
    ASSERT_TRUE(db.is_open());

    db.shutdown();
    EXPECT_FALSE(db.is_open());
    EXPECT_EQ(
        db.enqueue_upsert(make_record(90, 1712345600100, 9001)),
        SignalRecordDBStatus::QUEUE_CLOSED);
    EXPECT_EQ(db.find_by_signal_id(1).status, SignalRecordDBStatus::NOT_OPEN);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
