#pragma once
#ifndef _OPTIONX_PLATFORMS_INTRADERBAR_HTTP_CLIENT_COMPONENT_HTTP_UTILS_HPP_INCLUDED
#define _OPTIONX_PLATFORMS_INTRADERBAR_HTTP_CLIENT_COMPONENT_HTTP_UTILS_HPP_INCLUDED

/// \file http_utils.hpp
/// \brief Contains utility functions for handling HTTP responses and errors.

#include "symbol_utils.hpp"

namespace optionx::platforms::intrade_bar {

    /// \brief Validates the HTTP response for status and DDoS protection.
    /// \param response The HTTP response to validate.
    /// \param callback The callback to invoke if the validation fails. The callback is called with a failure flag, a reason message, and cloned authorization data.
    /// \param auth_data The authorization data to pass to the callback upon failure.
    /// \return True if the response is valid; otherwise, false. If false, the callback is invoked.
    inline bool validate_response(
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

    /// \brief Returns the broker price precision used for a normalized symbol.
    /// \param symbol Broker or public symbol name.
    /// \return Number of price decimal places.
    inline std::uint8_t price_digits_for_symbol(const std::string& symbol) {
        const std::string normalized = normalize_symbol_name(symbol);
        if (normalized == "BTCUSDT") return 2;
        if (normalized.size() >= 6 && normalized.substr(3, 3) == "JPY") {
            return 3;
        }
        return 5;
    }

    /// \brief Converts an HTTP(S) or WS(S) host setting to a websocket host.
    /// \param host Broker host value from endpoint configuration.
    /// \return Host with `ws://` or `wss://` scheme.
    inline std::string make_websocket_host(const std::string& host) {
        const std::string wss_prefix = "wss://";
        const std::string ws_prefix = "ws://";
        const std::string https_prefix = "https://";
        const std::string http_prefix = "http://";

        if (host.rfind(wss_prefix, 0) == 0 || host.rfind(ws_prefix, 0) == 0) {
            return host;
        }
        if (host.rfind(https_prefix, 0) == 0) {
            return wss_prefix + host.substr(https_prefix.size());
        }
        if (host.rfind(http_prefix, 0) == 0) {
            return ws_prefix + host.substr(http_prefix.size());
        }
        return wss_prefix + host;
    }

    /// \brief Marks a SpreadPack as a known zero-spread Intrade Bar value.
    /// \param spread Spread pack to fill.
    /// \param symbol Symbol whose price precision should be stored.
    inline void set_zero_spread_for_symbol(SpreadPack& spread, const std::string& symbol) {
        const auto digits = price_digits_for_symbol(symbol);
        spread.set_spreads(0.0, 0.0, digits);
    }

    using ::optionx::utils::validate_status;
    using ::optionx::utils::validate_ddos_protection;
    using ::optionx::utils::validate_response;
}

#endif // _OPTIONX_PLATFORMS_INTRADERBAR_HTTP_CLIENT_COMPONENT_HTTP_UTILS_HPP_INCLUDED
