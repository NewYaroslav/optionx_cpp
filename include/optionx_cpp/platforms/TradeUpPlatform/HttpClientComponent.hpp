#pragma once
#ifndef _OPTIONX_PLATFORMS_TRADEUP_HTTP_CLIENT_COMPONENT_HPP_INCLUDED
#define _OPTIONX_PLATFORMS_TRADEUP_HTTP_CLIENT_COMPONENT_HPP_INCLUDED

/// \file HttpClientComponent.hpp
/// \brief Defines the HTTP client component for the TradeUp platform.

namespace optionx::platforms::tradeup {

    enum class RateLimitType : uint32_t {
        GENERAL,
        AUTH,
        BALANCE,
        COUNT
    };

    /// \class HttpClientComponent
    class HttpClientComponent final : public components::BaseHttpClientComponent {
    public:
        explicit HttpClientComponent(BaseTradingPlatform& platform)
            : components::BaseHttpClientComponent(platform.event_bus()) {
            set_rate_limit_rpm(RateLimitType::GENERAL, 60);
            set_rate_limit_rps(RateLimitType::AUTH, 1);
            set_rate_limit_rps(RateLimitType::BALANCE, 1);
            get_http_client().assign_rate_limit_id(get_rate_limit(RateLimitType::GENERAL));
            platform.register_component(this);
        }
        
        virtual ~HttpClientComponent() = default;
    };

} // namespace optionx::platforms::tradeup

#endif // _OPTIONX_PLATFORMS_TRADEUP_HTTP_CLIENT_COMPONENT_HPP_INCLUDED
