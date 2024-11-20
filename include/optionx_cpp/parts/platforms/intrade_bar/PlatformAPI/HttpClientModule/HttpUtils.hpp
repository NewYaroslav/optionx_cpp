#pragma once
#ifndef _OPTIONX_PLATFORMS_INTRADERBAR_HTTP_CLIENT_MODULE_HTTP_UTILS_HPP_INCLUDED
#define _OPTIONX_PLATFORMS_INTRADERBAR_HTTP_CLIENT_MODULE_HTTP_UTILS_HPP_INCLUDED

/// \file HttpUtils.hpp
/// \brief Contains utility functions for handling HTTP responses and errors.

#include <kurlyk.hpp>
#include <log-it/LogIt.hpp>
#include <string>

namespace optionx {
namespace platforms {
namespace intrade_bar {

    /// \brief Validates the HTTP response for an unexpected status code.
    /// \details Logs an error if the response is null or the status code is not 200.
    /// \param response The HTTP response object.
    /// \param log_message A message to log in case of an unexpected status code.
    /// \return True if the response status code is 200; otherwise, false.
    bool validate_status(const kurlyk::HttpResponsePtr& response, const std::string& log_message) {
        if (!response || response->status_code != 200) {
#           ifdef OPTIONX_LOG_UNIQUE_FILE_INDEX
            const int log_index = OPTIONX_LOG_UNIQUE_FILE_INDEX;
            LOGIT_STREAM_ERROR_TO(log_index) << (response ? response->content : "Empty response");
            LOGIT_PRINT_ERROR(
                log_message,
                (response ? response->status_code : -1),
                "; Content log was written to file: ",
                LOGIT_GET_LAST_FILE_NAME(log_index)
            );
#           else
            LOGIT_STREAM_INFO() << response->error_code;
            LOGIT_PRINT_ERROR(
                log_message,
                (response ? response->status_code : -1)
            );
#           endif
            return false;
        }
        return true;
    }

    /// \brief Validates the HTTP response for an unexpected status code.
    /// \details Logs an error if the response is null or the status code is not 200.
    /// \param response The HTTP response object.
    /// \return True if the response status code is 200; otherwise, false.
    bool validate_status(const kurlyk::HttpResponsePtr& response) {
        return validate_status(response, "Unexpected status code: ");
    }

    /// \brief Checks if the HTTP response content indicates DDoS protection.
    /// \details Logs an error if "DDoS-GUARD" is detected in the response content.
    /// \param response The HTTP response object.
    /// \return True if no DDoS protection is detected; otherwise, false.
    bool validate_ddos_protection(const kurlyk::HttpResponsePtr& response) {
        const std::string ddos_marker = "DDoS-GUARD";
        if (response && response->content.find(ddos_marker) != std::string::npos) {
#           ifdef OPTIONX_LOG_UNIQUE_FILE_INDEX
            const int log_index = OPTIONX_LOG_UNIQUE_FILE_INDEX;
            LOGIT_STREAM_ERROR_TO(log_index) << response->content;
            LOGIT_PRINT_ERROR(
                "DDoS protection detected. Content log was written to file: ",
                LOGIT_GET_LAST_FILE_NAME(log_index)
            );
#           else
            LOGIT_ERROR("DDoS protection detected.");
#           endif
            return false;
        }
        return true;
    }

    /// \brief Validates the HTTP response for status and DDoS protection.
    /// \param response The HTTP response to validate.
    /// \param callback The callback to invoke if the validation fails. The callback is called with a failure flag, a reason message, and cloned authorization data.
    /// \param auth_data The authorization data to pass to the callback upon failure.
    /// \return True if the response is valid; otherwise, false. If false, the callback is invoked.
    bool validate_response(
            const kurlyk::HttpResponsePtr& response,
            const modules::ConnectRequestEvent::callback_t& callback,
            const std::shared_ptr<AuthData>& auth_data) {
        if (!response) {
            callback(false, "No response received from the server.", auth_data->clone_unique());
            LOGIT_ERROR("No response received from the server.");
            return false;
        }
        if (!validate_status(response)) {
            callback(false, "Unexpected status code: " + std::to_string((response ? response->status_code : -1)), auth_data->clone_unique());
            return false;
        }
        if (!validate_ddos_protection(response)) {
            callback(false, "DDoS protection detected.", auth_data->clone_unique());
            return false;
        }
        return true;
    }

    /// \brief Validates the HTTP response for status and DDoS protection.
    /// \tparam Callback The type of callback to be invoked on failure.
    /// \param response The HTTP response to validate. Must not be nullptr.
    /// \param callback The callback to invoke if validation fails. It receives an error message as a parameter.
    /// \return True if the response is valid; otherwise, false. If false, the callback is invoked.
    template <typename Callback>
    bool validate_response(
        const kurlyk::HttpResponsePtr& response,
        Callback callback) {
        if (!response) {
            callback("No response received from the server.");
            LOGIT_ERROR("No response received from the server.");
            return false;
        }
        if (!validate_status(response)) {
            callback("Invalid status code received from the server.");
            return false;
        }
        if (!validate_ddos_protection(response)) {
            callback("DDoS protection detected.");
            return false;
        }
        return true;
    }

    /// \brief Validates the HTTP response for status and DDoS protection.
    /// \param response The HTTP response to validate.
    /// \return True if the response is valid; otherwise, false. If false, the callback is invoked.
    bool validate_response(
        const kurlyk::HttpResponsePtr& response) {
        if (!response) {
            LOGIT_ERROR("No response received from the server.");
            return false;
        }
        if (!validate_status(response)) {
            return false;
        }
        if (!validate_ddos_protection(response)) {
            return false;
        }
        return true;
    }

    std::string normalize_symbol_name(std::string symbol) {
        for (;;) {
            auto it_str = symbol.find('/');
            if(it_str != std::string::npos) symbol.erase(it_str, 1);
            else break;
        }
        if (symbol == "BTCUSD") return "BTCUSDT";
        return symbol;
    }

} // namespace intrade_bar
} // namespace platforms
} // namespace optionx

#endif // _OPTIONX_PLATFORMS_INTRADERBAR_HTTP_CLIENT_MODULE_HTTP_UTILS_HPP_INCLUDED
