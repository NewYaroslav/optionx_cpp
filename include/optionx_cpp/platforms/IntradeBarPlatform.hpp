#pragma once
#ifndef _OPTIONX_PLATFORMS_INTRADERBAR_PLATFORM_API_HPP_INCLUDED
#define _OPTIONX_PLATFORMS_INTRADERBAR_PLATFORM_API_HPP_INCLUDED

/// \file IntradeBarPlatform.hpp
/// \brief Defines the IntradeBarPlatform class, which provides an implementation of the trading platform API.

#include "BaseTradingPlatform.hpp"
#include "IntradeBarPlatform/AuthData.hpp"
#include "IntradeBarPlatform/AccountInfoData.hpp"
#include "IntradeBarPlatform/http_utils.hpp"
#include "IntradeBarPlatform/http_parsers.hpp"
#include "IntradeBarPlatform/HttpClientModule.hpp"
#include "IntradeBarPlatform/RequestManager.hpp"
#include "IntradeBarPlatform/AuthManager.hpp"
#include "IntradeBarPlatform/TradeExecutionModule.hpp"
#include "IntradeBarPlatform/BalanceManager.hpp"
#include "IntradeBarPlatform/PriceManager.hpp"
#include "IntradeBarPlatform/BtcPriceManager.hpp"
#include "IntradeBarPlatform/TradeManager.hpp"

namespace optionx::platforms {

    /// \class IntradeBarPlatform
    /// \brief Implements the trading platform API for Intrade Bar.
    ///
    /// This class is responsible for handling all trading operations, including authentication,
    /// balance management, price retrieval, trade execution, and account information updates.
    /// It integrates various modules that manage these aspects of trading.
    class IntradeBarPlatform final : public BaseTradingPlatform {
    public:

        /// \brief Constructs the Intrade Bar trading platform.
        ///
        /// Initializes all required modules, including HTTP communication, authentication,
        /// balance tracking, trade execution, and price updates.
        IntradeBarPlatform()
            : BaseTradingPlatform(std::make_shared<intrade_bar::AccountInfoData>()),
              m_http_client(*this),
              m_request_manager(*this, m_http_client),
              m_trade_execution(*this, m_account_info),
              m_auth_manager(*this, m_request_manager, m_account_info),
              m_balance_manager(*this, m_request_manager, m_account_info),
              m_price_manager(*this, m_request_manager),
              m_btc_price_manager(*this),
              m_trade_manager(*this, m_request_manager, m_account_info) {
        }

        /// \brief Default destructor.
        virtual ~IntradeBarPlatform() = default;

        /// \brief Places a trade on the platform.
        ///
        /// This method forwards the trade request to the trade execution module.
        /// \param trade_request The trade request details.
        /// \return True if the trade was placed successfully; false otherwise.
        bool place_trade(std::unique_ptr<TradeRequest> trade_request) override {
            return m_trade_execution.place_trade(std::move(trade_request));
        }

    private:
        intrade_bar::HttpClientModule     m_http_client;      ///< Handles HTTP communication.
        intrade_bar::RequestManager       m_request_manager;  ///< Manages API requests.
        intrade_bar::TradeExecutionModule m_trade_execution;  ///< Manages trade execution.
        intrade_bar::AuthManager          m_auth_manager;     ///< Handles authentication processes.
        intrade_bar::BalanceManager       m_balance_manager;  ///< Tracks account balance.
        intrade_bar::PriceManager         m_price_manager;    ///< Retrieves and updates price data.
        intrade_bar::BtcPriceManager      m_btc_price_manager;///<
        intrade_bar::TradeManager         m_trade_manager;    ///< Manages trades and status updates.
    }; // IntradeBarPlatform

} // namespace optionx::platforms

#endif // _OPTIONX_PLATFORMS_INTRADERBAR_PLATFORM_API_HPP_INCLUDED
