#pragma once
#ifndef _OPTIONX_PLATFORMS_INTRADERBAR_PLATFORM_API_HPP_INCLUDED
#define _OPTIONX_PLATFORMS_INTRADERBAR_PLATFORM_API_HPP_INCLUDED

/// \file IPlatformAPI.hpp
/// \brief Defines the IPlatformAPI interface for interacting with various trading platforms and APIs.

#include "../../interfaces/IPlatformAPI.hpp"

namespace optionx {
namespace platforms {
namespace intrade_bar {

    class PlatformAPI final : public IPlatformAPI {
    public:

        /// \brief Sets a callback for trade result events, including open, close, and errors.
        /// \param callback Function to handle trade result events.
        void set_trade_result_callback(trade_result_callback_t callback) override final {};

        /// \brief Sets a callback to receive updates on account information (balance, connection status, etc.).
        /// \param callback Function to handle account information updates.
        void set_account_info_callback(account_info_callback_t callback) override final {};

        /// \brief Sets a callback to receive new candle data for the trading stream.
        /// \param callback Function to handle incoming candle data.
        void set_candle_info_callback(candle_info_callback_t callback) override final {};

        /// \brief Sets a callback to receive new tick data for the trading stream.
        /// \param callback Function to handle incoming tick data.
        void set_tick_info_callback(tick_info_callback_t callback) override final {};

        /// \brief Sets the authorization data for the platform API.
        /// \param auth_data Authorization data.
        /// \return True if the authorization data was set successfully; false otherwise.
        bool configure_auth(std::unique_ptr<IAuthData> auth_data) override final {};

        /// \brief Places a trade based on the specified trade request.
        /// \param trade_request Trade request details.
        /// \return True if the trade was placed successfully; false otherwise.
        bool place_trade(std::unique_ptr<TradeRequest> trade_request) override final {

        };

        /// \brief Requests historical candle data within a specified time range.
        /// \param request Historical candle data request parameters.
        /// \param callback Callback function to handle the received candle data series.
        void get_candles(
            const CandleHistoryRequest& request,
            std::function<void(const SeriesData&)> callback) override final {};

        /// \brief Requests information about available trading symbols.
        /// \param callback Callback function to handle the received symbol information.
        void get_symbols(std::function<void(const SymbolsInfo&)> callback) override final {};

        /// \brief Initiates a connection to the trading platform.
        /// \param callback Callback to handle connection result with error code.
        void connect(std::function<void(const std::error_code&)> callback) override final {};

        /// \brief Disconnects from the trading platform.
        /// \param callback Callback to handle completion of the disconnection process.
        void disconnect(std::function<void()> callback) override final {};

        /// \brief Processes internal events, such as state updates and message handling.
        /// \details This function should be called periodically to maintain the platform's internal state.
        void process() override final {};

        /// \brief Shuts down the platform API, releasing any allocated resources.
        void shutdown() override final {};

        virtual ~IPlatformAPI() = default;

    private:

        /// \brief Retrieves account information as a boolean value.
        /// \param request The request specifying the type of information to retrieve.
        /// \return Boolean account information.
        bool get_account_info_bool(const AccountInfoRequest& request) override final {};

        /// \brief Retrieves account information as a 64-bit integer.
        /// \param request The request specifying the type of information to retrieve.
        /// \return Account information as int64_t.
        int64_t get_account_info_int64(const AccountInfoRequest& request) override final {};

        /// \brief Retrieves account information as a floating-point number.
        /// \param request The request specifying the type of information to retrieve.
        /// \return Account information as a double.
        double get_account_info_f64(const AccountInfoRequest& request) override final {};

        /// \brief Retrieves account information as a string.
        /// \param request The request specifying the type of information to retrieve.
        /// \return Account information as a string.
        std::string get_account_info_str(const AccountInfoRequest& request) override final {};

    }; // PlatformAPI

} // namespace intrade_bar
} // namespace platforms
} // namespace optionx

#endif // _OPTIONX_PLATFORMS_INTRADERBAR_PLATFORM_API_HPP_INCLUDED
