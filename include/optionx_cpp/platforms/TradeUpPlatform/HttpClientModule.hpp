#pragma once
#ifndef _OPTIONX_PLATFORMS_TRADEUP_HTTP_CLIENT_MODULE_HPP_INCLUDED
#define _OPTIONX_PLATFORMS_TRADEUP_HTTP_CLIENT_MODULE_HPP_INCLUDED

/// \file HttpClientModule.hpp
/// \brief Defines the HTTP client module for the TradeUp platform.

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
            get_http_client().assign_rate_limit_id(get_rate_limit(RateLimitType::GENERAL));
            platform.register_module(this);
        }
        
        virtual ~HttpClientModule() = default;
    };

} // namespace optionx::platforms::tradeup

#endif // _OPTIONX_PLATFORMS_TRADEUP_HTTP_CLIENT_MODULE_HPP_INCLUDED
