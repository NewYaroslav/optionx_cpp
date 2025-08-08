#pragma once
#ifndef _OPTIONX_PLATFORMS_TRADEUP_HTTP_UTILS_HPP_INCLUDED
#define _OPTIONX_PLATFORMS_TRADEUP_HTTP_UTILS_HPP_INCLUDED

/// \file http_utils.hpp
/// \brief Utility helpers for TradeUp HTTP responses.

#include <string>
#include <algorithm>
#include "optionx_cpp/data/account/ConnectionResult.hpp"
#include "optionx_cpp/utils/string_utils.hpp"

namespace optionx::platforms::tradeup {

    inline bool validate_status(const kurlyk::HttpResponsePtr& response) {
        return response && response->status_code == 200;
    }

    inline bool validate_response(const kurlyk::HttpResponsePtr& response) {
        return validate_status(response);
    }

    /// \brief Extracts a cookie value from HTTP headers.
    inline std::string extract_cookie(const kurlyk::Headers& headers, const std::string& name) {
        for (const auto& h : headers) {
            std::string key = h.first;
            std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c){ return std::tolower(c); });
            if (key == "set-cookie") {
                auto pos = h.second.find(name + "=");
                if (pos != std::string::npos) {
                    pos += name.size() + 1;
                    auto end = h.second.find(';', pos);
                    return h.second.substr(pos, end - pos);
                }
            }
        }
        return {};
    }

} // namespace optionx::platforms::tradeup

#endif // _OPTIONX_PLATFORMS_TRADEUP_HTTP_UTILS_HPP_INCLUDED
