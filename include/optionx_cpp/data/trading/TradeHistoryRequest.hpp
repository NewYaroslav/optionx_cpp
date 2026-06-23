#pragma once
#ifndef _OPTIONX_TRADE_HISTORY_REQUEST_HPP_INCLUDED
#define _OPTIONX_TRADE_HISTORY_REQUEST_HPP_INCLUDED

/// \file TradeHistoryRequest.hpp
/// \brief Defines account trade-history lookup parameters.

#include <cstdint>
#include <string>

#include <nlohmann/json.hpp>

#include "TradeRecordTimeRange.hpp"

namespace optionx {

    /// \class TradeHistoryRequest
    /// \brief Requests closed trade history for the currently selected broker account.
    class TradeHistoryRequest {
    public:
        int64_t start_ms = 0; ///< Range start in Unix milliseconds.
        int64_t stop_ms = 0;  ///< Range end in Unix milliseconds.
        TradeRecordTimeField time_field = TradeRecordTimeField::CLOSE_DATE; ///< Timestamp field used for range filtering.
        TimeRangeMode range_mode = TimeRangeMode::CLOSED; ///< Range inclusion mode; NONE means no time filtering.
        std::string comment; ///< Optional comment copied to exported TradeRecord entries.

        /// \brief Creates a request that loads all available closed trade history.
        /// \return Trade history request with range filtering disabled.
        static TradeHistoryRequest all() noexcept {
            TradeHistoryRequest request;
            request.range_mode = TimeRangeMode::NONE;
            return request;
        }

        /// \brief Checks whether the request contains a valid time range.
        /// \return True when range filtering is disabled or timestamps are positive and ordered.
        bool has_valid_range() const noexcept {
            if (range_mode == TimeRangeMode::NONE) return true;
            return start_ms > 0 && stop_ms > 0 && start_ms <= stop_ms;
        }

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(
            TradeHistoryRequest,
            start_ms,
            stop_ms,
            time_field,
            range_mode,
            comment
        )
    };

} // namespace optionx

#endif // _OPTIONX_TRADE_HISTORY_REQUEST_HPP_INCLUDED
