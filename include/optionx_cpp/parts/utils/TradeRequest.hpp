#pragma once
#ifndef _OPTIONX_TRADE_REQUEST_HPP_INCLUDED
#define _OPTIONX_TRADE_REQUEST_HPP_INCLUDED

/// \file TradeRequest.hpp
/// \brief Contains the TradeRequest class to initiate a trade.

#include "Enums.hpp"
#include "TradeResult.hpp"
#include "StringUtils.hpp"
#include <functional>
#include <memory>
#include <list>

namespace optionx {

    /// \class TradeRequest
    /// \brief Represents a trade request with all necessary parameters for execution.
    class TradeRequest {
    public:
        std::string symbol;                             ///< Symbol for the trade.
        std::string signal_name;                        ///< Name of the signal for tracking.
        std::string user_data;                          ///< User-defined data for tracking.
        std::string comment;                            ///< Optional trade comment.
        std::string unique_hash;                        ///< Unique request hash for tracking.
        int64_t     unique_id   = 0;                    ///< Unique request ID for tracking.
        OptionType  option      = OptionType::UNKNOWN;  ///< Option type.
        OrderType   order       = OrderType::UNKNOWN;   ///< Order type (buy/sell).
        AccountType account     = AccountType::UNKNOWN; ///< Account type, if supported.
        CurrencyType currency   = CurrencyType::UNKNOWN;///< Account currency, if supported.
        double      amount      = 0.0;                  ///< Trade amount.
        double      refund      = 0.0;                  ///< Refund percentage (from 0 to 1.0).
        double      min_payout  = 0.0;                  ///< Minimum payout percentage, if supported (from 0 to 1.0).
        int64_t     duration    = 0;                    ///< Option duration in seconds.
        int64_t     expiry_time = 0;                    ///< Expiration timestamp (Unix, seconds).

        using callback_t = std::function<void(std::unique_ptr<TradeRequest>, std::unique_ptr<TradeResult>)>;

        /// \brief Adds a callback function to the request.
        /// \param callback Function to handle the response.
        void add_callback(callback_t callback) {
            m_callbacks.push_back(std::move(callback));
        }

        /// \brief Executes all registered callback functions.
        /// \param request Pointer to the original trade request.
        /// \param result Pointer to the trade result.
        void invoke_callbacks(
                std::unique_ptr<TradeRequest> request,
                std::unique_ptr<TradeResult> result) {
            for (auto& callback : m_callbacks) {
                callback(
                    std::move(request->clone_unique()),
                    std::move(result->clone_unique()));
            }
        }

        using json = nlohmann::json;

        /// \brief Serializes the trade request to a JSON object.
        /// \param j JSON object to hold serialized data.
        /// \return True if serialization was successful; false otherwise.
        virtual bool to_json(json &j) const {
            try {
                j["symbol"]         = symbol;
                j["signal_name"]    = signal_name;
                j["user_data"]      = user_data;
                j["comment"]        = comment;
                j["unique_hash"]    = unique_hash;
                j["unique_id"]      = unique_id;
                j["option"]         = to_str(option);
                j["order"]          = to_str(order);
                j["account"]        = to_str(account);
                j["currency"]       = to_str(currency);
                j["amount"]         = amount;
                j["refund"]         = refund;
                j["min_payout"]     = min_payout;
                j["duration"]       = duration;
                j["expiry_time"]    = expiry_time;
                return true;
            } catch(...) {};
            return false;
        };

        /// \brief Deserializes data from a JSON object to populate the trade request.
        /// \param j JSON object containing trade request data.
        /// \return True if deserialization was successful; false otherwise.
        virtual bool from_json(json &j) {
            try {
                symbol = j["symbol"];
                if (j.contains("signal_name"))  signal_name = j["signal_name"];
                if (j.contains("user_data"))    user_data   = j["user_data"];
                if (j.contains("comment"))      comment     = j["comment"];
                if (j.contains("unique_hash"))  unique_hash = j["unique_hash"];
                if (j.contains("unique_id"))    unique_id   = j["unique_id"];

                if (!to_enum(to_upper_case(j["option"].get<std::string>()), option)) {
                    return false;
                }
                if (!to_enum(to_upper_case(j["order"].get<std::string>()), order)) {
                    return false;
                }
                if (j.contains("account")) {
                    if (!to_enum(to_upper_case(j["account"].get<std::string>()), account)) {
                        return false;
                    }
                }
                if (j.contains("currency")) {
                    if (!to_enum(to_upper_case(j["currency"].get<std::string>()), currency)) {
                        return false;
                    }
                }
                if (j.contains("amount")) amount = j["amount"];
                if (j.contains("refund")) refund = j["refund"];
                if (j.contains("min_payout")) min_payout = j["min_payout"];
                if (j.contains("duration")) duration = j["duration"];
                if (j.contains("expiry_time")) expiry_time = j["expiry_time"];
            } catch(...) {};
            return false;
        };

        /// \brief Creates a unique pointer to a trade result instance with initialized data.
        /// \return Unique pointer to a new trade result instance.
        virtual std::unique_ptr<TradeResult> create_trade_result_unique() const {
            std::unique_ptr<TradeResult> result = std::make_unique<TradeResult>();
            result->account     = account;
            result->currency    = currency;
            result->amount      = amount;
            return result;
        }

        /// \brief Creates a shared pointer to a trade result instance with initialized data.
        /// \return Shared pointer to a new trade result instance.
        virtual std::shared_ptr<TradeResult> create_trade_result_shared() const {
            std::shared_ptr<TradeResult> result = std::make_shared<TradeResult>();
            result->account     = account;
            result->currency    = currency;
            result->amount      = amount;
            return result;
        }

        /// \brief Creates a unique pointer copy of the current trade request.
        /// \return Unique pointer to a copy of this trade request.
        virtual std::unique_ptr<TradeRequest> clone_unique() const {
            return std::make_unique<TradeRequest>(*this);
        }

        /// \brief Creates a shared pointer copy of the current trade request.
        /// \return Shared pointer to a copy of this trade request.
        virtual std::shared_ptr<TradeRequest> clone_shared() const {
            return std::make_shared<TradeRequest>(*this);
        }

        virtual ~TradeRequest() = default;

    private:
        std::list<callback_t> m_callbacks; ///< List of callbacks to be invoked after trade execution.
    }; // TradeRequest

    /// \typedef trade_request_t
    /// \brief Alias for a unique pointer to TradeRequest.
    typedef std::unique_ptr<TradeRequest> trade_request_t;

}; // namespace optionx

#endif // _OPTIONX_TRADE_REQUEST_HPP_INCLUDED
