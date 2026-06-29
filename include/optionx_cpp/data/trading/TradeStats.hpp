#pragma once
#ifndef _OPTIONX_TRADE_STATS_HPP_INCLUDED
#define _OPTIONX_TRADE_STATS_HPP_INCLUDED

/// \file TradeStats.hpp
/// \brief DTOs for trade statistics, charting, and meta-analysis.

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "enums.hpp"
#include "TradeRecord.hpp"
#include "TradeRecordFilter.hpp"
#include "TradeTimeZone.hpp"
#include "utils/trade_state_utils.hpp"

namespace optionx {

    /// \enum TradeStatsSelection
    /// \brief Controls which subset of trades contributes to selected result statistics.
    enum class TradeStatsSelection {
        ALL_TRADES,      ///< Include every trade.
        FIRST_MM_STEP,   ///< Include only trades with mm_step == 0.
        LAST_IN_GROUP    ///< Include only trades with the last-in-group flag set.
    };

    /// \class TradeWinrateStats
    /// \brief Aggregated counts for win/loss/standoff/refund/error outcomes.
    class TradeWinrateStats {
    public:
        std::uint64_t wins = 0;
        std::uint64_t losses = 0;
        std::uint64_t standoffs = 0;
        std::uint64_t refunds = 0;
        std::uint64_t errors = 0;
        std::uint64_t trades = 0;

        double winrate = 0.0;       ///< wins / (wins + losses), or 0 when denominator is 0.
        double result_rate = 0.0;   ///< (wins + losses + standoffs + refunds) / trades.

        /// \brief Adds one trade to the counters based on its terminal state.
        void add(const TradeRecord& record) {
            ++trades;
            if (record.trade_state == TradeState::WIN) {
                ++wins;
            } else if (record.trade_state == TradeState::LOSS) {
                ++losses;
            } else if (record.trade_state == TradeState::STANDOFF) {
                ++standoffs;
            } else if (record.trade_state == TradeState::REFUND) {
                ++refunds;
            } else if (is_error_trade_state(record.trade_state)) {
                ++errors;
            }
        }

        /// \brief Recalculates derived rates after all adds are done.
        void calc() noexcept {
            const auto decisive = wins + losses;
            winrate = (decisive > 0) ? (static_cast<double>(wins) / static_cast<double>(decisive)) : 0.0;
            const auto terminal = wins + losses + standoffs + refunds;
            result_rate = (trades > 0) ? (static_cast<double>(terminal) / static_cast<double>(trades)) : 0.0;
        }
    };

    /// \class TradeChartData
    /// \brief Simple x/y chart series with optional string labels.
    class TradeChartData {
    public:
        std::vector<std::int64_t> x_time;   ///< X-axis timestamps in milliseconds.
        std::vector<double>       y_value;  ///< Y-axis numeric values.
        std::vector<std::string>  x_label;  ///< Optional human-readable X labels.
    };

    /// \struct TradeSeriesStats
    /// \brief Win/loss streak statistics.
    struct TradeSeriesStats {
        std::uint64_t max_win_series = 0;
        std::uint64_t max_loss_series = 0;
        std::uint64_t avg_win_series = 0;
        std::uint64_t avg_loss_series = 0;
        std::uint64_t total_win_series = 0;
        std::uint64_t total_loss_series = 0;
        std::uint64_t current_series = 0;
        bool current_is_win = false;
    };

    /// \struct TradePingStats
    /// \brief Result distribution grouped by ping bucket.
    struct TradePingStats {
        std::map<std::int64_t, TradeWinrateStats> by_ping_ms; ///< Key = ping rounded to ms.
    };

    /// \class TradeStats
    /// \brief Complete statistical summary over a collection of trades.
    class TradeStats {
    public:
        TradeWinrateStats total;
        TradeWinrateStats buy;
        TradeWinrateStats sell;

        std::map<std::string, TradeWinrateStats>  by_symbol;
        std::map<std::string, TradeWinrateStats>  by_signal;
        std::map<PlatformType, TradeWinrateStats> by_platform;
        std::map<CurrencyType, TradeWinrateStats> by_currency;
        std::map<std::int64_t, TradeWinrateStats> by_duration;
        std::map<std::int32_t, TradeWinrateStats> by_mm_step;

        std::array<TradeWinrateStats, 86400> by_sec;       ///< Index = second of day (0..86399).
        std::array<TradeWinrateStats, 1440>  by_min;       ///< Index = minute of day (0..1439).
        std::array<TradeWinrateStats, 24>    by_hour;      ///< Index = hour (0..23).
        std::array<TradeWinrateStats, 7>     by_weekday;   ///< Index = weekday (0=Sun..6=Sat).
        std::array<TradeWinrateStats, 31>    by_month_day; ///< Index = day of month (0..30).
        std::array<TradeWinrateStats, 12>    by_month;     ///< Index = month (0..11, 0=Jan).
        std::array<TradeWinrateStats, 60>    by_second;    ///< Index = second (0..59).

        double total_volume = 0.0;            ///< Sum of all trade amounts.
        double total_profit = 0.0;            ///< Sum of net PnL across result-state trades.
        double gross_profit = 0.0;            ///< Sum of positive profits.
        double gross_loss = 0.0;              ///< Sum of negative profits (absolute).
        double profit_factor = 0.0;           ///< gross_profit / gross_loss (0 when no loss).

        double average_amount = 0.0;          ///< total_volume / selected monetary result trades count.
        double average_profit = 0.0;          ///< total_profit / selected monetary result trades count.
        double average_profit_per_trade = 0.0; ///< Alias-style per-trade average over selected monetary result trades.

        double max_profit_trade = 0.0;
        double max_loss_trade = 0.0;

        double max_absolute_drawdown = 0.0;
        double max_relative_drawdown = 0.0;
        std::int64_t max_drawdown_date = 0;

        double max_absolute_drawdown_free = 0.0;
        double max_relative_drawdown_free = 0.0;
        std::int64_t max_drawdown_date_free = 0;

        TradeChartData equity_curve;
        TradeChartData free_funds_curve;
        TradeChartData profit_curve;
        TradeChartData profit_percent_curve; ///< Cumulative net PnL as percent of start_balance when start_balance > 0.
        TradeChartData daily_profit;
        TradeChartData hourly_profit;

        TradeSeriesStats series;
        TradePingStats ping;
    };

    /// \enum TradeStatsBalanceMode
    /// \brief Controls optional free-funds curve computation.
    enum class TradeStatsBalanceMode {
        SIMPLE,     ///< Do not build an event-driven free-funds curve.
        SWEEP_LINE  ///< Build free funds from open/close cash-lock events.
    };

    /// \enum TradeStatsEquityMode
    /// \brief Selects the source used for equity/profit/drawdown curves.
    enum class TradeStatsEquityMode {
        SYNTHETIC_PROFIT, ///< start_balance plus cumulative realized profit.
        RECORD_BALANCE,   ///< Single-account curve from TradeRecord close balance snapshots.
        PORTFOLIO_BALANCE ///< Multi-account curve from per-account balance snapshots.
    };

    /// \class TradeCurrencyConversionMatrix
    /// \brief Static currency conversion rates for portfolio statistics.
    class TradeCurrencyConversionMatrix {
    public:
        CurrencyType base_currency = CurrencyType::UNKNOWN; ///< Reporting currency for converted values.
        std::map<std::pair<CurrencyType, CurrencyType>, double> rates; ///< Pair(from, to) -> multiplier.

        /// \brief Sets a direct conversion multiplier.
        void set_rate(CurrencyType from, CurrencyType to, double rate) {
            rates[{from, to}] = rate;
        }

        /// \brief Converts a value between currencies, using inverse rates when possible.
        double convert(double value, CurrencyType from, CurrencyType to) const {
            if (from == to || from == CurrencyType::UNKNOWN || to == CurrencyType::UNKNOWN) {
                return value;
            }

            const auto direct = rates.find({from, to});
            if (direct != rates.end()) {
                return value * direct->second;
            }

            const auto inverse = rates.find({to, from});
            if (inverse != rates.end() && inverse->second != 0.0) {
                return value / inverse->second;
            }

            return value;
        }

        /// \brief Converts a value to base_currency when it is configured.
        double convert_to_base(double value, CurrencyType from) const {
            return convert(value, from, base_currency);
        }
    };

    /// \enum TradeStatsInputOrder
    /// \brief Legacy hint about the order of input records.
    enum class TradeStatsInputOrder {
        AS_IS,           ///< Records may be in any order; realized curves and series are sorted internally.
        PLACE_DATE_ASC   ///< Deprecated hint; realized statistics are still ordered by result timestamp.
    };

    /// \class TradeStatsConfig
    /// \brief Configuration controlling which trades are counted and how.
    class TradeStatsConfig {
    public:
        TradeStatsSelection selection = TradeStatsSelection::ALL_TRADES;
        double start_balance = 0.0;
        TradeTimeZone time_zone;
        TradeRecordFilter filter;
        bool include_errors = false;
        bool include_non_terminal = false;
        std::function<double(double value, CurrencyType from)> convert; ///< Optional currency converter.
        std::function<double(double value, CurrencyType from, CurrencyType to, std::int64_t timestamp_ms)>
            convert_currency; ///< Optional timestamp-aware converter.
        TradeCurrencyConversionMatrix currency_matrix; ///< Static rates for multi-currency statistics.
        TradeStatsEquityMode equity_mode = TradeStatsEquityMode::SYNTHETIC_PROFIT;
        TradeStatsBalanceMode balance_mode = TradeStatsBalanceMode::SWEEP_LINE;
        TradeStatsInputOrder input_order = TradeStatsInputOrder::AS_IS;
    };

    /// \class TradeMetaStats
    /// \brief Meta-analysis: available values and per-value statistics.
    class TradeMetaStats {
    public:
        std::vector<PlatformType> platforms;
        std::vector<AccountType> accounts;
        std::vector<CurrencyType> currencies;
        std::vector<std::string> symbols;
        std::vector<std::string> signals;
        std::vector<std::int64_t> durations;
        bool has_demo = false;
        bool has_real = false;

        std::vector<TradeStats> platform_stats;
        std::vector<TradeStats> account_stats;
        std::vector<TradeStats> currency_stats;
        std::vector<TradeStats> signal_stats;
        std::vector<TradeStats> symbol_stats;
        std::vector<TradeStats> duration_stats;
        std::vector<TradeStats> hour_stats;
        std::vector<TradeStats> weekday_stats;
    };

} // namespace optionx

#endif // _OPTIONX_TRADE_STATS_HPP_INCLUDED
