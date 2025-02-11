#pragma once
#ifndef _OPTIONX_PLATFORMS_INTRADERBAR_TRADE_EXECUTION_MODULE_HPP_INCLUDED
#define _OPTIONX_PLATFORMS_INTRADERBAR_TRADE_EXECUTION_MODULE_HPP_INCLUDED

/// \file TradeExecutionModule.hpp
/// \brief Implements trade execution functionality for the Intrade Bar platform.

namespace optionx::platforms::intrade_bar {

    /// \class TradeExecutionModule
    /// \brief Handles trade execution operations for the Intrade Bar platform.
    ///
    /// This module is responsible for executing trade requests, interacting with
    /// the HTTP client, and managing trade execution workflows.
    class TradeExecutionModule final : public modules::BaseTradeExecutionModule {
    public:

        /// \brief Constructs the trade execution module.
        /// \param platform Reference to the trading platform.
        /// \param account_info Shared pointer to account information data.
        explicit TradeExecutionModule(
                BaseTradingPlatform& platform,
                std::shared_ptr<BaseAccountInfoData> account_info)
                : modules::BaseTradeExecutionModule(
                    platform.event_hub(),
                    std::move(account_info)) {
            platform.register_module(this);
        }

        /// \brief Default destructor.
        virtual ~TradeExecutionModule() = default;

        /// \brief Returns the platform type.
        /// \return Platform type identifier (`PlatformType::INTRADE_BAR`).
        PlatformType platform_type() override final {
            return PlatformType::INTRADE_BAR;
        }

    }; // class TradeExecutionModule

} // namespace optionx::platforms::intrade_bar

#endif // _OPTIONX_PLATFORMS_INTRADERBAR_TRADE_EXECUTION_MODULE_HPP_INCLUDED
