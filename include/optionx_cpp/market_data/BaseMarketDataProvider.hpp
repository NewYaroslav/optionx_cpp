#pragma once
#ifndef _OPTIONX_MARKET_DATA_BASE_MARKET_DATA_PROVIDER_HPP_INCLUDED
#define _OPTIONX_MARKET_DATA_BASE_MARKET_DATA_PROVIDER_HPP_INCLUDED

/// \file BaseMarketDataProvider.hpp
/// \brief Declares the public market-data provider role.

namespace optionx::market_data {

    /// \class BaseMarketDataProvider
    /// \brief Role interface for endpoints that provide live or historical market data.
    class BaseMarketDataProvider {
    public:
        using bars_callback_t = std::function<void(const std::vector<SingleBar>&)>;
        using ticks_callback_t = std::function<void(const std::vector<SingleTick>&)>;
        using bar_history_callback_t = std::function<void(BarHistoryResult)>;

        /// \brief Virtual destructor for polymorphic provider implementations.
        virtual ~BaseMarketDataProvider() = default;

        /// \brief Returns a reference to the live bar-data callback.
        /// \return Mutable callback reference, or a null callback if live bars are unsupported.
        virtual bars_callback_t& on_bar_data() {
            static bars_callback_t null_callback;
            return null_callback;
        }

        /// \brief Returns a reference to the live tick-data callback.
        /// \return Mutable callback reference, or a null callback if live ticks are unsupported.
        virtual ticks_callback_t& on_tick_data() {
            static ticks_callback_t null_callback;
            return null_callback;
        }

        /// \brief Requests historical bar data for a specified time range.
        /// \param request Historical bar-data request parameters.
        /// \param callback Callback function to receive bars or a failure reason.
        /// \return True if the request was accepted for processing; false otherwise.
        virtual bool fetch_bar_history(
                const BarHistoryRequest& request,
                bar_history_callback_t callback) {
            (void)request;
            (void)callback;
            return false;
        }
    };

} // namespace optionx::market_data

#endif // _OPTIONX_MARKET_DATA_BASE_MARKET_DATA_PROVIDER_HPP_INCLUDED
