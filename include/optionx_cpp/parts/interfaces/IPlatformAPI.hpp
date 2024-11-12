#pragma once
#ifndef _OPTIONX_IPLATFORM_API_HPP_INCLUDED
#define _OPTIONX_IPLATFORM_API_HPP_INCLUDED

/// \file IPlatformAPI.hpp
/// \brief Defines the IPlatformAPI interface for interacting with various trading platforms and APIs.

#include "IAuthData.hpp"
#include "IAccountInfoData.hpp"
#include "../utils/TradeRequest.hpp"
#include "../utils/AccountInfoRequest.hpp"
#include "../utils/CandleInfo.hpp"
#include "../utils/TickInfo.hpp"
#include "../utils/CandleHistoryRequest.hpp"
#include "../utils/SeriesData.hpp"
#include "../utils/SymbolsInfo.hpp"
#include <functional>
#include <string>
#include <memory>

namespace optionx {

    /// \class IPlatformAPI
    /// \brief Interface for interacting with trading platforms and handling trading-related operations.
    class IPlatformAPI {
    public:
        using trade_result_callback_t = std::function<void(std::unique_ptr<TradeRequest>, std::unique_ptr<TradeResult>)>;
        using account_info_callback_t = std::function<void(std::unique_ptr<IAccountInfoData>)>;
        using candle_info_callback_t = std::function<void(const CandleInfo&)>;
        using tick_info_callback_t = std::function<void(const TickInfo&)>;

        /// \brief Sets a callback for trade result events, including open, close, and errors.
        /// \param callback Function to handle trade result events.
        virtual void set_trade_result_callback(trade_result_callback_t callback) = 0;

        /// \brief Sets a callback to receive updates on account information (balance, connection status, etc.).
        /// \param callback Function to handle account information updates.
        virtual void set_account_info_callback(account_info_callback_t callback) = 0;

        /// \brief Sets a callback to receive new candle data for the trading stream.
        /// \param callback Function to handle incoming candle data.
        virtual void set_candle_info_callback(candle_info_callback_t callback) = 0;

        /// \brief Sets a callback to receive new tick data for the trading stream.
        /// \param callback Function to handle incoming tick data.
        virtual void set_tick_info_callback(tick_info_callback_t callback) = 0;

        /// \brief Sets the authorization data for the platform API.
        /// \param auth_data Authorization data.
        /// \return True if the authorization data was set successfully; false otherwise.
        virtual bool configure_auth(std::unique_ptr<IAuthData> auth_data) = 0;

        /// \brief Places a trade based on the specified trade request.
        /// \param trade_request Trade request details.
        /// \return True if the trade was placed successfully; false otherwise.
        virtual bool place_trade(std::unique_ptr<TradeRequest> trade_request) = 0;

        /// \brief Requests historical candle data within a specified time range.
        /// \param request Historical candle data request parameters.
        /// \param callback Callback function to handle the received candle data series.
        virtual void get_candles(
            const CandleHistoryRequest& request,
            std::function<void(const SeriesData&)> callback) = 0;

        /// \brief Requests information about available trading symbols.
        /// \param callback Callback function to handle the received symbol information.
        virtual void get_symbols(std::function<void(const SymbolsInfo&)> callback) = 0;

        /// \brief Retrieves specific account information based on the request type.
        /// \tparam T The expected type of the account information.
        /// \param request The type of account information to retrieve.
        /// \return The account information as type T.
        template<class T>
        inline const T get_account_info(const AccountInfoRequest& request);

        /// \brief Initiates a connection to the trading platform.
        /// \param callback Callback to handle connection result with error code.
        virtual void connect(std::function<void(const std::error_code&)> callback) = 0;

        /// \brief Disconnects from the trading platform.
        /// \param callback Callback to handle completion of the disconnection process.
        virtual void disconnect(std::function<void()> callback) = 0;

        /// \brief Processes internal events, such as state updates and message handling.
        /// \details This function should be called periodically to maintain the platform's internal state.
        virtual void process() = 0;

        /// \brief Shuts down the platform API, releasing any allocated resources.
        virtual void shutdown() = 0;

        virtual ~IPlatformAPI() = default;

    protected:
        /// \brief Retrieves account information as a boolean value.
        /// \param request The request specifying the type of information to retrieve.
        /// \return Boolean account information.
        virtual bool get_account_info_bool(const AccountInfoRequest& request) = 0;

        /// \brief Retrieves account information as a 64-bit integer.
        /// \param request The request specifying the type of information to retrieve.
        /// \return Account information as int64_t.
        virtual int64_t get_account_info_int64(const AccountInfoRequest& request) = 0;

        /// \brief Retrieves account information as a floating-point number.
        /// \param request The request specifying the type of information to retrieve.
        /// \return Account information as a double.
        virtual double get_account_info_f64(const AccountInfoRequest& request) = 0;

        /// \brief Retrieves account information as a string.
        /// \param request The request specifying the type of information to retrieve.
        /// \return Account information as a string.
        virtual std::string get_account_info_str(const AccountInfoRequest& request) = 0;
    };

    // Template specializations for get_account_info

    /// \brief Specialization for retrieving account information as a boolean.
    template<>
    inline const bool IPlatformAPI::get_account_info<bool>(const AccountInfoRequest& request) {
        return get_account_info_bool(request);
    }

    /// \brief Specialization for retrieving account information as a 64-bit integer.
    template<>
    inline const int64_t IPlatformAPI::get_account_info<int64_t>(const AccountInfoRequest& request) {
        return get_account_info_int64(request);
    }

    /// \brief Specialization for retrieving account information as a size_t.
    template<>
    inline const size_t IPlatformAPI::get_account_info<size_t>(const AccountInfoRequest& request) {
        return static_cast<size_t>(get_account_info_int64(request));
    }

    /// \brief Specialization for retrieving account information as a double.
    template<>
    inline const double IPlatformAPI::get_account_info<double>(const AccountInfoRequest& request) {
        return get_account_info_f64(request);
    }

    /// \brief Specialization for retrieving account information as a string.
    template<>
    inline const std::string IPlatformAPI::get_account_info<std::string>(const AccountInfoRequest& request) {
        return get_account_info_str(request);
    }

}; // namespace optionx

#endif // _OPTIONX_IPLATFORM_API_HPP_INCLUDED
