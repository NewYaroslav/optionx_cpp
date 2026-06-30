#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include <optionx_cpp/storages.hpp>

namespace {

using optionx::TradeRecord;
using optionx::TradeRecordFilter;
using optionx::TradeRecordQuery;
using optionx::TradeRecordTimeField;
using optionx::TimeRangeMode;
using optionx::storage::TradeRecordDB;
using optionx::storage::TradeRecordDBStatus;
using optionx::storage::TradeRecordFilterMatcher;
using optionx::storage::TradeRecordStatusFixer;
using optionx::storage::TradeStatsCalculator;
using optionx::storage::TradeMetaStatsCalculator;
using optionx::storage::detail::selected_timestamp_ms;

bool contains_uid(const std::vector<TradeRecord>& records, std::int64_t uid) {
    return std::any_of(records.begin(), records.end(), [uid](const TradeRecord& record) {
        return record.request_unique_id == uid;
    });
}

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

TradeRecord make_win_record(
    std::int64_t uid,
    std::int64_t open_date,
    double amount = 10.0,
    double profit = 8.2,
    const std::string& symbol = "EURUSD",
    optionx::TradeState state = optionx::TradeState::WIN) {
    TradeRecord record;
    record.request_unique_id = uid;
    record.account_id = 7001;
    record.option_id = 1000 + uid;
    record.platform_type = optionx::PlatformType::INTRADE_BAR;
    record.account_type = optionx::AccountType::DEMO;
    record.currency = optionx::CurrencyType::USD;
    record.symbol = symbol;
    record.signal_name = "test-signal";
    record.option_type = optionx::OptionType::CLASSIC;
    record.order_type = (uid % 2 == 0) ? optionx::OrderType::BUY : optionx::OrderType::SELL;
    record.amount = amount;
    record.payout = 0.82;
    record.profit = profit;
    record.set_open_balance(1000.0);
    record.set_close_balance(1000.0 + profit);
    record.trade_state = state;
    record.open_price = 1.12345;
    record.close_price = 1.12400;
    record.delay = 30;
    record.ping = 15;
    record.place_date = open_date;
    record.send_date = open_date;
    record.open_date = open_date;
    record.close_date = open_date + 60000;
    record.duration = 60;
    record.mm_step = (uid == 1) ? 0 : 1;
    return record;
}

} // namespace

TEST(TradeRecordFilterMatcherTest, MatchesBySymbol) {
    TradeRecord rec = make_win_record(1, 100000, 10.0, 8.2, "EURUSD");

    TradeRecordQuery query;
    query.filter.symbols.add_include("EURUSD");
    EXPECT_TRUE(TradeRecordFilterMatcher::match(rec, query));

    query.filter.symbols.clear();
    query.filter.symbols.add_include("GBPUSD");
    EXPECT_FALSE(TradeRecordFilterMatcher::match(rec, query));
}

TEST(TradeRecordFilterMatcherTest, MatchesByPlatformAndState) {
    TradeRecord rec = make_win_record(1, 100000);
    rec.platform_type = optionx::PlatformType::INTRADE_BAR;
    rec.trade_state = optionx::TradeState::WIN;

    TradeRecordQuery query;
    query.filter.platforms.add_include(optionx::PlatformType::INTRADE_BAR);
    query.filter.trade_states.add_include(optionx::TradeState::WIN);
    EXPECT_TRUE(TradeRecordFilterMatcher::match(rec, query));

    query.filter.trade_states.clear();
    query.filter.trade_states.add_include(optionx::TradeState::LOSS);
    EXPECT_FALSE(TradeRecordFilterMatcher::match(rec, query));
}

TEST(TradeRecordFilterMatcherTest, MatchesByAmountRange) {
    TradeRecord rec = make_win_record(1, 100000, 15.0);

    TradeRecordQuery query;
    query.filter.min_amount = 10.0;
    query.filter.max_amount = 20.0;
    EXPECT_TRUE(TradeRecordFilterMatcher::match(rec, query));

    query.filter.min_amount = 20.0;
    EXPECT_FALSE(TradeRecordFilterMatcher::match(rec, query));
}

TEST(TradeRecordFilterMatcherTest, OptionalRangeBoundsDistinguishUnsetFromZero) {
    TradeRecordFilter filter;
    EXPECT_FALSE(filter.max_profit.has_value());

    filter.max_profit = 0.0;
    ASSERT_TRUE(filter.max_profit.has_value());
    EXPECT_DOUBLE_EQ(*filter.max_profit, 0.0);

    filter.max_profit.reset();
    EXPECT_FALSE(filter.max_profit.has_value());
}

TEST(TradeRecordFilterMatcherTest, MatchesZeroAndNegativeProfitRange) {
    TradeRecord loss = make_win_record(1, 100000, 10.0, -5.0);
    TradeRecord standoff = make_win_record(2, 100000, 10.0, 0.0);
    TradeRecord win = make_win_record(3, 100000, 10.0, 2.0);

    TradeRecordQuery query;
    query.filter.max_profit = 0.0;
    EXPECT_TRUE(TradeRecordFilterMatcher::match(loss, query));
    EXPECT_TRUE(TradeRecordFilterMatcher::match(standoff, query));
    EXPECT_FALSE(TradeRecordFilterMatcher::match(win, query));

    query.filter.min_profit = -10.0;
    query.filter.max_profit = -1.0;
    EXPECT_TRUE(TradeRecordFilterMatcher::match(loss, query));
    EXPECT_FALSE(TradeRecordFilterMatcher::match(standoff, query));
    EXPECT_FALSE(TradeRecordFilterMatcher::match(win, query));
}

TEST(TradeRecordFilterMatcherTest, MatchesZeroLatencyRange) {
    TradeRecord immediate = make_win_record(1, 100000);
    immediate.ping = 0;
    immediate.delay = 0;

    TradeRecord delayed = make_win_record(2, 100000);
    delayed.ping = 1;
    delayed.delay = 1;

    TradeRecordQuery query;
    query.filter.max_ping = 0;
    query.filter.max_delay = 0;

    EXPECT_TRUE(TradeRecordFilterMatcher::match(immediate, query));
    EXPECT_FALSE(TradeRecordFilterMatcher::match(delayed, query));
}

TEST(TradeRecordFilterMatcherTest, OnlyTerminalFilter) {
    TradeRecord rec = make_win_record(1, 100000, 10.0, 8.2, "EURUSD", optionx::TradeState::IN_PROGRESS);

    TradeRecordQuery query;
    query.filter.only_terminal = true;
    EXPECT_FALSE(TradeRecordFilterMatcher::match(rec, query));

    rec.trade_state = optionx::TradeState::WIN;
    EXPECT_TRUE(TradeRecordFilterMatcher::match(rec, query));
}

TEST(TradeRecordFilterMatcherTest, TimeRangeClosedAndHalfOpen) {
    TradeRecord rec = make_win_record(1, 100000);

    TradeRecordQuery query;
    query.start_ms = 50000;
    query.stop_ms = 100000;
    query.range_mode = TimeRangeMode::CLOSED;
    query.time_field = TradeRecordTimeField::OPEN_DATE;
    EXPECT_TRUE(TradeRecordFilterMatcher::match(rec, query));

    query.range_mode = TimeRangeMode::HALF_OPEN;
    EXPECT_FALSE(TradeRecordFilterMatcher::match(rec, query)); // 100000 >= 100000
}

TEST(TradeRecordFilterMatcherTest, NamedZoneUsesDstForHourFilter) {
    TradeRecord rec = make_win_record(
        1,
        time_shield::to_timestamp_ms(2026, 7, 1, 12, 0, 0));

    TradeRecordQuery query;
    query.time_zone = optionx::TradeTimeZone::named(time_shield::CET);
    query.filter.hours.add_include(14);
    EXPECT_TRUE(TradeRecordFilterMatcher::match(rec, query));

    query.time_zone = optionx::TradeTimeZone::fixed_offset(time_shield::SEC_PER_HOUR);
    EXPECT_FALSE(TradeRecordFilterMatcher::match(rec, query));
}

TEST(TradeTimeZoneTest, RepeatedDstHourHasDistinctUtcBuckets) {
    const auto tz = optionx::TradeTimeZone::named(time_shield::CET);

    const auto first = time_shield::to_timestamp_ms(2026, 10, 25, 0, 30, 0);
    const auto second = time_shield::to_timestamp_ms(2026, 10, 25, 1, 30, 0);

    EXPECT_EQ(
        tz.start_of_local_hour_utc_ms(first),
        time_shield::to_timestamp_ms(2026, 10, 25, 0, 0, 0));
    EXPECT_EQ(
        tz.start_of_local_hour_utc_ms(second),
        time_shield::to_timestamp_ms(2026, 10, 25, 1, 0, 0));
    EXPECT_NE(
        tz.start_of_local_hour_utc_ms(first),
        tz.start_of_local_hour_utc_ms(second));
}

TEST(TradeRecordStatusFixerTest, MarksStaleAsCheckError) {
    std::vector<TradeRecord> records;
    // Terminal trade at 1000000
    records.push_back(make_win_record(1, 0, 10.0, 8.2, "EURUSD", optionx::TradeState::WIN));
    records.back().close_date = 1000000;
    records.back().open_date = 900000;
    // Stuck trade
    records.push_back(make_win_record(2, 0, 10.0, 0.0, "EURUSD", optionx::TradeState::OPEN_SUCCESS));
    records.back().open_date = 800000;
    records.back().close_date = 850000;
    records.back().duration = 60;

    // wait_status_ms = 100000 -> stale_border = 1000000 - 100000 = 900000
    // open_date=800000 + 60*1000 = 860000 < 900000 -> CHECK_ERROR
    TradeRecordStatusFixer::fix_stale_statuses(records, 100000);

    EXPECT_EQ(records[0].trade_state, optionx::TradeState::WIN);
    EXPECT_EQ(records[1].trade_state, optionx::TradeState::CHECK_ERROR);
}

TEST(TradeRecordStatusFixerTest, CallbackResolvesStaleTrade) {
    std::vector<TradeRecord> records;
    records.push_back(make_win_record(1, 0, 10.0, 8.2, "EURUSD", optionx::TradeState::WIN));
    records.back().close_date = 1000000;
    records.back().open_date = 900000;

    records.push_back(make_win_record(2, 0, 10.0, 0.0, "EURUSD", optionx::TradeState::OPEN_SUCCESS));
    records.back().open_date = 800000;
    records.back().close_date = 850000;
    records.back().duration = 60;

    auto resolver = [](const TradeRecord& rec) -> optionx::TradeResult {
        optionx::TradeResult result;
        result.trade_state = optionx::TradeState::WIN;
        result.profit = 8.0;
        result.close_date = rec.close_date;
        return result;
    };

    TradeRecordStatusFixer::fix_stale_statuses(records, 100000, resolver);

    EXPECT_EQ(records[1].trade_state, optionx::TradeState::WIN);
    EXPECT_DOUBLE_EQ(records[1].profit, 8.0);
    EXPECT_EQ(records[1].close_date, 850000);
}

TEST(TradeRecordDBTest, FindRecordsWithSymbolFilter) {
    const auto config = make_config("find_records_filter");
    TradeRecordDB db(config);
    ASSERT_TRUE(db.is_open());

    auto r1 = db.upsert(make_win_record(1, 1712345600100, 10.0, 8.2, "EURUSD"));
    auto r2 = db.upsert(make_win_record(2, 1712345600200, 10.0, 8.2, "GBPUSD"));
    auto r3 = db.upsert(make_win_record(3, 1712345600300, 10.0, -10.0, "EURUSD", optionx::TradeState::LOSS));
    ASSERT_TRUE(r1.ok());
    ASSERT_TRUE(r2.ok());
    ASSERT_TRUE(r3.ok());

    TradeRecordQuery query;
    query.start_ms = 0;
    query.stop_ms = std::numeric_limits<std::int64_t>::max();
    query.filter.symbols.add_include("EURUSD");

    auto result = db.find_records(query);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result.records.size(), 2u);
    EXPECT_EQ(result.records[0].symbol, "EURUSD");
    EXPECT_EQ(result.records[1].symbol, "EURUSD");
}

TEST(TradeRecordDBTest, FindRecordsWithTimeFieldAndLimit) {
    const auto config = make_config("find_records_limit");
    TradeRecordDB db(config);
    ASSERT_TRUE(db.is_open());

    for (int i = 0; i < 5; ++i) {
        ASSERT_TRUE(db.upsert(make_win_record(i + 1, 1712345600100 + i * 1000)).ok());
    }

    TradeRecordQuery query;
    query.start_ms = 0;
    query.stop_ms = std::numeric_limits<std::int64_t>::max();
    query.limit = 3;
    query.ascending = true;

    auto result = db.find_records(query);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result.records.size(), 3u);
}

TEST(TradeRecordDBTest, FindTodayReturnsEmptyWhenNoTrades) {
    const auto config = make_config("find_today_empty");
    TradeRecordDB db(config);
    ASSERT_TRUE(db.is_open());

    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    auto result = db.find_today(now, 0);
    ASSERT_TRUE(result.ok());
    EXPECT_TRUE(result.records.empty());
}

TEST(TradeRecordDBTest, FindDayUsesNamedZoneDstBoundaries) {
    const auto config = make_config("find_day_named_zone_dst");
    TradeRecordDB db(config);
    ASSERT_TRUE(db.is_open());

    const auto before_day = time_shield::to_timestamp_ms(2026, 3, 28, 22, 30, 0);
    const auto start_day = time_shield::to_timestamp_ms(2026, 3, 28, 23, 30, 0);
    const auto end_day = time_shield::to_timestamp_ms(2026, 3, 29, 21, 30, 0);
    const auto after_day = time_shield::to_timestamp_ms(2026, 3, 29, 22, 30, 0);

    ASSERT_TRUE(db.upsert(make_win_record(1, before_day)).ok());
    ASSERT_TRUE(db.upsert(make_win_record(2, start_day)).ok());
    ASSERT_TRUE(db.upsert(make_win_record(3, end_day)).ok());
    ASSERT_TRUE(db.upsert(make_win_record(4, after_day)).ok());

    const auto result = db.find_day(
        time_shield::to_timestamp_ms(2026, 3, 29, 12, 0, 0),
        optionx::TradeTimeZone::named(time_shield::CET));

    ASSERT_TRUE(result.ok()) << result.message;
    EXPECT_EQ(result.records.size(), 2u);
    EXPECT_FALSE(contains_uid(result.records, 1));
    EXPECT_TRUE(contains_uid(result.records, 2));
    EXPECT_TRUE(contains_uid(result.records, 3));
    EXPECT_FALSE(contains_uid(result.records, 4));
}

TEST(TradeRecordDBTest, FindDayUsesNamedZoneDstEndBoundaries) {
    const auto config = make_config("find_day_named_zone_dst_end");
    TradeRecordDB db(config);
    ASSERT_TRUE(db.is_open());

    const auto before_day = time_shield::to_timestamp_ms(2026, 10, 24, 21, 30, 0);
    const auto first_repeated_hour = time_shield::to_timestamp_ms(2026, 10, 25, 0, 30, 0);
    const auto second_repeated_hour = time_shield::to_timestamp_ms(2026, 10, 25, 1, 30, 0);
    const auto late_day = time_shield::to_timestamp_ms(2026, 10, 25, 22, 30, 0);
    const auto after_day = time_shield::to_timestamp_ms(2026, 10, 25, 23, 30, 0);

    ASSERT_TRUE(db.upsert(make_win_record(1, before_day)).ok());
    ASSERT_TRUE(db.upsert(make_win_record(2, first_repeated_hour)).ok());
    ASSERT_TRUE(db.upsert(make_win_record(3, second_repeated_hour)).ok());
    ASSERT_TRUE(db.upsert(make_win_record(4, late_day)).ok());
    ASSERT_TRUE(db.upsert(make_win_record(5, after_day)).ok());

    const auto result = db.find_day(
        time_shield::to_timestamp_ms(2026, 10, 25, 12, 0, 0),
        optionx::TradeTimeZone::named(time_shield::CET));

    ASSERT_TRUE(result.ok()) << result.message;
    EXPECT_EQ(result.records.size(), 3u);
    EXPECT_FALSE(contains_uid(result.records, 1));
    EXPECT_TRUE(contains_uid(result.records, 2));
    EXPECT_TRUE(contains_uid(result.records, 3));
    EXPECT_TRUE(contains_uid(result.records, 4));
    EXPECT_FALSE(contains_uid(result.records, 5));
}

TEST(TradeStatsCalculatorTest, ComputesBasicWinrate) {
    std::vector<TradeRecord> records;
    records.push_back(make_win_record(1, 1000, 10.0, 8.2, "EURUSD", optionx::TradeState::WIN));
    records.push_back(make_win_record(2, 2000, 10.0, -10.0, "EURUSD", optionx::TradeState::LOSS));
    records.push_back(make_win_record(3, 3000, 10.0, 8.2, "EURUSD", optionx::TradeState::WIN));

    optionx::TradeStatsConfig cfg;
    cfg.include_non_terminal = false;
    auto stats_ptr = TradeStatsCalculator::calc(records, cfg);
    auto& stats = *stats_ptr;

    EXPECT_EQ(stats.total.wins, 2u);
    EXPECT_EQ(stats.total.losses, 1u);
    EXPECT_EQ(stats.total.trades, 3u);
    EXPECT_DOUBLE_EQ(stats.total.winrate, 2.0 / 3.0);
    EXPECT_DOUBLE_EQ(stats.total_profit, 6.4);
    EXPECT_DOUBLE_EQ(stats.total_volume, 30.0);
}

TEST(TradeStatsCalculatorTest, ComputesBySymbolAndHour) {
    std::vector<TradeRecord> records;
    records.push_back(make_win_record(1, 3600000, 10.0, 8.2, "EURUSD", optionx::TradeState::WIN));
    records.push_back(make_win_record(2, 7200000, 10.0, -10.0, "GBPUSD", optionx::TradeState::LOSS));

    optionx::TradeStatsConfig cfg;
    cfg.time_zone = optionx::TradeTimeZone::utc();
    cfg.include_non_terminal = false;
    auto stats_ptr = TradeStatsCalculator::calc(records, cfg);
    auto& stats = *stats_ptr;

    EXPECT_EQ(stats.by_symbol.count("EURUSD"), 1u);
    EXPECT_EQ(stats.by_symbol.at("EURUSD").wins, 1u);
    EXPECT_EQ(stats.by_hour[1].wins, 1u);  // 3600000 ms = 1 hour
    EXPECT_EQ(stats.by_hour[2].losses, 1u);
}

TEST(TradeStatsCalculatorTest, NamedZoneUsesDstForLocalHourBuckets) {
    std::vector<TradeRecord> records;
    records.push_back(make_win_record(
        1,
        time_shield::to_timestamp_ms(2026, 7, 1, 12, 0, 0),
        10.0,
        8.2,
        "EURUSD",
        optionx::TradeState::WIN));

    optionx::TradeStatsConfig named_cfg;
    named_cfg.time_zone = optionx::TradeTimeZone::named(time_shield::CET);
    named_cfg.include_non_terminal = false;
    const auto named_stats = TradeStatsCalculator::calc(records, named_cfg);

    optionx::TradeStatsConfig fixed_cfg;
    fixed_cfg.time_zone = optionx::TradeTimeZone::fixed_offset(time_shield::SEC_PER_HOUR);
    fixed_cfg.include_non_terminal = false;
    const auto fixed_stats = TradeStatsCalculator::calc(records, fixed_cfg);

    EXPECT_EQ(named_stats->by_hour[14].wins, 1u);
    EXPECT_EQ(named_stats->by_hour[13].wins, 0u);
    EXPECT_EQ(fixed_stats->by_hour[13].wins, 1u);
    EXPECT_EQ(fixed_stats->by_hour[14].wins, 0u);
}

TEST(TradeStatsCalculatorTest, RepeatedDstHourUsesDistinctHourlyProfitBuckets) {
    std::vector<TradeRecord> records;
    records.push_back(make_win_record(
        1,
        time_shield::to_timestamp_ms(2026, 10, 25, 0, 30, 0),
        10.0,
        8.0,
        "EURUSD",
        optionx::TradeState::WIN));
    records.push_back(make_win_record(
        2,
        time_shield::to_timestamp_ms(2026, 10, 25, 1, 30, 0),
        10.0,
        9.0,
        "EURUSD",
        optionx::TradeState::WIN));

    optionx::TradeStatsConfig cfg;
    cfg.time_zone = optionx::TradeTimeZone::named(time_shield::CET);
    cfg.include_non_terminal = false;
    const auto stats = TradeStatsCalculator::calc(records, cfg);

    ASSERT_EQ(stats->hourly_profit.x_time.size(), 2u);
    EXPECT_EQ(
        stats->hourly_profit.x_time[0],
        time_shield::to_timestamp_ms(2026, 10, 25, 0, 0, 0));
    EXPECT_EQ(
        stats->hourly_profit.x_time[1],
        time_shield::to_timestamp_ms(2026, 10, 25, 1, 0, 0));
    EXPECT_DOUBLE_EQ(stats->hourly_profit.y_value[0], 8.0);
    EXPECT_DOUBLE_EQ(stats->hourly_profit.y_value[1], 9.0);
}

TEST(TradeStatsCalculatorTest, DrawdownAndEquityCurve) {
    std::vector<TradeRecord> records;
    records.push_back(make_win_record(1, 1000, 10.0, 8.2, "EURUSD", optionx::TradeState::WIN));
    records.push_back(make_win_record(2, 2000, 10.0, -10.0, "EURUSD", optionx::TradeState::LOSS));
    records.push_back(make_win_record(3, 3000, 10.0, -10.0, "EURUSD", optionx::TradeState::LOSS));

    optionx::TradeStatsConfig cfg;
    cfg.start_balance = 100.0;
    cfg.include_non_terminal = false;
    auto stats_ptr = TradeStatsCalculator::calc(records, cfg);
    auto& stats = *stats_ptr;

    EXPECT_EQ(stats.equity_curve.y_value.size(), 3u);
    EXPECT_DOUBLE_EQ(stats.equity_curve.y_value[0], 108.2);
    EXPECT_DOUBLE_EQ(stats.equity_curve.y_value[1], 98.2);
    EXPECT_DOUBLE_EQ(stats.equity_curve.y_value[2], 88.2);
    EXPECT_DOUBLE_EQ(stats.max_absolute_drawdown, 20.0); // 108.2 - 88.2
    EXPECT_NEAR(stats.max_relative_drawdown, 20.0 / 108.2, 0.001);
}

TEST(TradeStatsCalculatorTest, AsIsInputSortsRealizedCurvesAndSeries) {
    std::vector<TradeRecord> records;
    records.push_back(make_win_record(3, 3000, 10.0, -10.0, "EURUSD", optionx::TradeState::LOSS));
    records.push_back(make_win_record(1, 1000, 10.0, 8.0, "EURUSD", optionx::TradeState::WIN));
    records.push_back(make_win_record(2, 2000, 10.0, 7.0, "EURUSD", optionx::TradeState::WIN));

    optionx::TradeStatsConfig cfg;
    cfg.start_balance = 100.0;
    cfg.include_non_terminal = false;
    cfg.input_order = optionx::TradeStatsInputOrder::AS_IS;
    auto stats_ptr = TradeStatsCalculator::calc(records, cfg);
    auto& stats = *stats_ptr;

    ASSERT_EQ(stats.equity_curve.x_time.size(), 3u);
    EXPECT_EQ(stats.equity_curve.x_time[0], records[1].close_date);
    EXPECT_EQ(stats.equity_curve.x_time[1], records[2].close_date);
    EXPECT_EQ(stats.equity_curve.x_time[2], records[0].close_date);

    EXPECT_DOUBLE_EQ(stats.equity_curve.y_value[0], 108.0);
    EXPECT_DOUBLE_EQ(stats.equity_curve.y_value[1], 115.0);
    EXPECT_DOUBLE_EQ(stats.equity_curve.y_value[2], 105.0);

    EXPECT_EQ(stats.series.max_win_series, 2u);
    EXPECT_EQ(stats.series.current_series, 1u);
    EXPECT_FALSE(stats.series.current_is_win);
}

TEST(TradeStatsCalculatorTest, PlaceDateAscStillUsesResultTimeForRealizedCurve) {
    std::vector<TradeRecord> records;
    records.push_back(make_win_record(1, 1000, 10.0, -100.0, "EURUSD", optionx::TradeState::LOSS));
    records.back().close_date = 5000;
    records.push_back(make_win_record(2, 2000, 10.0, 50.0, "EURUSD", optionx::TradeState::WIN));
    records.back().close_date = 3000;

    optionx::TradeStatsConfig cfg;
    cfg.start_balance = 100.0;
    cfg.include_non_terminal = false;
    cfg.input_order = optionx::TradeStatsInputOrder::PLACE_DATE_ASC;
    auto stats_ptr = TradeStatsCalculator::calc(records, cfg);
    auto& stats = *stats_ptr;

    ASSERT_EQ(stats.equity_curve.x_time.size(), 2u);
    EXPECT_EQ(stats.equity_curve.x_time[0], records[1].close_date);
    EXPECT_EQ(stats.equity_curve.x_time[1], records[0].close_date);
    EXPECT_DOUBLE_EQ(stats.equity_curve.y_value[0], 150.0);
    EXPECT_DOUBLE_EQ(stats.equity_curve.y_value[1], 50.0);
    EXPECT_DOUBLE_EQ(stats.max_absolute_drawdown, 100.0);
}

TEST(TradeStatsCalculatorTest, SameCloseDateAggregatesRealizedProfitBeforeDrawdown) {
    std::vector<TradeRecord> records;
    records.push_back(make_win_record(1, 1000, 10.0, -100.0, "EURUSD", optionx::TradeState::LOSS));
    records.push_back(make_win_record(2, 2000, 10.0, 100.0, "EURUSD", optionx::TradeState::WIN));
    records[0].close_date = 5000;
    records[1].close_date = 5000;

    optionx::TradeStatsConfig cfg;
    cfg.start_balance = 1000.0;
    cfg.include_non_terminal = false;
    auto stats_ptr = TradeStatsCalculator::calc(records, cfg);
    auto& stats = *stats_ptr;

    ASSERT_EQ(stats.equity_curve.x_time.size(), 1u);
    EXPECT_EQ(stats.equity_curve.x_time[0], 5000);
    EXPECT_DOUBLE_EQ(stats.equity_curve.y_value[0], 1000.0);
    ASSERT_EQ(stats.profit_curve.y_value.size(), 1u);
    EXPECT_DOUBLE_EQ(stats.profit_curve.y_value[0], 0.0);
    EXPECT_DOUBLE_EQ(stats.max_absolute_drawdown, 0.0);
}

TEST(TradeStatsCalculatorTest, SameCloseDateSeriesUsesDecisionTimeOrder) {
    std::vector<TradeRecord> records;
    records.push_back(make_win_record(3, 2000, 10.0, -10.0, "EURUSD", optionx::TradeState::LOSS));
    records.push_back(make_win_record(1, 1000, 10.0, 8.0, "EURUSD", optionx::TradeState::WIN));
    records.push_back(make_win_record(2, 3000, 10.0, 8.0, "EURUSD", optionx::TradeState::WIN));
    for (auto& record : records) {
        record.close_date = 5000;
    }

    optionx::TradeStatsConfig cfg;
    cfg.include_non_terminal = false;
    auto stats_ptr = TradeStatsCalculator::calc(records, cfg);
    auto& stats = *stats_ptr;

    EXPECT_EQ(stats.series.max_win_series, 1u);
    EXPECT_EQ(stats.series.max_loss_series, 1u);
    EXPECT_EQ(stats.series.total_win_series, 2u);
    EXPECT_EQ(stats.series.total_loss_series, 1u);
    EXPECT_EQ(stats.series.current_series, 1u);
    EXPECT_TRUE(stats.series.current_is_win);
}

TEST(TradeStatsCalculatorTest, SweepLineAggregatesSameTimestampEventsBeforeDrawdown) {
    std::vector<TradeRecord> records;
    records.push_back(make_win_record(1, 1000, 100.0, 10.0, "EURUSD", optionx::TradeState::WIN));
    records.back().open_date = 5000;
    records.back().close_date = 5000;

    optionx::TradeStatsConfig cfg;
    cfg.start_balance = 1000.0;
    cfg.balance_mode = optionx::TradeStatsBalanceMode::SWEEP_LINE;
    cfg.include_non_terminal = false;
    auto stats_ptr = TradeStatsCalculator::calc(records, cfg);
    auto& stats = *stats_ptr;

    ASSERT_EQ(stats.free_funds_curve.x_time.size(), 1u);
    EXPECT_EQ(stats.free_funds_curve.x_time[0], 5000);
    EXPECT_DOUBLE_EQ(stats.free_funds_curve.y_value[0], 1010.0);
    EXPECT_DOUBLE_EQ(stats.max_absolute_drawdown_free, 0.0);
    EXPECT_DOUBLE_EQ(stats.max_relative_drawdown_free, 0.0);
}

TEST(TradeStatsCalculatorTest, ProfitPercentCurveUsesStartBalance) {
    std::vector<TradeRecord> records;
    records.push_back(make_win_record(1, 1000, 10.0, 10.0, "EURUSD", optionx::TradeState::WIN));
    records.push_back(make_win_record(2, 2000, 10.0, -5.0, "EURUSD", optionx::TradeState::LOSS));

    optionx::TradeStatsConfig cfg;
    cfg.start_balance = 100.0;
    cfg.include_non_terminal = false;
    auto stats_ptr = TradeStatsCalculator::calc(records, cfg);
    auto& stats = *stats_ptr;

    ASSERT_EQ(stats.profit_percent_curve.y_value.size(), 2u);
    EXPECT_DOUBLE_EQ(stats.profit_percent_curve.y_value[0], 10.0);
    EXPECT_DOUBLE_EQ(stats.profit_percent_curve.y_value[1], 5.0);
}

TEST(TradeStatsCalculatorTest, ProfitPercentCurveEmptyWithoutStartBalance) {
    std::vector<TradeRecord> records;
    records.push_back(make_win_record(1, 1000, 10.0, 10.0, "EURUSD", optionx::TradeState::WIN));

    optionx::TradeStatsConfig cfg;
    cfg.start_balance = 0.0;
    cfg.include_non_terminal = false;
    auto stats_ptr = TradeStatsCalculator::calc(records, cfg);

    EXPECT_TRUE(stats_ptr->profit_percent_curve.y_value.empty());
}

TEST(TradeStatsCalculatorTest, RealizedCurveFallsBackToOpenDateWhenCloseDateMissing) {
    std::vector<TradeRecord> records;
    records.push_back(make_win_record(1, 1000, 10.0, 8.0, "EURUSD", optionx::TradeState::WIN));
    records.back().close_date = 0;
    records.push_back(make_win_record(2, 2000, 10.0, 7.0, "EURUSD", optionx::TradeState::WIN));

    optionx::TradeStatsConfig cfg;
    cfg.start_balance = 100.0;
    cfg.include_non_terminal = false;
    auto stats_ptr = TradeStatsCalculator::calc(records, cfg);
    auto& stats = *stats_ptr;

    ASSERT_EQ(stats.equity_curve.x_time.size(), 2u);
    EXPECT_EQ(stats.equity_curve.x_time[0], records[0].open_date);
    EXPECT_EQ(stats.equity_curve.x_time[1], records[1].close_date);
    EXPECT_DOUBLE_EQ(stats.equity_curve.y_value[0], 108.0);
    EXPECT_DOUBLE_EQ(stats.equity_curve.y_value[1], 115.0);
}

TEST(TradeStatsCalculatorTest, RecordBalanceModeUsesSnapshotsWithoutStartBalance) {
    std::vector<TradeRecord> records;
    records.push_back(make_win_record(1, 1000, 10.0, -5.0, "EURUSD", optionx::TradeState::LOSS));
    records.back().set_open_balance(1000.0);
    records.back().set_close_balance(995.0);
    records.push_back(make_win_record(2, 2000, 10.0, 15.0, "EURUSD", optionx::TradeState::WIN));
    records.back().set_open_balance(995.0);
    records.back().set_close_balance(1010.0);

    optionx::TradeStatsConfig cfg;
    cfg.equity_mode = optionx::TradeStatsEquityMode::RECORD_BALANCE;
    cfg.include_non_terminal = false;
    auto stats_ptr = TradeStatsCalculator::calc(records, cfg);
    auto& stats = *stats_ptr;

    ASSERT_EQ(stats.equity_curve.y_value.size(), 2u);
    EXPECT_DOUBLE_EQ(stats.equity_curve.y_value[0], 995.0);
    EXPECT_DOUBLE_EQ(stats.equity_curve.y_value[1], 1010.0);
    ASSERT_EQ(stats.profit_curve.y_value.size(), 2u);
    EXPECT_DOUBLE_EQ(stats.profit_curve.y_value[0], -5.0);
    EXPECT_DOUBLE_EQ(stats.profit_curve.y_value[1], 10.0);
    ASSERT_EQ(stats.profit_percent_curve.y_value.size(), 2u);
    EXPECT_DOUBLE_EQ(stats.profit_percent_curve.y_value[0], -0.5);
    EXPECT_DOUBLE_EQ(stats.profit_percent_curve.y_value[1], 1.0);
    EXPECT_DOUBLE_EQ(stats.max_absolute_drawdown, 5.0);
}

TEST(TradeStatsCalculatorTest, PortfolioBalanceModeCombinesAccountsInBaseCurrency) {
    std::vector<TradeRecord> records;
    records.push_back(make_win_record(2, 2000, 1000.0, -900.0, "EURUSD", optionx::TradeState::LOSS));
    records.back().account_id = 2;
    records.back().currency = optionx::CurrencyType::RUB;
    records.back().set_open_balance(90000.0);
    records.back().set_close_balance(89100.0);

    records.push_back(make_win_record(1, 1000, 10.0, 10.0, "EURUSD", optionx::TradeState::WIN));
    records.back().account_id = 1;
    records.back().currency = optionx::CurrencyType::USD;
    records.back().set_open_balance(1000.0);
    records.back().set_close_balance(1010.0);

    optionx::TradeStatsConfig cfg;
    cfg.equity_mode = optionx::TradeStatsEquityMode::PORTFOLIO_BALANCE;
    cfg.currency_matrix.base_currency = optionx::CurrencyType::USD;
    cfg.currency_matrix.set_rate(optionx::CurrencyType::RUB, optionx::CurrencyType::USD, 0.01);
    cfg.include_non_terminal = false;
    auto stats_ptr = TradeStatsCalculator::calc(records, cfg);
    auto& stats = *stats_ptr;

    ASSERT_EQ(stats.equity_curve.x_time.size(), 2u);
    EXPECT_EQ(stats.equity_curve.x_time[0], records[1].close_date);
    EXPECT_EQ(stats.equity_curve.x_time[1], records[0].close_date);
    EXPECT_DOUBLE_EQ(stats.equity_curve.y_value[0], 1010.0);
    EXPECT_DOUBLE_EQ(stats.equity_curve.y_value[1], 1901.0);

    ASSERT_EQ(stats.profit_curve.y_value.size(), 2u);
    EXPECT_DOUBLE_EQ(stats.profit_curve.y_value[0], 10.0);
    EXPECT_DOUBLE_EQ(stats.profit_curve.y_value[1], 1.0);
    ASSERT_EQ(stats.profit_percent_curve.y_value.size(), 2u);
    EXPECT_DOUBLE_EQ(stats.profit_percent_curve.y_value[1], (1.0 / 1900.0) * 100.0);
    EXPECT_DOUBLE_EQ(stats.total_profit, 1.0);
}

TEST(TradeStatsCalculatorTest, RecordBalanceModePreservesZeroCloseBalanceSnapshot) {
    std::vector<TradeRecord> records;
    records.push_back(make_win_record(1, 1000, 100.0, -100.0, "EURUSD", optionx::TradeState::LOSS));
    records.back().set_open_balance(100.0);
    records.back().set_close_balance(0.0);

    optionx::TradeStatsConfig cfg;
    cfg.equity_mode = optionx::TradeStatsEquityMode::RECORD_BALANCE;
    cfg.include_non_terminal = false;
    auto stats_ptr = TradeStatsCalculator::calc(records, cfg);
    auto& stats = *stats_ptr;

    ASSERT_EQ(stats.equity_curve.y_value.size(), 1u);
    EXPECT_DOUBLE_EQ(stats.equity_curve.y_value[0], 0.0);
    ASSERT_EQ(stats.profit_curve.y_value.size(), 1u);
    EXPECT_DOUBLE_EQ(stats.profit_curve.y_value[0], -100.0);
    ASSERT_EQ(stats.profit_percent_curve.y_value.size(), 1u);
    EXPECT_DOUBLE_EQ(stats.profit_percent_curve.y_value[0], -100.0);
    EXPECT_DOUBLE_EQ(stats.max_absolute_drawdown, 100.0);
    EXPECT_DOUBLE_EQ(stats.max_relative_drawdown, 1.0);
}

TEST(TradeStatsCalculatorTest, PortfolioBalanceModePreservesZeroAccountSnapshot) {
    std::vector<TradeRecord> records;
    records.push_back(make_win_record(1, 1000, 100.0, -100.0, "EURUSD", optionx::TradeState::LOSS));
    records.back().account_id = 1;
    records.back().set_open_balance(100.0);
    records.back().set_close_balance(0.0);

    records.push_back(make_win_record(2, 2000, 10.0, 10.0, "EURUSD", optionx::TradeState::WIN));
    records.back().account_id = 2;
    records.back().set_open_balance(1000.0);
    records.back().set_close_balance(1010.0);

    optionx::TradeStatsConfig cfg;
    cfg.equity_mode = optionx::TradeStatsEquityMode::PORTFOLIO_BALANCE;
    cfg.include_non_terminal = false;
    auto stats_ptr = TradeStatsCalculator::calc(records, cfg);
    auto& stats = *stats_ptr;

    ASSERT_EQ(stats.equity_curve.y_value.size(), 2u);
    EXPECT_DOUBLE_EQ(stats.equity_curve.y_value[0], 0.0);
    EXPECT_DOUBLE_EQ(stats.equity_curve.y_value[1], 1010.0);
    ASSERT_EQ(stats.profit_curve.y_value.size(), 2u);
    EXPECT_DOUBLE_EQ(stats.profit_curve.y_value[0], -100.0);
    EXPECT_DOUBLE_EQ(stats.profit_curve.y_value[1], -90.0);
}

TEST(TradeStatsCalculatorTest, SelectionAppliesToMonetaryStats) {
    std::vector<TradeRecord> records;
    records.push_back(make_win_record(1, 1000, 10.0, 8.0, "EURUSD", optionx::TradeState::WIN));
    records.push_back(make_win_record(2, 2000, 100.0, 80.0, "EURUSD", optionx::TradeState::WIN));

    optionx::TradeStatsConfig cfg;
    cfg.selection = optionx::TradeStatsSelection::FIRST_MM_STEP;
    cfg.include_non_terminal = false;
    auto stats_ptr = TradeStatsCalculator::calc(records, cfg);
    auto& stats = *stats_ptr;

    EXPECT_EQ(stats.total.trades, 1u);
    EXPECT_EQ(stats.total.wins, 1u);
    EXPECT_DOUBLE_EQ(stats.total_volume, 10.0);
    EXPECT_DOUBLE_EQ(stats.total_profit, 8.0);
    EXPECT_DOUBLE_EQ(stats.average_amount, 10.0);
    EXPECT_DOUBLE_EQ(stats.average_profit, 8.0);
    EXPECT_DOUBLE_EQ(stats.average_profit_per_trade, 8.0);
    EXPECT_EQ(stats.by_mm_step.count(0), 1u);
    EXPECT_EQ(stats.by_mm_step.count(1), 0u);
}

TEST(TradeMetaStatsCalculatorTest, CollectsUniqueSymbols) {
    std::vector<TradeRecord> records;
    records.push_back(make_win_record(1, 1000, 10.0, 8.2, "EURUSD", optionx::TradeState::WIN));
    records.push_back(make_win_record(2, 2000, 10.0, -10.0, "GBPUSD", optionx::TradeState::LOSS));

    optionx::TradeStatsConfig cfg;
    cfg.include_non_terminal = false;
    auto meta = TradeMetaStatsCalculator::calc(records, cfg);

    EXPECT_EQ(meta.symbols.size(), 2u);
    EXPECT_EQ(meta.symbol_stats.size(), 2u);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
