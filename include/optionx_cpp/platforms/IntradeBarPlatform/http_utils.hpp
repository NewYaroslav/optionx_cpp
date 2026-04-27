#pragma once
#ifndef _OPTIONX_PLATFORMS_INTRADERBAR_HTTP_CLIENT_MODULE_HTTP_UTILS_HPP_INCLUDED
#define _OPTIONX_PLATFORMS_INTRADERBAR_HTTP_CLIENT_MODULE_HTTP_UTILS_HPP_INCLUDED

/// \file http_utils.hpp
/// \brief Contains utility functions for handling HTTP responses and errors.

namespace optionx::platforms::intrade_bar {

    /// \brief Validates the HTTP response for status and DDoS protection.
    /// \param response The HTTP response to validate.
    /// \param callback The callback to invoke if the validation fails. The callback is called with a failure flag, a reason message, and cloned authorization data.
    /// \param auth_data The authorization data to pass to the callback upon failure.
    /// \return True if the response is valid; otherwise, false. If false, the callback is invoked.
    bool validate_response(
            const kurlyk::HttpResponsePtr& response,
            const connection_callback_t& callback,
            const std::shared_ptr<AuthData>& auth_data) {
        if (!response) {
            LOGIT_ERROR("No response received from the server.");
            callback({false, "No response received from the server.", auth_data->clone_unique()});
            return false;
        }
        if (!::optionx::utils::validate_status(response)) {
            callback({
                false,
                ::optionx::utils::describe_response_error(response, "Invalid HTTP response."),
                auth_data->clone_unique()
            });
            return false;
        }
        if (!::optionx::utils::validate_ddos_protection(response)) {
            callback({false, "DDoS protection detected.", auth_data->clone_unique()});
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

    using ::optionx::utils::validate_status;
    using ::optionx::utils::validate_ddos_protection;
    using ::optionx::utils::validate_response;
}

#endif // _OPTIONX_PLATFORMS_INTRADERBAR_HTTP_CLIENT_MODULE_HTTP_UTILS_HPP_INCLUDED
