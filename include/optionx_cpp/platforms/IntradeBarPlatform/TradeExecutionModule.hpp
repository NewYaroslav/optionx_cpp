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

    private:

        /// \brief Preprocesses a trade request before placing it into the queue.
        /// \param trade_request Unique pointer to a trade request.
        /// \param trade_result Unique pointer to a trade result.
        /// \return True if the request is valid after preprocessing; false otherwise.
        bool preprocess_trade_request(
                std::unique_ptr<TradeRequest> &trade_request,
                std::unique_ptr<TradeResult> &trade_result) override final {
            if (trade_request &&
                trade_request->option_type == OptionType::CLASSIC) {
                if (trade_request->expiry_time > 0) {
                    trade_request->duration = calc_expiration(trade_result->place_date, trade_request->expiry_time);
                } else
                if (trade_request->expiry_time == 0 &&
                    trade_request->duration > 0) {
                    trade_request->expiry_time = calc_expiry_time(trade_result->place_date, trade_request->duration / time_shield::SEC_PER_MIN);
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

        /// \brief Gets the closing timestamp for a classic binary option.
        /// \param timestamp The initial timestamp in seconds.
        /// \param expiration Expiration time in minutes.
        /// \return Closing timestamp, or 0 if invalid.
        int64_t calc_expiry_time(int64_t timestamp, int64_t expiration) const {
            if ((expiration % 5) != 0 || expiration < 5) return 0;
            const int64_t timestamp_future = timestamp + (expiration + 3) * time_shield::SEC_PER_MIN;
            return (timestamp_future - timestamp_future % (5 * time_shield::SEC_PER_MIN));
        }
    }; // class TradeExecutionModule

} // namespace optionx::platforms::intrade_bar

#endif // _OPTIONX_PLATFORMS_INTRADERBAR_TRADE_EXECUTION_MODULE_HPP_INCLUDED
