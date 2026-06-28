#pragma once
#ifndef _OPTIONX_TRADE_STATS_CALCULATOR_HPP_INCLUDED
#define _OPTIONX_TRADE_STATS_CALCULATOR_HPP_INCLUDED

/// \file TradeStatsCalculator.hpp
/// \brief Computes TradeStats from a vector of TradeRecord objects.

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <queue>

#include <time_shield.hpp>

#include "data/trading.hpp"
#include "TradeRecordFilterMatcher.hpp"
#include "utils/trade_state_utils.hpp"

namespace optionx::storage {

    /// \class TradeStatsCalculator
    /// \brief Static calculator producing a full statistical summary over trade records.
    class TradeStatsCalculator {
    public:
        /// \brief Calculates statistics from a collection of trade records.
        /// \param records Input trade records.
        /// \param config Optional filtering and conversion configuration.
        /// \return Heap-allocated TradeStats (large struct — never returned by value).
        static std::unique_ptr<optionx::TradeStats> calc(
                const std::vector<optionx::TradeRecord>& records,
                const optionx::TradeStatsConfig& config = {}) {
            auto stats_ptr = std::make_unique<optionx::TradeStats>();
            auto& stats = *stats_ptr;

            double running_balance = config.start_balance;
            double peak_balance = config.start_balance;

            std::map<std::int64_t, double> daily_profit_map;
            std::map<std::int64_t, double> hourly_profit_map;

            std::uint64_t win_series_total_len = 0;
            std::uint64_t loss_series_total_len = 0;
            std::uint64_t win_series_count = 0;
            std::uint64_t loss_series_count = 0;
            std::uint64_t current_series = 0;
            bool current_is_win = false;

            std::uint64_t volume_trade_count = 0;

            struct Event {
                std::int64_t ts;
                double delta;
                bool operator<(const Event& other) const noexcept {
                    return ts > other.ts; // min-heap for std::priority_queue
                }
            };
            std::vector<Event> events;
            events.reserve(records.size() * 2);

            for (const auto& rec : records) {
                // 1. Filter predicate (applies to both outcome and monetary)
                if (!TradeRecordFilterMatcher::match_filter(rec, config.filter, config.time_zone)) {
                    continue;
                }

                // 2. Determine if this record contributes to OUTCOME statistics
                bool include_outcome = true;
                if (config.selection == optionx::TradeStatsSelection::FIRST_MM_STEP && rec.mm_step != 0) {
                    include_outcome = false;
                }
                if (config.selection == optionx::TradeStatsSelection::LAST_IN_GROUP) {
                    if (!rec.last_in_group()) {
                        include_outcome = false;
                    }
                }

                // 3. Error / terminal inclusion for outcome stats
                if (include_outcome) {
                    if (!config.include_errors && optionx::is_error_trade_state(rec.trade_state)) {
                        include_outcome = false;
                    }
                    if (!config.include_non_terminal && !optionx::is_terminal_trade_state(rec.trade_state)) {
                        include_outcome = false;
                    }
                }

                // 4. Currency conversion (for monetary stats)
                double amount = rec.amount;
                double profit = rec.profit;
                if (config.convert) {
                    amount = config.convert(amount, rec.currency);
                    profit = config.convert(profit, rec.currency);
                }

                // 5. Monetary aggregations (all result-state trades always contribute)
                if (optionx::is_result_state(rec.trade_state)) {
                    ++volume_trade_count;
                    stats.total_volume += amount;
                    stats.total_profit += profit;
                    if (profit > 0.0) stats.gross_profit += profit;
                    if (profit < 0.0) stats.gross_loss += -profit;

                    stats.max_profit_trade = std::max(stats.max_profit_trade, profit);
                    stats.max_loss_trade = std::min(stats.max_loss_trade, profit);

                    // Equity / profit curves (classic realized-profit mode)
                    const auto curve_ts = rec.close_date > 0 ? rec.close_date : rec.open_date;
                    running_balance += profit;
                    peak_balance = std::max(peak_balance, running_balance);

                    if (curve_ts > 0) {
                        stats.equity_curve.x_time.push_back(curve_ts);
                        stats.equity_curve.y_value.push_back(running_balance);
                        stats.profit_curve.x_time.push_back(curve_ts);
                        stats.profit_curve.y_value.push_back(stats.total_profit);

                        // Daily / hourly profit buckets
                        const auto day_utc =
                            config.time_zone.start_of_local_day_utc_ms(curve_ts);
                        daily_profit_map[day_utc] += profit;

                        const auto hour_utc =
                            config.time_zone.start_of_local_hour_utc_ms(curve_ts);
                        hourly_profit_map[hour_utc] += profit;
                    }

                    // Drawdown (classic)
                    const auto dd = peak_balance - running_balance;
                    if (dd > stats.max_absolute_drawdown) {
                        stats.max_absolute_drawdown = dd;
                        stats.max_drawdown_date = curve_ts;
                    }
                    if (peak_balance > 0.0) {
                        stats.max_relative_drawdown = std::max(stats.max_relative_drawdown, dd / peak_balance);
                    }

                    // Ping stats
                    if (rec.ping > 0) {
                        stats.ping.by_ping_ms[rec.ping].add(rec);
                    }

                    // Sweep-line events for free-funds curve
                    if (config.balance_mode == optionx::TradeStatsBalanceMode::SWEEP_LINE) {
                        if (rec.open_date > 0) {
                            events.push_back({rec.open_date, -amount});
                        }
                        if (curve_ts > 0) {
                            events.push_back({curve_ts, amount + profit});
                        }
                    }
                }

                // 6. Winrate / outcome aggregations (terminal trades only, subject to selection)
                if (include_outcome && optionx::is_terminal_trade_state(rec.trade_state)) {
                    stats.total.add(rec);
                    if (rec.order_type == optionx::OrderType::BUY) stats.buy.add(rec);
                    else if (rec.order_type == optionx::OrderType::SELL) stats.sell.add(rec);

                    if (!rec.symbol.empty()) stats.by_symbol[rec.symbol].add(rec);
                    if (!rec.signal_name.empty()) stats.by_signal[rec.signal_name].add(rec);
                    stats.by_platform[rec.platform_type].add(rec);
                    stats.by_currency[rec.currency].add(rec);
                    if (rec.duration > 0) stats.by_duration[rec.duration].add(rec);
                    stats.by_mm_step[rec.mm_step].add(rec);

                    // Time-of-day buckets
                    const auto ts = optionx::select_timestamp_ms(rec, optionx::TradeRecordTimeField::AUTO);
                    if (ts > 0) {
                        const auto local_ms = config.time_zone.to_local_ms(ts);
                        const auto sec = time_shield::ms_to_sec<time_shield::ts_t>(local_ms);
                        const auto dt = time_shield::to_date_time<time_shield::DateTimeStruct>(sec);
                        const auto sec_of_day = dt.hour * 3600 + dt.min * 60 + dt.sec;
                        const auto min_of_day = dt.hour * 60 + dt.min;

                        if (sec_of_day >= 0 && sec_of_day < 86400) {
                            stats.by_sec[static_cast<std::size_t>(sec_of_day)].add(rec);
                        }
                        if (min_of_day >= 0 && min_of_day < 1440) {
                            stats.by_min[static_cast<std::size_t>(min_of_day)].add(rec);
                        }
                        if (dt.hour >= 0 && dt.hour < 24) {
                            stats.by_hour[static_cast<std::size_t>(dt.hour)].add(rec);
                        }
                        const auto weekday = time_shield::weekday_of_ts(sec);
                        if (weekday >= 0 && weekday < 7) {
                            stats.by_weekday[static_cast<std::size_t>(weekday)].add(rec);
                        }
                        if (dt.day >= 1 && dt.day <= 31) {
                            stats.by_month_day[static_cast<std::size_t>(dt.day - 1)].add(rec);
                        }
                        if (dt.mon >= 1 && dt.mon <= 12) {
                            stats.by_month[static_cast<std::size_t>(dt.mon - 1)].add(rec);
                        }
                        if (dt.sec >= 0 && dt.sec < 60) {
                            stats.by_second[static_cast<std::size_t>(dt.sec)].add(rec);
                        }
                    }

                    // Series tracking
                    if (rec.trade_state == optionx::TradeState::WIN) {
                        if (!current_is_win && current_series > 0) {
                            ++loss_series_count;
                            stats.series.max_loss_series = std::max(stats.series.max_loss_series, current_series);
                        }
                        current_is_win = true;
                        ++current_series;
                        win_series_total_len += current_series;
                    } else if (rec.trade_state == optionx::TradeState::LOSS) {
                        if (current_is_win && current_series > 0) {
                            ++win_series_count;
                            stats.series.max_win_series = std::max(stats.series.max_win_series, current_series);
                        }
                        current_is_win = false;
                        ++current_series;
                        loss_series_total_len += current_series;
                    } else {
                        // Standoff / refund / error: close current series
                        if (current_series > 0) {
                            if (current_is_win) {
                                ++win_series_count;
                                stats.series.max_win_series = std::max(stats.series.max_win_series, current_series);
                            } else {
                                ++loss_series_count;
                                stats.series.max_loss_series = std::max(stats.series.max_loss_series, current_series);
                            }
                        }
                        current_series = 0;
                    }
                }
            }

            // Finalize open series
            if (current_series > 0) {
                if (current_is_win) {
                    stats.series.max_win_series = std::max(stats.series.max_win_series, current_series);
                } else {
                    stats.series.max_loss_series = std::max(stats.series.max_loss_series, current_series);
                }
            }
            stats.series.current_series = current_series;
            stats.series.current_is_win = current_is_win;

            // Recount series properly for averages
            recount_series(records, config, stats.series);

            // Finalize winrate stats
            stats.total.calc();
            stats.buy.calc();
            stats.sell.calc();
            for (auto& kv : stats.by_symbol) kv.second.calc();
            for (auto& kv : stats.by_signal) kv.second.calc();
            for (auto& kv : stats.by_platform) kv.second.calc();
            for (auto& kv : stats.by_currency) kv.second.calc();
            for (auto& kv : stats.by_duration) kv.second.calc();
            for (auto& kv : stats.by_mm_step) kv.second.calc();
            for (auto& kv : stats.ping.by_ping_ms) kv.second.calc();
            for (auto& s : stats.by_sec) s.calc();
            for (auto& s : stats.by_min) s.calc();
            for (auto& s : stats.by_hour) s.calc();
            for (auto& s : stats.by_weekday) s.calc();
            for (auto& s : stats.by_month_day) s.calc();
            for (auto& s : stats.by_month) s.calc();
            for (auto& s : stats.by_second) s.calc();

            // Derived monetary stats
            stats.profit_factor = (stats.gross_loss > 0.0)
                ? (stats.gross_profit / stats.gross_loss)
                : ((stats.gross_profit > 0.0)
                    ? std::numeric_limits<double>::infinity()
                    : 0.0);

            if (volume_trade_count > 0) {
                stats.average_amount = stats.total_volume / static_cast<double>(volume_trade_count);
            }
            if (stats.total.trades > 0) {
                stats.average_profit_per_trade = stats.total_profit / static_cast<double>(stats.total.trades);
            }

            const auto result_count = stats.total.wins + stats.total.losses +
                                      stats.total.standoffs + stats.total.refunds;
            if (result_count > 0) {
                stats.average_profit = stats.total_profit / static_cast<double>(result_count);
            }

            // Fill chart data
            for (const auto& kv : daily_profit_map) {
                stats.daily_profit.x_time.push_back(kv.first);
                stats.daily_profit.y_value.push_back(kv.second);
            }
            for (const auto& kv : hourly_profit_map) {
                stats.hourly_profit.x_time.push_back(kv.first);
                stats.hourly_profit.y_value.push_back(kv.second);
            }

            // Sweep-line free-funds curve
            if (config.balance_mode == optionx::TradeStatsBalanceMode::SWEEP_LINE && !events.empty()) {
                std::sort(events.begin(), events.end(), [](const Event& a, const Event& b) {
                    return a.ts < b.ts;
                });

                double free_funds = config.start_balance;
                double peak_free = config.start_balance;
                std::int64_t last_ts = events.front().ts;

                for (const auto& ev : events) {
                    if (ev.ts != last_ts) {
                        stats.free_funds_curve.x_time.push_back(last_ts);
                        stats.free_funds_curve.y_value.push_back(free_funds);
                        last_ts = ev.ts;
                    }
                    free_funds += ev.delta;
                    peak_free = std::max(peak_free, free_funds);

                    const auto dd = peak_free - free_funds;
                    if (dd > stats.max_absolute_drawdown_free) {
                        stats.max_absolute_drawdown_free = dd;
                        stats.max_drawdown_date_free = ev.ts;
                    }
                    if (peak_free > 0.0) {
                        stats.max_relative_drawdown_free = std::max(
                            stats.max_relative_drawdown_free, dd / peak_free);
                    }
                }
                // Push final point
                stats.free_funds_curve.x_time.push_back(last_ts);
                stats.free_funds_curve.y_value.push_back(free_funds);
            }

            return stats_ptr;
        }

    private:
        static bool include_outcome(
                const optionx::TradeRecord& rec,
                const optionx::TradeStatsConfig& config) noexcept {
            if (!TradeRecordFilterMatcher::match_filter(rec, config.filter, config.time_zone)) {
                return false;
            }
            if (config.selection == optionx::TradeStatsSelection::FIRST_MM_STEP && rec.mm_step != 0) {
                return false;
            }
            if (config.selection == optionx::TradeStatsSelection::LAST_IN_GROUP) {
                if (!rec.last_in_group()) {
                    return false;
                }
            }
            if (!config.include_errors && optionx::is_error_trade_state(rec.trade_state)) {
                return false;
            }
            if (!config.include_non_terminal && !optionx::is_terminal_trade_state(rec.trade_state)) {
                return false;
            }
            return true;
        }

        static void recount_series(
                const std::vector<optionx::TradeRecord>& records,
                const optionx::TradeStatsConfig& config,
                optionx::TradeSeriesStats& series) {
            std::uint64_t cur_win = 0, cur_loss = 0;
            std::uint64_t max_w = 0, max_l = 0;
            std::uint64_t count_w = 0, count_l = 0;
            std::uint64_t total_w_len = 0, total_l_len = 0;

            for (const auto& rec : records) {
                if (!include_outcome(rec, config)) continue;
                if (!optionx::is_terminal_trade_state(rec.trade_state)) continue;

                if (rec.trade_state == optionx::TradeState::WIN) {
                    if (cur_loss > 0) {
                        ++count_l;
                        max_l = std::max(max_l, cur_loss);
                        total_l_len += cur_loss;
                        cur_loss = 0;
                    }
                    ++cur_win;
                } else if (rec.trade_state == optionx::TradeState::LOSS) {
                    if (cur_win > 0) {
                        ++count_w;
                        max_w = std::max(max_w, cur_win);
                        total_w_len += cur_win;
                        cur_win = 0;
                    }
                    ++cur_loss;
                } else {
                    // Standoff / refund / error: close both series
                    if (cur_win > 0) {
                        ++count_w;
                        max_w = std::max(max_w, cur_win);
                        total_w_len += cur_win;
                        cur_win = 0;
                    }
                    if (cur_loss > 0) {
                        ++count_l;
                        max_l = std::max(max_l, cur_loss);
                        total_l_len += cur_loss;
                        cur_loss = 0;
                    }
                }
            }

            // Close trailing series
            if (cur_win > 0) {
                ++count_w;
                max_w = std::max(max_w, cur_win);
                total_w_len += cur_win;
            }
            if (cur_loss > 0) {
                ++count_l;
                max_l = std::max(max_l, cur_loss);
                total_l_len += cur_loss;
            }

            series.max_win_series = max_w;
            series.max_loss_series = max_l;
            series.total_win_series = count_w;
            series.total_loss_series = count_l;
            series.avg_win_series = (count_w > 0) ? (total_w_len / count_w) : 0;
            series.avg_loss_series = (count_l > 0) ? (total_l_len / count_l) : 0;
            series.current_series = (cur_win > 0) ? cur_win : cur_loss;
            series.current_is_win = (cur_win > 0);
        }
    };

} // namespace optionx::storage

#endif // _OPTIONX_TRADE_STATS_CALCULATOR_HPP_INCLUDED
