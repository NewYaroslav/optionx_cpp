#pragma once
#ifndef _OPTIONX_TRADE_HISTORY_RESULT_HPP_INCLUDED
#define _OPTIONX_TRADE_HISTORY_RESULT_HPP_INCLUDED

/// \file TradeHistoryResult.hpp
/// \brief Defines the result of a broker trade-history request.

#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "TradeResult.hpp"

namespace optionx {

    /// \struct TradeHistoryResult
    /// \brief Public result DTO for closed broker trade-history requests.
    struct TradeHistoryResult {
        static constexpr long NO_HTTP_STATUS = 0;      ///< No HTTP status was captured.
        static constexpr long NO_RESPONSE_STATUS = -1; ///< The request did not produce a response.

        bool success = false;              ///< Whether the history request succeeded.
        long status_code = NO_HTTP_STATUS; ///< HTTP status code, when available.
        std::string error_desc;            ///< Human-readable failure reason.
        std::vector<TradeResult> trades;   ///< Closed trades returned by the broker.

        /// \brief Creates a successful history result.
        /// \param history_trades Closed trades returned by the broker.
        /// \param status HTTP status code, if available.
        /// \return Successful result.
        static TradeHistoryResult ok(
                std::vector<TradeResult> history_trades,
                long status = NO_HTTP_STATUS) {
            TradeHistoryResult result;
            result.success = true;
            result.status_code = status;
            result.trades = std::move(history_trades);
            return result;
        }

        /// \brief Creates a failed history result.
        /// \param message Human-readable failure reason.
        /// \param status HTTP status code, if available.
        /// \return Failed result.
        static TradeHistoryResult fail(
                std::string message,
                long status = NO_RESPONSE_STATUS) {
            TradeHistoryResult result;
            result.success = false;
            result.status_code = status;
            result.error_desc = std::move(message);
            return result;
        }

        /// \brief Checks whether status_code contains a real HTTP status.
        /// \return True for positive HTTP status codes.
        bool has_http_status() const noexcept {
            return status_code > NO_HTTP_STATUS;
        }

        /// \brief Allows concise success checks.
        explicit operator bool() const noexcept {
            return success;
        }

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(
            TradeHistoryResult,
            success,
            status_code,
            error_desc,
            trades
        )
    };

} // namespace optionx

#endif // _OPTIONX_TRADE_HISTORY_RESULT_HPP_INCLUDED
