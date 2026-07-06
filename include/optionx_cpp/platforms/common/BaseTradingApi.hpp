#pragma once
#ifndef OPTIONX_HEADER_PLATFORMS_COMMON_BASE_TRADING_API_HPP_INCLUDED
#define OPTIONX_HEADER_PLATFORMS_COMMON_BASE_TRADING_API_HPP_INCLUDED

/// \file BaseTradingApi.hpp
/// \brief Declares the public trading API role for platform facades.

namespace optionx::platforms {

    /// \class BaseTradingApi
    /// \brief Role interface for endpoints that can place trades and retrieve trade results.
    class BaseTradingApi {
    public:
        using trade_result_callback_t = std::function<void(std::unique_ptr<TradeRequest>, std::unique_ptr<TradeResult>)>;
        using trade_result_check_callback_t = std::function<void(std::unique_ptr<TradeResult>)>;
        using trade_history_callback_t = std::function<void(TradeHistoryResult)>;
        using trade_id_provider_t = std::function<std::uint32_t()>;

        /// \brief Virtual destructor for polymorphic trading API implementations.
        virtual ~BaseTradingApi() = default;

        /// \brief Returns a reference to the trade result callback.
        /// \return Reference to the stored trade result callback, or a null function if not set.
        virtual trade_result_callback_t& on_trade_result() {
            static trade_result_callback_t null_callback;
            return null_callback;
        }

        /// \brief Returns provider used by concrete platforms to initialize TradeRequest::trade_id.
        /// \return Mutable provider callback, or a null provider for base platforms.
        virtual trade_id_provider_t& on_trade_id() {
            static trade_id_provider_t null_provider;
            return null_provider;
        }

        /// \brief Places a trade based on the specified trade request.
        /// \param trade_request Trade request details.
        /// \return True if the trade was accepted for processing; false otherwise.
        virtual bool place_trade(std::unique_ptr<TradeRequest> trade_request) {
            (void)trade_request;
            return false;
        }

        /// \brief Requests the final broker-side result for a previously opened trade.
        /// \param query Broker-side trade identity and retry settings.
        /// \param result Partially filled result object to update with broker data.
        /// \param callback Callback receiving the updated result.
        /// \return True if the request was accepted for processing; false otherwise.
        virtual bool fetch_trade_result(
                TradeResultQuery query,
                std::unique_ptr<TradeResult> result,
                trade_result_check_callback_t callback) {
            (void)query;
            (void)result;
            (void)callback;
            return false;
        }

        /// \brief Requests closed trade history for the account.
        /// \param request Trade history range and timestamp selection.
        /// \param callback Callback function to receive the history result.
        /// \return True if the request was accepted for processing; false otherwise.
        virtual bool fetch_trade_history(
                const TradeHistoryRequest& request,
                trade_history_callback_t callback) {
            (void)request;
            (void)callback;
            return false;
        }

        /// \brief Requests all available closed trade history for the current account.
        /// \param callback Callback function to receive the history result.
        /// \return True if the request was accepted for processing; false otherwise.
        virtual bool fetch_trade_history(trade_history_callback_t callback) {
            return fetch_trade_history(TradeHistoryRequest::all(), std::move(callback));
        }
    };

} // namespace optionx::platforms

#endif // OPTIONX_HEADER_PLATFORMS_COMMON_BASE_TRADING_API_HPP_INCLUDED
