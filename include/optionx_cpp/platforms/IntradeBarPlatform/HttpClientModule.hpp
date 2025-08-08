#pragma once
#ifndef _OPTIONX_PLATFORMS_INTRADERBAR_HTTP_CLIENT_MODULE_HPP_INCLUDED
#define _OPTIONX_PLATFORMS_INTRADERBAR_HTTP_CLIENT_MODULE_HPP_INCLUDED

/// \file HttpClientModule.hpp
/// \brief Defines the HTTP client module for the Intrade Bar platform.

namespace optionx::platforms::intrade_bar {

    /// \enum RateLimitType
    /// \brief Defines the types of rate limits for HTTP requests.
    enum class RateLimitType : uint32_t {
        GENERAL,                ///< The general limit for all requests.
        TRADE_EXECUTION,        ///< The limit for trade opening requests.
        TRADE_RESULT,           ///< The limit for trade result requests.
        BALANCE,                ///< The limit for balance requests.
        ACCOUNT_INFO,           ///< The limit for account information requests.
        ACCOUNT_SETTINGS,       ///< The limit for account type or currency change requests.
        TICK_DATA,              ///< The limit for tick data requests.
        COUNT                   ///< The total number of rate limit types.
    };

    /// \class HttpClientModule
    /// \brief Handles HTTP requests and manages rate limits for the Intrade Bar platform.
    class HttpClientModule final : public modules::BaseHttpClientModule {
    public:

        /// \brief Constructs the HTTP client module and initializes rate limits.
        /// \param platform Reference to the trading platform.
        explicit HttpClientModule(
                BaseTradingPlatform& platform)
                : modules::BaseHttpClientModule(platform.event_bus()) {
            set_rate_limit_rpm(RateLimitType::GENERAL, 60);
            set_rate_limit_rps(RateLimitType::TRADE_EXECUTION, 1);
            set_rate_limit_rps(RateLimitType::TRADE_RESULT, 1);
            set_rate_limit_rps(RateLimitType::BALANCE, 1);
            set_rate_limit_rpm(RateLimitType::ACCOUNT_INFO, 6);
            set_rate_limit_rpm(RateLimitType::ACCOUNT_SETTINGS, 12);
            set_rate_limit_rps(RateLimitType::TICK_DATA, 1);
            platform.register_module(this);
        }

        /// \brief Default destructor.
        virtual ~HttpClientModule() = default;

    }; // class HttpClientModule

} // namespace optionx::platforms::intrade_bar

#endif // _OPTIONX_PLATFORMS_INTRADERBAR_HTTP_CLIENT_MODULE_HPP_INCLUDED
