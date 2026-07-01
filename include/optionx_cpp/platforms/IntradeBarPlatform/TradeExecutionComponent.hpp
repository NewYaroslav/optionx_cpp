#pragma once
#ifndef _OPTIONX_PLATFORMS_INTRADERBAR_TRADE_EXECUTION_COMPONENT_HPP_INCLUDED
#define _OPTIONX_PLATFORMS_INTRADERBAR_TRADE_EXECUTION_COMPONENT_HPP_INCLUDED

/// \file TradeExecutionComponent.hpp
/// \brief Implements trade execution functionality for the Intrade Bar platform.

#include <limits>

#include "SymbolUtils.hpp"

namespace optionx::platforms::intrade_bar {

    /// \class TradeExecutionComponent
    /// \brief Handles trade execution operations for the Intrade Bar platform.
    ///
    /// This component is responsible for executing trade requests, interacting with
    /// the HTTP client, and managing trade execution workflows.
    class TradeExecutionComponent final : public components::BaseTradeExecutionComponent {
    public:

        /// \brief Constructs the trade execution component.
        /// \param platform Reference to the trading platform.
        /// \param account_info Shared pointer to account information data.
        explicit TradeExecutionComponent(
                BaseTradingPlatform& platform,
                std::shared_ptr<BaseAccountInfoData> account_info)
                : components::BaseTradeExecutionComponent(
                    platform.event_bus(),
                    std::move(account_info)) {
            platform.register_component(this);
        }

        /// \brief Default destructor.
        virtual ~TradeExecutionComponent() = default;

        /// \brief Returns the platform type.
        /// \return Platform type identifier (`PlatformType::INTRADE_BAR`).
        PlatformType platform_type() const override final {
            return PlatformType::INTRADE_BAR;
        }

    private:

        /// \brief Preprocesses a trade request before placing it into the queue.
        /// \param trade_request Unique pointer to a trade request.
        /// \param trade_result Unique pointer to a trade result.
        /// \return True if the request is valid after preprocessing; false otherwise.
        bool preprocess_trade_request(
                std::unique_ptr<TradeRequest> &trade_request,
                std::unique_ptr<TradeResult> &trade_result) override final {
            if (trade_request) {
                trade_request->symbol = normalize_symbol_name(std::move(trade_request->symbol));
            }
            if (trade_request &&
                trade_request->option_type == OptionType::CLASSIC) {
                if (trade_request->expiry_time > 0) {
                    const auto duration = calc_expiration(
                        time_shield::ms_to_sec(trade_result->place_date),
                        trade_request->expiry_time);
                    trade_request->duration =
                        duration > 0 &&
                        duration <= static_cast<std::int64_t>((std::numeric_limits<std::uint32_t>::max)())
                        ? static_cast<std::uint32_t>(duration)
                        : 0;
                } else
                if (trade_request->expiry_time == 0 &&
                    trade_request->duration > 0) {
                    trade_request->expiry_time = calc_expiry_time(
                        time_shield::ms_to_sec(trade_result->place_date),
                        trade_request->duration / time_shield::SEC_PER_MIN);
                }
            }
            return true;
        }

        /// \brief Calculates the expiration time for a classic binary option.
        /// \param timestamp The current timestamp in seconds.
        /// \param expiry_time The intended closing timestamp.
        /// \return Expiration time in seconds, or 0 if invalid.
        int64_t calc_expiration(int64_t timestamp, int64_t expiry_time) const {
            if ((expiry_time % time_shield::SEC_PER_5_MIN) != 0) return 0;
            const int64_t diff = expiry_time - timestamp;
            if (diff <= time_shield::SEC_PER_3_MIN) return 0;
            return ((((diff - 1) / time_shield::SEC_PER_MIN - 3) / 5) * 5 + 5) * time_shield::SEC_PER_MIN;
        }

        /// \brief Calculates the closing timestamp for a classic binary option.
        /// \param timestamp The initial timestamp in seconds.
        /// \param expiration Expiration time in minutes.
        /// \return Closing timestamp in seconds, or 0 if the expiration is invalid.
        int64_t calc_expiry_time(int64_t timestamp, int64_t expiration) const {
            if ((expiration % 5) != 0 || expiration < 5) return 0;
            const int64_t timestamp_future = timestamp + (expiration + 3) * time_shield::SEC_PER_MIN;
            return (timestamp_future - timestamp_future % (5 * time_shield::SEC_PER_MIN));
        }
    }; // class TradeExecutionComponent

} // namespace optionx::platforms::intrade_bar

#endif // _OPTIONX_PLATFORMS_INTRADERBAR_TRADE_EXECUTION_COMPONENT_HPP_INCLUDED
