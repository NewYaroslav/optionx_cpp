#pragma once
#ifndef _OPTIONX_BAR_HISTORY_RESULT_HPP_INCLUDED
#define _OPTIONX_BAR_HISTORY_RESULT_HPP_INCLUDED

/// \file BarHistoryResult.hpp
/// \brief Defines the result of a broker bar-history request.

namespace optionx {

    /// \struct BarHistoryResult
    /// \brief Public result DTO for broker historical bar requests.
    struct BarHistoryResult {
        static constexpr long NO_HTTP_STATUS = 0;      ///< No HTTP status was captured.
        static constexpr long NO_RESPONSE_STATUS = -1; ///< The request did not produce a response.

        bool success = false;              ///< Whether the history request succeeded.
        long status_code = NO_HTTP_STATUS; ///< HTTP status code, when available.
        std::string error_desc;            ///< Human-readable failure reason.
        BarSequence sequence;              ///< Historical bar sequence returned by the broker.

        /// \brief Creates a successful bar-history result.
        /// \param bar_sequence Historical bars returned by the broker.
        /// \param status HTTP status code, if available.
        /// \return Successful result.
        static BarHistoryResult ok(
                BarSequence bar_sequence,
                long status = NO_HTTP_STATUS) {
            BarHistoryResult result;
            result.success = true;
            result.status_code = status;
            result.sequence = std::move(bar_sequence);
            return result;
        }

        /// \brief Creates a failed bar-history result.
        /// \param message Human-readable failure reason.
        /// \param status HTTP status code, if available.
        /// \return Failed result.
        static BarHistoryResult fail(
                std::string message,
                long status = NO_RESPONSE_STATUS) {
            BarHistoryResult result;
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
    };

} // namespace optionx

#endif // _OPTIONX_BAR_HISTORY_RESULT_HPP_INCLUDED
