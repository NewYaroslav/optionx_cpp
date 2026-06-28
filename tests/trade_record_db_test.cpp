#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <limits>
#include <string>
#include <thread>
#include <vector>

#include <optionx_cpp/storages.hpp>

namespace {

using optionx::TradeRecord;
using optionx::storage::TradeRecordDB;
using optionx::storage::TradeRecordDBStatus;
using optionx::storage::detail::selected_timestamp_ms;
using optionx::storage::detail::timestamp_ms_to_unix_minutes;

std::string unique_db_path(const std::string& name) {
    static std::atomic<std::uint64_t> counter{0};
    const auto stamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    return "data/" + name + "_" + std::to_string(stamp) + "_" + std::to_string(counter.fetch_add(1));
}

mdbxc::Config make_config(const std::string& name) {
    mdbxc::Config config;
    config.pathname = unique_db_path(name);
    config.max_dbs = 4;
    config.no_subdir = false;
    config.relative_to_exe = true;
    return config;
}

TradeRecord make_record(
    std::int64_t uid,
    std::int64_t open_date,
    std::int64_t option_id = 1001,
    const std::string& option_hash = "broker-a",
    const std::string& symbol = "EURUSD") {
    TradeRecord record;
    record.request_unique_id = uid;
    record.request_unique_hash = "request-" + std::to_string(uid);
    record.account_id = 7001;
    record.option_id = option_id;
    record.option_hash = option_hash;
    record.platform_type = optionx::PlatformType::INTRADE_BAR;
    record.account_type = optionx::AccountType::DEMO;
    record.currency = optionx::CurrencyType::USD;
    record.symbol = symbol;
    record.signal_name = "mean-reversion";
    record.comment = "db-test";
    record.option_type = optionx::OptionType::CLASSIC;
    record.order_type = optionx::OrderType::BUY;
    record.amount = 15.0;
    record.refund = 0.1;
    record.min_payout = 0.75;
    record.payout = 0.82;
    record.profit = 12.3;
    record.balance = 1012.3;
    record.trade_state = optionx::TradeState::IN_PROGRESS;
    record.live_state = optionx::TradeState::IN_PROGRESS;
    record.open_price = 1.12345;
    record.close_price = 1.12400;
    record.delay = 30;
    record.ping = 15;
    record.place_date = open_date;
    record.send_date = open_date;
    record.open_date = open_date;
    record.close_date = open_date + 60000;
    record.duration = 60;
    record.mm_type = optionx::MmSystemType::MARTINGALE_SIGNAL;
    record.mm_step = 2;
    record.mm_group_id = 10;
    record.mm_group_hash = "group-hash";
    record.mm_group_name = "EURUSD-demo";
    record.mm_params_json = R"({"step":2})";
    record.decision_params_json = R"({"threshold":0.6})";
    record.metadata_json = R"({"source":"test"})";
    return record;
}

bool contains_uid(const std::vector<TradeRecord>& records, std::int64_t uid) {
    return std::any_of(records.begin(), records.end(), [uid](const TradeRecord& record) {
        return record.request_unique_id == uid;
    });
}

} // namespace

TEST(TradeRecordDBTest, ReservesPersistentTradeIdAndAssignsRequest) {
    const auto config = make_config("trade_record_db_uid");

    {
        TradeRecordDB db(config);
        ASSERT_TRUE(db.is_open());

        EXPECT_EQ(db.get_trade_id(), 1u);

        optionx::TradeRequest request;
        ASSERT_TRUE(db.assign_trade_id(request));
        EXPECT_EQ(request.trade_id, 2u);
        EXPECT_EQ(request.unique_id, 0);

        optionx::TradeRequest legacy_request;
        ASSERT_TRUE(db.assign_trade_uid(legacy_request));
        EXPECT_EQ(legacy_request.trade_id, 3u);
        EXPECT_EQ(legacy_request.unique_id, 0);
    }

    {
        TradeRecordDB reopened(config);
        ASSERT_TRUE(reopened.is_open());
        EXPECT_EQ(reopened.get_trade_id(), 4u);
    }
}

TEST(TradeRecordDBTest, UpsertFindMigrateEraseClearAndCount) {
    const auto config = make_config("trade_record_db_direct");
    TradeRecordDB db(config);
    ASSERT_TRUE(db.is_open());

    const std::int64_t ts = 1712345600100;
    auto first = make_record(42, ts, 1001, "broker-a");
    auto write = db.upsert(first);
    ASSERT_TRUE(write.ok()) << write.message;
    EXPECT_EQ(write.record.trade_id, 1u);
    EXPECT_EQ(db.count(), 1u);

    const auto first_id = write.record.trade_id;
    auto by_id = db.find_by_trade_id(first_id);
    ASSERT_TRUE(by_id.ok()) << by_id.message;
    EXPECT_EQ(by_id.record.request_unique_id, 42);

    auto by_uid = db.find_by_uid(42);
    ASSERT_TRUE(by_uid.ok()) << by_uid.message;
    EXPECT_EQ(by_uid.record.trade_id, first_id);

    auto updated = make_record(42, ts, 1001, "broker-a");
    updated.profit = 20.5;
    auto update = db.upsert(updated);
    ASSERT_TRUE(update.ok()) << update.message;
    EXPECT_EQ(update.record.trade_id, first_id);
    EXPECT_DOUBLE_EQ(db.find_by_trade_id(first_id).record.profit, 20.5);

    auto migrated = make_record(42, ts + 1, 1001, "broker-a");
    migrated.profit = 30.0;
    auto migration = db.upsert(migrated);
    ASSERT_TRUE(migration.ok()) << migration.message;
    EXPECT_EQ(migration.record.trade_id, first_id);
    ASSERT_TRUE(db.find_by_trade_id(first_id).ok());
    ASSERT_TRUE(db.find_by_uid(42).ok());
    EXPECT_EQ(db.find_by_uid(42).record.trade_id, migration.record.trade_id);
    EXPECT_EQ(db.count(), 1u);

    auto second = make_record(43, ts + 1, 1002, "broker-b");
    auto second_write = db.upsert(second);
    ASSERT_TRUE(second_write.ok()) << second_write.message;
    EXPECT_NE(second_write.record.trade_id, migration.record.trade_id);
    EXPECT_EQ(second_write.record.trade_id, 2u);
    EXPECT_EQ(db.count(), 2u);

    auto by_timestamp = db.find_by_timestamp(ts + 1);
    ASSERT_TRUE(by_timestamp.ok()) << by_timestamp.message;
    EXPECT_EQ(by_timestamp.records.size(), 2u);
    EXPECT_TRUE(contains_uid(by_timestamp.records, 42));
    EXPECT_TRUE(contains_uid(by_timestamp.records, 43));

    auto range = db.find_range(ts, ts + 2);
    ASSERT_TRUE(range.ok()) << range.message;
    EXPECT_EQ(range.records.size(), 2u);

    EXPECT_EQ(db.erase_by_trade_id(migration.record.trade_id), TradeRecordDBStatus::SUCCESS);
    EXPECT_EQ(db.find_by_uid(42).status, TradeRecordDBStatus::NOT_FOUND);
    EXPECT_EQ(db.count(), 1u);

    EXPECT_EQ(db.clear(), TradeRecordDBStatus::SUCCESS);
    EXPECT_EQ(db.count(), 0u);
    EXPECT_EQ(db.get_trade_id(), 1u);
}

TEST(TradeRecordDBTest, DefaultQueryReturnsAllModernRecords) {
    const auto config = make_config("trade_record_db_default_query");
    TradeRecordDB db(config);
    ASSERT_TRUE(db.is_open());

    ASSERT_TRUE(db.upsert(make_record(1, 1712345600100)).ok());

    const auto result = db.find_records(optionx::TradeRecordQuery{});
    ASSERT_TRUE(result.ok()) << result.message;
    ASSERT_EQ(result.records.size(), 1u);
    EXPECT_EQ(result.records[0].request_unique_id, 1);
}

TEST(TradeRecordDBTest, WriteRemovesStaleUidIndexWhenUidChanges) {
    const auto config = make_config("trade_record_db_stale_uid");
    TradeRecordDB db(config);
    ASSERT_TRUE(db.is_open());

    auto original = make_record(50, 1712345600125, 1501, "stale-uid");
    original.trade_id = 10;
    ASSERT_TRUE(db.write(original).ok());

    auto updated = make_record(51, 1712345600125, 1501, "stale-uid");
    updated.trade_id = 10;
    ASSERT_TRUE(db.write(updated).ok());

    EXPECT_EQ(db.find_by_uid(50).status, TradeRecordDBStatus::NOT_FOUND);
    ASSERT_TRUE(db.find_by_uid(51).ok());
    EXPECT_EQ(db.find_by_uid(51).record.trade_id, 10u);
}

TEST(TradeRecordDBTest, WriteBumpsNextTradeIdPastManualTradeId) {
    const auto config = make_config("trade_record_db_manual_id");
    TradeRecordDB db(config);
    ASSERT_TRUE(db.is_open());

    auto manual = make_record(0, 1712345600130, 1601, "manual-id");
    manual.trade_id = 25;
    ASSERT_TRUE(db.write(manual).ok());

    EXPECT_EQ(db.get_trade_id(), 26u);
}

TEST(TradeRecordDBTest, UpsertAllocatesLinearIdWithoutTimestamp) {
    const auto config = make_config("trade_record_db_no_timestamp");
    TradeRecordDB db(config);
    ASSERT_TRUE(db.is_open());

    TradeRecord record;
    record.symbol = "EURUSD";
    record.amount = 10.0;

    auto write = db.upsert(record);
    ASSERT_TRUE(write.ok()) << write.message;
    EXPECT_EQ(write.record.trade_id, 1u);
    EXPECT_TRUE(db.find_by_trade_id(write.record.trade_id).ok());
}

TEST(TradeRecordDBTest, SameMillisecondTradesGetDifferentLinearIds) {
    const auto config = make_config("trade_record_db_same_ms");
    TradeRecordDB db(config);
    ASSERT_TRUE(db.is_open());

    const std::int64_t ts = 1712345600150;
    auto first = db.upsert(make_record(1, ts, 2001, "same-ms-a"));
    auto second = db.upsert(make_record(2, ts, 2002, "same-ms-b"));

    ASSERT_TRUE(first.ok()) << first.message;
    ASSERT_TRUE(second.ok()) << second.message;
    EXPECT_NE(first.record.trade_id, second.record.trade_id);

    auto by_timestamp = db.find_by_timestamp(ts);
    ASSERT_TRUE(by_timestamp.ok()) << by_timestamp.message;
    EXPECT_EQ(by_timestamp.records.size(), 2u);
}

TEST(TradeRecordDBTest, BrokerIdentityUpdatesExistingTimestampRecord) {
    const auto config = make_config("trade_record_db_identity");
    TradeRecordDB db(config);
    ASSERT_TRUE(db.is_open());

    const std::int64_t ts = 1712345600200;
    auto original = db.upsert(make_record(0, ts, 5001, "identity-hash"));
    ASSERT_TRUE(original.ok()) << original.message;

    auto same_broker = make_record(77, ts, 5001, "identity-hash");
    same_broker.profit = -15.0;
    auto update = db.upsert(same_broker);
    ASSERT_TRUE(update.ok()) << update.message;
    EXPECT_EQ(update.record.trade_id, original.record.trade_id);
    EXPECT_EQ(db.count(), 1u);
    ASSERT_TRUE(db.find_by_uid(77).ok());
    EXPECT_DOUBLE_EQ(db.find_by_trade_id(original.record.trade_id).record.profit, -15.0);
}

TEST(TradeRecordDBTest, ProcessRunsQueuedWorkOnCallerThread) {
    const auto config = make_config("trade_record_db_process");
    TradeRecordDB db(config);
    ASSERT_TRUE(db.is_open());

    const auto caller_thread = std::this_thread::get_id();
    bool upsert_called = false;
    std::thread::id callback_thread;

    EXPECT_EQ(
        db.enqueue_upsert(make_record(10, 1712345600300), [&](optionx::storage::TradeRecordDBWriteResult result) {
            upsert_called = true;
            callback_thread = std::this_thread::get_id();
            EXPECT_TRUE(result.ok()) << result.message;
        }),
        TradeRecordDBStatus::SUCCESS);

    EXPECT_FALSE(upsert_called);
    db.process();
    EXPECT_TRUE(upsert_called);
    EXPECT_EQ(callback_thread, caller_thread);

    bool read_called = false;
    EXPECT_EQ(
        db.enqueue_find_by_uid(10, [&](optionx::storage::TradeRecordDBReadResult result) {
            read_called = true;
            EXPECT_TRUE(result.ok()) << result.message;
            EXPECT_EQ(std::this_thread::get_id(), caller_thread);
        }),
        TradeRecordDBStatus::SUCCESS);

    db.process();
    EXPECT_TRUE(read_called);
}

TEST(TradeRecordDBTest, WorkerFlushDeliversCallbacksOnFlushThread) {
    const auto config = make_config("trade_record_db_worker");
    TradeRecordDB db(config);
    ASSERT_TRUE(db.is_open());
    ASSERT_TRUE(db.run());

    const auto caller_thread = std::this_thread::get_id();
    bool callback_called = false;
    std::thread::id callback_thread;

    EXPECT_EQ(
        db.enqueue_upsert(make_record(11, 1712345600400), [&](optionx::storage::TradeRecordDBWriteResult result) {
            callback_called = true;
            callback_thread = std::this_thread::get_id();
            EXPECT_TRUE(result.ok()) << result.message;
        }),
        TradeRecordDBStatus::SUCCESS);

    db.flush();
    EXPECT_TRUE(callback_called);
    EXPECT_EQ(callback_thread, caller_thread);
    EXPECT_TRUE(db.find_by_uid(11).ok());
}

TEST(TradeRecordDBTest, ShutdownRejectsQueuedWork) {
    const auto config = make_config("trade_record_db_shutdown");
    TradeRecordDB db(config);
    ASSERT_TRUE(db.is_open());

    db.shutdown();
    EXPECT_EQ(db.enqueue_upsert(make_record(12, 1712345600500)), TradeRecordDBStatus::QUEUE_CLOSED);
    EXPECT_EQ(db.find_by_uid(12).status, TradeRecordDBStatus::NOT_OPEN);
}

TEST(SpreadPackTest, PacksAndUnpacksValues) {
    optionx::SpreadPack pack;
    pack.set_open_spread(0.00015, 5);
    pack.set_close_spread(-0.00010, 5);

    EXPECT_DOUBLE_EQ(pack.open_spread(), 0.00015);
    EXPECT_DOUBLE_EQ(pack.close_spread(), -0.00010);
    EXPECT_DOUBLE_EQ(pack.spread_difference(), -0.00025);
    EXPECT_TRUE(pack.is_open_spread_positive());
    EXPECT_FALSE(pack.is_close_spread_positive());
}

TEST(SpreadPackTest, PreservesCloseSpreadWhenOpenIsSetAfterwards) {
    optionx::SpreadPack pack;
    pack.set_close_spread(-0.00010, 5);
    pack.set_open_spread(0.00015, 5);

    EXPECT_DOUBLE_EQ(pack.open_spread(), 0.00015);
    EXPECT_DOUBLE_EQ(pack.close_spread(), -0.00010);
    EXPECT_DOUBLE_EQ(pack.spread_difference(), -0.00025);
}

TEST(SpreadPackTest, SetsBothSpreadsAtomicallyWithSharedPrecision) {
    optionx::SpreadPack pack;
    pack.set_spreads(0.00015, -0.00010, 5);

    EXPECT_EQ(pack.digits, 5);
    EXPECT_DOUBLE_EQ(pack.open_spread(), 0.00015);
    EXPECT_DOUBLE_EQ(pack.close_spread(), -0.00010);
    EXPECT_DOUBLE_EQ(pack.spread_difference(), -0.00025);
    EXPECT_TRUE(pack.is_open_spread_positive());
    EXPECT_FALSE(pack.is_close_spread_positive());
}

TEST(SpreadPackTest, HandlesZeroAndEdgeValues) {
    optionx::SpreadPack pack;
    pack.set_open_spread(0.0, 0);
    pack.set_close_spread(0.0, 0);
    EXPECT_DOUBLE_EQ(pack.open_spread(), 0.0);
    EXPECT_DOUBLE_EQ(pack.close_spread(), 0.0);

    pack.set_open_spread(2.0, 0);
    pack.set_close_spread(-3.0, 0);
    EXPECT_DOUBLE_EQ(pack.open_spread(), 2.0);
    EXPECT_DOUBLE_EQ(pack.close_spread(), -3.0);
}

TEST(SpreadPackTest, RejectsInvalidValuesAndPreservesPreviousState) {
    optionx::SpreadPack pack;
    pack.set_open_spread(0.00015, 5);
    pack.set_close_spread(-0.00010, 5);
    const auto raw = pack.raw;
    const auto digits = pack.digits;

    EXPECT_THROW(pack.set_open_spread(std::numeric_limits<double>::infinity(), 5), std::invalid_argument);
    EXPECT_THROW(pack.set_close_spread(0.000000003, 18), std::out_of_range);
    EXPECT_THROW(pack.set_open_spread(0.1, optionx::SpreadPack::max_digits + 1), std::invalid_argument);
    EXPECT_THROW(pack.set_spreads(0.0, 0.000000003, 18), std::out_of_range);

    EXPECT_EQ(pack.raw, raw);
    EXPECT_EQ(pack.digits, digits);
}

TEST(CompositeKeyTest, OrdersAcrossMinuteBuckets) {
    const auto config = make_config("trade_record_db_minute_buckets");
    TradeRecordDB db(config);
    ASSERT_TRUE(db.is_open());

    const std::int64_t ts_bucket0 = 59999;   // minute 0
    const std::int64_t ts_bucket1 = 60000;   // minute 1
    const std::int64_t ts_bucket2 = 120000;  // minute 2

    auto r0 = db.upsert(make_record(100, ts_bucket0, 3000, "bucket-0"));
    auto r1 = db.upsert(make_record(101, ts_bucket1, 3001, "bucket-1"));
    auto r2 = db.upsert(make_record(102, ts_bucket2, 3002, "bucket-2"));

    ASSERT_TRUE(r0.ok()) << r0.message;
    ASSERT_TRUE(r1.ok()) << r1.message;
    ASSERT_TRUE(r2.ok()) << r2.message;

    auto range = db.find_range(0, 130000);
    ASSERT_TRUE(range.ok()) << range.message;
    ASSERT_EQ(range.records.size(), 3u);

    // Should be sorted by timestamp ascending, trade_id ascending
    EXPECT_EQ(selected_timestamp_ms(range.records[0]), ts_bucket0);
    EXPECT_EQ(selected_timestamp_ms(range.records[1]), ts_bucket1);
    EXPECT_EQ(selected_timestamp_ms(range.records[2]), ts_bucket2);
}

TEST(CompositeKeyTest, ConvertsNegativeTimestampsUsingFloorMinutes) {
    EXPECT_EQ(timestamp_ms_to_unix_minutes(-1), -1);
    EXPECT_EQ(timestamp_ms_to_unix_minutes(-60000), -1);
    EXPECT_EQ(timestamp_ms_to_unix_minutes(-60001), -2);
}

TEST(CompositeKeyTest, UpdateMovesBetweenMinuteBuckets) {
    const auto config = make_config("trade_record_db_move_bucket");
    TradeRecordDB db(config);
    ASSERT_TRUE(db.is_open());

    const std::int64_t ts_old = 60000;
    const std::int64_t ts_new = 120000;

    auto original = make_record(200, ts_old, 4000, "move-bucket");
    auto write = db.upsert(original);
    ASSERT_TRUE(write.ok()) << write.message;
    const auto trade_id = write.record.trade_id;

    // Same trade_id, different minute bucket
    auto moved = make_record(200, ts_new, 4000, "move-bucket");
    moved.trade_id = trade_id;
    auto update = db.write(moved);
    ASSERT_TRUE(update.ok()) << update.message;

    auto old_range = db.find_range(0, 65000);
    ASSERT_TRUE(old_range.ok()) << old_range.message;
    EXPECT_EQ(old_range.records.size(), 0u);

    auto new_range = db.find_range(60000, 130000);
    ASSERT_TRUE(new_range.ok()) << new_range.message;
    ASSERT_EQ(new_range.records.size(), 1u);
    EXPECT_EQ(new_range.records[0].trade_id, trade_id);
    EXPECT_EQ(selected_timestamp_ms(new_range.records[0]), ts_new);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
