#pragma once
#ifndef _OPTIONX_PLATFORMS_TRADEUP_HTTP_CLIENT_MODULE_HPP_INCLUDED
#define _OPTIONX_PLATFORMS_TRADEUP_HTTP_CLIENT_MODULE_HPP_INCLUDED

/// \file HttpClientModule.hpp
/// \brief Defines the HTTP client module for the TradeUp platform.

#include "optionx_cpp/modules/BaseHttpClientModule.hpp"

namespace optionx::platforms::tradeup {

    enum class RateLimitType : uint32_t {
        GENERAL,
        AUTH,
        BALANCE,
        COUNT
    };

    /// \class HttpClientModule
    class HttpClientModule final : public modules::BaseHttpClientModule {
    public:
        explicit HttpClientModule(BaseTradingPlatform& platform)
            : modules::BaseHttpClientModule(platform.event_bus()) {
            set_rate_limit_rpm(RateLimitType::GENERAL, 60);
            set_rate_limit_rps(RateLimitType::AUTH, 1);
            set_rate_limit_rps(RateLimitType::BALANCE, 1);
            platform.register_module(this);
        }

        void set_auth_token(std::string token) { m_token = std::move(token); }
        const std::string& auth_token() const { return m_token; }

        void set_session_cookie(std::string cookie) { m_cookie = std::move(cookie); }
        const std::string& session_cookie() const { return m_cookie; }

    private:
        std::string m_token;
        std::string m_cookie;
    };

} // namespace optionx::platforms::tradeup

#endif // _OPTIONX_PLATFORMS_TRADEUP_HTTP_CLIENT_MODULE_HPP_INCLUDED
