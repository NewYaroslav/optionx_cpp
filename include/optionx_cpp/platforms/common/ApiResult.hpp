#pragma once
#ifndef _OPTIONX_PLATFORMS_COMMON_API_RESULT_HPP_INCLUDED
#define _OPTIONX_PLATFORMS_COMMON_API_RESULT_HPP_INCLUDED

/// \file ApiResult.hpp
/// \brief Small typed result wrapper for broker HTTP API calls.

#include <string>
#include <utility>

namespace optionx::platforms {

    /// \brief Typed result for one broker API operation.
    /// \tparam T Payload type returned on success.
    template<class T>
    struct ApiResult {
        bool success = false;           ///< Whether the operation succeeded.
        long status_code = 0;           ///< HTTP status code, if available.
        T value{};                      ///< Parsed response payload.
        std::string error_message;      ///< Human-readable failure reason.

        /// \brief Creates a successful result.
        static ApiResult ok(T payload, long status = 0) {
            ApiResult result;
            result.success = true;
            result.status_code = status;
            result.value = std::move(payload);
            return result;
        }

        /// \brief Creates a failed result.
        static ApiResult fail(std::string message, long status = -1) {
            ApiResult result;
            result.success = false;
            result.status_code = status;
            result.error_message = std::move(message);
            return result;
        }

        /// \brief Allows concise success checks.
        explicit operator bool() const noexcept {
            return success;
        }
    };

} // namespace optionx::platforms

#endif // _OPTIONX_PLATFORMS_COMMON_API_RESULT_HPP_INCLUDED
