#pragma once
#ifndef OPTIONX_HEADER_PLATFORMS_TRADE_UP_PLATFORM_HTTP_UTILS_HPP_INCLUDED
#define OPTIONX_HEADER_PLATFORMS_TRADE_UP_PLATFORM_HTTP_UTILS_HPP_INCLUDED

/// \file http_utils.hpp
/// \brief Utility helpers for TradeUp HTTP responses.

#include <string>
#include <algorithm>

namespace optionx::platforms::tradeup {

    using ::optionx::utils::validate_status;
    using ::optionx::utils::validate_ddos_protection;
    using ::optionx::utils::validate_response;

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
    
    inline void collect_set_cookie(const Headers& headers, ::kurlyk::Cookies& out) {
        auto [it, end] = headers.equal_range("set-cookie");
        for (; it != end; ++it) {
            ::kurlyk::Cookies c = ::kurlyk::utils::parse_cookie(it->second);
            out.insert(c.begin(), c.end());
        }
    }

} // namespace optionx::platforms::tradeup

#endif // OPTIONX_HEADER_PLATFORMS_TRADE_UP_PLATFORM_HTTP_UTILS_HPP_INCLUDED
