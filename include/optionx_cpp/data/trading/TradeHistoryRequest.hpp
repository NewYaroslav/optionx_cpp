#pragma once
#ifndef _OPTIONX_TRADE_HISTORY_REQUEST_HPP_INCLUDED
#define _OPTIONX_TRADE_HISTORY_REQUEST_HPP_INCLUDED

/// \file TradeHistoryRequest.hpp
/// \brief Defines account trade-history lookup parameters.

#include <cstdint>

#include <nlohmann/json.hpp>

#include "enums.hpp"

namespace optionx {

    /// \class TradeHistoryRequest
    /// \brief Requests closed trade history for a broker account and time range.
    class TradeHistoryRequest {
    public:
        int64_t start_time_ms = 0; ///< Inclusive range start in Unix milliseconds.
        int64_t end_time_ms = 0;   ///< Inclusive range end in Unix milliseconds.
        AccountType account_type = AccountType::UNKNOWN; ///< Account type to query, if supported.

        /// \brief Checks whether the request contains a valid time range.
        /// \return True when both timestamps are positive and ordered.
        bool has_valid_range() const noexcept {
            return start_time_ms > 0 && end_time_ms > 0 && start_time_ms <= end_time_ms;
        }

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(
            TradeHistoryRequest,
            start_time_ms,
            end_time_ms,
            account_type
        )
    };

} // namespace optionx

#endif // _OPTIONX_TRADE_HISTORY_REQUEST_HPP_INCLUDED
