#pragma once
#ifndef _OPTIONX_PLATFORMS_TRADEUP_PLATFORM_API_HPP_INCLUDED
#define _OPTIONX_PLATFORMS_TRADEUP_PLATFORM_API_HPP_INCLUDED

/// \file TradeUpPlatform.hpp
/// \brief Defines the TradeUpPlatform class implementing TradeUp broker API.

#include "platforms/common/BaseTradingPlatform.hpp"
#include "TradeUpPlatform/AuthData.hpp"
#include "TradeUpPlatform/AccountInfoData.hpp"
#include "TradeUpPlatform/http_utils.hpp"
#include "TradeUpPlatform/http_parsers.hpp"
#include "TradeUpPlatform/HttpClientModule.hpp"
#include "TradeUpPlatform/AuthManager.hpp"
#include "TradeUpPlatform/BalanceManager.hpp"

namespace optionx::platforms {

    /// \class TradeUpPlatform
    /// \brief Minimal platform implementation for TradeUp broker.
    class TradeUpPlatform final : public BaseTradingPlatform {
    public:
        TradeUpPlatform()
            : BaseTradingPlatform(std::make_shared<tradeup::AccountInfoData>()),
              m_http_client(*this),
              m_auth_manager(*this, m_http_client, m_account_info),
              m_balance_manager(*this, m_http_client, m_account_info) {
        }

        virtual ~TradeUpPlatform() = default;

        bool place_trade(std::unique_ptr<TradeRequest> trade_request) override {
            (void)trade_request;
            return false;
        }

        PlatformType platform_type() const override {
            return PlatformType::TRADEUP;
        }

    private:
        tradeup::HttpClientModule m_http_client;
        tradeup::AuthManager      m_auth_manager;
        tradeup::BalanceManager   m_balance_manager;
    };

} // namespace optionx::platforms

#endif // _OPTIONX_PLATFORMS_TRADEUP_PLATFORM_API_HPP_INCLUDED
