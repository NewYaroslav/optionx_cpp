#pragma once
#ifndef OPTIONX_HEADER_PLATFORMS_COMMON_API_RESULT_HPP_INCLUDED
#define OPTIONX_HEADER_PLATFORMS_COMMON_API_RESULT_HPP_INCLUDED

/// \file ApiResult.hpp
/// \brief Small typed result wrapper for broker HTTP API calls.

namespace optionx::platforms {

    /// \brief Typed result for one broker API operation.
    /// \tparam T Payload type returned on success.
    template<class T>
    struct ApiResult {
        static constexpr long NO_HTTP_STATUS = 0;     ///< No HTTP status was captured for this result.
        static constexpr long NO_RESPONSE_STATUS = -1; ///< The HTTP request did not produce a response.

        bool success = false;                       ///< Whether the operation succeeded.
        long status_code = NO_HTTP_STATUS;          ///< HTTP status code, or a NO_* sentinel.
        T value{};                                  ///< Parsed response payload.
        std::string error_message;                  ///< Human-readable failure reason.

        /// \brief Creates a successful result.
        /// \param payload Parsed success payload.
        /// \param status HTTP status code, if available.
        /// \return Successful typed result.
        static ApiResult ok(T payload, long status = NO_HTTP_STATUS) {
            ApiResult result;
            result.success = true;
            result.status_code = status;
            result.value = std::move(payload);
            return result;
        }

        /// \brief Creates a failed result.
        /// \param message Human-readable failure reason.
        /// \param status HTTP status code, if available.
        /// \return Failed typed result.
        static ApiResult fail(std::string message, long status = NO_RESPONSE_STATUS) {
            ApiResult result;
            result.success = false;
            result.status_code = status;
            result.error_message = std::move(message);
            return result;
        }

        /// \brief Checks whether status_code contains a real HTTP status.
        /// \return True for positive HTTP status codes; false for NO_* sentinels.
        bool has_http_status() const noexcept {
            return status_code > NO_HTTP_STATUS;
        }

        /// \brief Allows concise success checks.
        explicit operator bool() const noexcept {
            return success;
        }
    };

} // namespace optionx::platforms

#endif // OPTIONX_HEADER_PLATFORMS_COMMON_API_RESULT_HPP_INCLUDED
