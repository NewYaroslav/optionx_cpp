#pragma once
#ifndef _OPTIONX_TRADE_RECORD_FILTER_HPP_INCLUDED
#define _OPTIONX_TRADE_RECORD_FILTER_HPP_INCLUDED

/// \file TradeRecordFilter.hpp
/// \brief Defines TradeRecordFilter and IncludeExcludeFilter for querying trade records.

#include <vector>
#include <algorithm>
#include <cstdint>
#include <string>

#include "optionx_cpp/data/trading/enums.hpp"

namespace optionx {

    /// \class IncludeExcludeFilter
    /// \brief Generic include/exclude filter for matching values.
    template<class T>
    class IncludeExcludeFilter {
    public:
        std::vector<T> include; ///< Values that must be present (empty = allow all).
        std::vector<T> exclude; ///< Values that must not be present.

        /// \brief Returns true if the value passes the filter.
        bool match(const T& value) const {
            if (!include.empty()) {
                if (std::find(include.begin(), include.end(), value) == include.end()) {
                    return false;
                }
            }
            if (!exclude.empty()) {
                if (std::find(exclude.begin(), exclude.end(), value) != exclude.end()) {
                    return false;
                }
            }
            return true;
        }

        /// \brief Adds a value to the include list.
        void add_include(const T& value) { include.push_back(value); }

        /// \brief Adds a value to the exclude list.
        void add_exclude(const T& value) { exclude.push_back(value); }

        /// \brief Clears the include list.
        void clear_include() { include.clear(); }

        /// \brief Clears the exclude list.
        void clear_exclude() { exclude.clear(); }

        /// \brief Clears both lists.
        void clear() { include.clear(); exclude.clear(); }

        /// \brief Returns true if both lists are empty.
        bool empty() const { return include.empty() && exclude.empty(); }
    };

    /// \class TradeRecordFilter
    /// \brief Comprehensive filter for TradeRecord queries and statistics.
    class TradeRecordFilter {
    public:
        IncludeExcludeFilter<PlatformType> platforms;
        IncludeExcludeFilter<AccountType> accounts;
        IncludeExcludeFilter<CurrencyType> currencies;
        IncludeExcludeFilter<OptionType> option_types;
        IncludeExcludeFilter<OrderType> order_types;
        IncludeExcludeFilter<TradeState> trade_states;
        IncludeExcludeFilter<TradeErrorCode> error_codes;

        IncludeExcludeFilter<std::string> symbols;
        IncludeExcludeFilter<std::string> signals;
        IncludeExcludeFilter<std::string> user_data;
        IncludeExcludeFilter<std::string> mm_group_names;

        IncludeExcludeFilter<std::int64_t> account_ids;
        IncludeExcludeFilter<std::int64_t> option_ids;
        IncludeExcludeFilter<std::uint64_t> trade_ids;
        IncludeExcludeFilter<std::int64_t> request_unique_ids;

        IncludeExcludeFilter<std::int64_t> durations;
        IncludeExcludeFilter<std::int32_t> mm_steps;

        IncludeExcludeFilter<std::uint32_t> hours;
        IncludeExcludeFilter<std::uint32_t> weekdays;
        IncludeExcludeFilter<std::uint32_t> month_days;
        IncludeExcludeFilter<std::uint32_t> months;

        std::int64_t start_second_of_day = 0;   ///< Range start in seconds from midnight (0..86399).
        std::int64_t stop_second_of_day = 0;    ///< Range end in seconds from midnight (0..86399).
        bool use_second_of_day = false;         ///< Enable time-of-day filtering.

        double min_amount = 0.0;
        double max_amount = 0.0;
        double min_payout = 0.0;
        double max_payout = 0.0;
        double min_profit = 0.0;
        double max_profit = 0.0;
        double min_balance = 0.0;
        double max_balance = 0.0;

        std::int64_t min_ping = 0;
        std::int64_t max_ping = 0;
        std::int64_t min_delay = 0;
        std::int64_t max_delay = 0;

        bool only_terminal = false;      ///< Match only terminal trade states.
        bool only_non_terminal = false;  ///< Match only non-terminal trade states.

        bool only_first_mm_step = false; ///< Match only records with mm_step == 0.
    };

    /// \brief Matches a second-of-day value against a range that may cross midnight.
    /// \param second Second of day (0..86399).
    /// \param start Range start (0..86399).
    /// \param stop Range end (0..86399).
    /// \return True if the second falls within the range.
    inline bool match_second_of_day(
            std::int64_t second,
            std::int64_t start,
            std::int64_t stop) noexcept {
        if (start == stop) {
            return true;
        }
        if (start < stop) {
            return second >= start && second <= stop;
        }
        // Range crosses midnight: e.g. 22:00..02:00
        return second >= start || second <= stop;
    }

} // namespace optionx

#endif // _OPTIONX_TRADE_RECORD_FILTER_HPP_INCLUDED
