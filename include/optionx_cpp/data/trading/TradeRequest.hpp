#pragma once
#ifndef _OPTIONX_TRADE_REQUEST_HPP_INCLUDED
#define _OPTIONX_TRADE_REQUEST_HPP_INCLUDED

/// \file TradeRequest.hpp
/// \brief Defines the TradeRequest class, which represents a trade request for execution.

namespace optionx {

    /// \class TradeRequest
    /// \brief Represents a trade request with all necessary parameters for execution.
    class TradeRequest {
    public:
        // Trade parameters
        std::string symbol;          ///< Trading symbol (e.g., "BTC/USD").
        std::string signal_name;     ///< Identifier of the trading strategy or signal.
        std::string user_data;       ///< User-defined metadata attached to the trade request.
        std::string comment;         ///< Optional comment for the trade.
        std::string unique_hash;     ///< Unique hash to prevent duplicate trade execution.

        // Identifiers
        int64_t unique_id   = 0;     ///< Unique identifier of the trade request.
        int64_t account_id  = 0;     ///< Identifier of the associated trading account.

        // Trading enums
        OptionType option_type = OptionType::UNKNOWN;   ///< Option type (e.g., SPRINT or CLASSIC).
        OrderType order_type = OrderType::UNKNOWN;      ///< Trade direction (e.g., BUY or SELL).
        AccountType account_type = AccountType::UNKNOWN;///< Type of account (e.g., DEMO or REAL).
        CurrencyType currency = CurrencyType::UNKNOWN;  ///< Currency used for the trade.

        // Financial parameters
        double amount = 0.0;     ///< Trade amount in the selected currency.
        double refund = 0.0;     ///< Refund percentage (0.0-1.0) in case of a loss.
        double min_payout = 0.0; ///< Minimum acceptable payout percentage.

        // Timing parameters
        int64_t duration = 0;        ///< Trade duration in seconds.
        int64_t expiry_time = 0;     ///< Expiration time as a Unix timestamp.

        /// \brief Callback type for handling trade execution results.
        using callback_t = std::function<void(std::unique_ptr<TradeRequest>,
                                              std::unique_ptr<TradeResult>)>;

        /// \brief Adds a response callback function to the trade request.
        ///
        /// The callback function is triggered upon receiving a trade execution result.
        /// \param callback Function to be called when the trade result is available.
        void add_callback(callback_t callback) {
            m_callbacks.push_back(std::move(callback));
        }

        /// \brief Dispatches registered callbacks with the trade result.
        /// \param request The original trade request.
        /// \param result The result of the trade execution.
        template<class RequestType, class ResultType>
        void dispatch_callbacks(RequestType& request, ResultType& result) {
            for (auto& callback : m_callbacks) {
                callback(request->clone_unique(), result->clone_unique());
            }
        }

        /// \brief Creates a unique pointer to a trade result object.
        ///
        /// This method initializes a new `TradeResult` instance with relevant fields copied from the request.
        /// \return A unique pointer to a new `TradeResult` instance.
        virtual std::unique_ptr<TradeResult> create_trade_result_unique() const {
            auto result = std::make_unique<TradeResult>();
            result->account_type = account_type;
            result->currency = currency;
            result->amount = amount;
            return result;
        }

        /// \brief Creates a shared pointer to a trade result object.
        ///
        /// This method initializes a new `TradeResult` instance with relevant fields copied from the request.
        /// \return A shared pointer to a new `TradeResult` instance.
        virtual std::shared_ptr<TradeResult> create_trade_result_shared() const {
            auto result = std::make_shared<TradeResult>();
            result->account_type = account_type;
            result->currency = currency;
            result->amount = amount;
            return result;
        }

        /// \brief Clones the trade request into a unique pointer.
        /// \return A unique pointer to a cloned `TradeRequest` instance.
        virtual std::unique_ptr<TradeRequest> clone_unique() const {
            return std::make_unique<TradeRequest>(*this);
        }

        /// \brief Clones the trade request into a shared pointer.
        /// \return A shared pointer to a cloned `TradeRequest` instance.
        virtual std::shared_ptr<TradeRequest> clone_shared() const {
            return std::make_shared<TradeRequest>(*this);
        }

        /// \brief Destructor for `TradeRequest`.
        virtual ~TradeRequest() = default;

        /// \brief Defines the JSON serialization format for `TradeRequest`.
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(
            TradeRequest,
            symbol,
            signal_name,
            user_data,
            comment,
            unique_hash,
            unique_id,
            account_id,
            option_type,
            order_type,
            account_type,
            currency,
            amount,
            refund,
            min_payout,
            duration,
            expiry_time
        )

    private:
        std::list<callback_t> m_callbacks; ///< List of registered trade result callbacks.
    };

    /// \brief Type alias for a unique pointer to `TradeRequest`.
    using trade_request_t = std::unique_ptr<TradeRequest>;

    /// \brief Type alias for a trade result callback function.
    using trade_result_callback_t = std::function<void(std::unique_ptr<TradeRequest>, std::unique_ptr<TradeResult>)>;

} // namespace optionx

#endif // _OPTIONX_TRADE_REQUEST_HPP_INCLUDED
