#pragma once
#ifndef _OPTIONX_TRADE_RESULT_HPP_INCLUDED
#define _OPTIONX_TRADE_RESULT_HPP_INCLUDED

/// \file TradeResult.hpp
/// \brief Contains the TradeResult class representing the result of a trade request.

#include "Enums.hpp"
#include "StringUtils.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <memory>

namespace optionx {

    /// \class TradeResult
    /// \brief Represents the result of a trade request, including details on trade execution and outcome.
    class TradeResult {
    public:
        TradeErrorCode error_code = TradeErrorCode::SUCCESS;   ///< Error code for the trade result.
        std::string error_desc;                                ///< Description of the error, if any.

        std::string option_hash;    ///< Unique hash of the order on the broker side.
        int64_t option_id   = 0;    ///< Unique ID of the order on the broker side.
        double amount       = 0.0;  ///< Total option amount.
        double payout       = 0.0;  ///< Broker payout percentage (from 0.0 to 1.0).
        double profit       = 0.0;  ///< Profit from the trade.
        double balance      = 0.0;  ///< Account balance after trade.
        double open_price   = 0.0;  ///< Entry price for the trade.
        double close_price  = 0.0;  ///< Exit price for the trade.
        int64_t delay       = 0;    ///< Signal sending delay in milliseconds.
        int64_t ping        = 0;    ///< Network latency (ping) in milliseconds.
        int64_t place_date  = 0;    ///< Request placement timestamp (Unix, milliseconds).
        int64_t send_date   = 0;    ///< Request sending timestamp (Unix, milliseconds).
        int64_t open_date   = 0;    ///< Trade opening timestamp (Unix, milliseconds).
        int64_t close_date  = 0;    ///< Trade closing timestamp (Unix, milliseconds).
        TradeState trade_state      = TradeState::UNKNOWN;  ///< Represents the state of the trade, including intermediate and final states.
        TradeState live_state       = TradeState::UNKNOWN;  ///< Represents the real-time state of the trade based on the current price.
        AccountType account_type    = AccountType::UNKNOWN; ///< Type of the account used.
        CurrencyType currency       = CurrencyType::UNKNOWN;///< Currency of the account.
        ApiType api_type            = ApiType::UNKNOWN;     ///< Type of API used for the trade.

        using json = nlohmann::json;

        /// \brief Serializes the trade result into a JSON object.
        /// \param j JSON object to hold serialized data.
        /// \return True if serialization was successful; false otherwise.
        virtual const bool to_json(json &j) const {
            try {
                j["option_id"]      = option_id;
                j["amount"]         = amount;
                j["payout"]         = payout;
                j["profit"]         = profit;
                j["balance"]        = balance;
                j["open_price"]     = open_price;
                j["close_price"]    = close_price;
                j["delay"]          = delay;
                j["ping"]           = ping;
                j["place_date"]     = place_date;
                j["send_date"]      = send_date;
                j["open_date"]      = open_date;
                j["close_date"]     = close_date;
                j["trade_state"]    = to_str(trade_state);
                j["live_state"]     = to_str(live_state);
                j["account_type"]   = to_str(account_type);
                j["currency"]       = to_str(currency);
                j["api_type"]       = to_str(api_type);
                return true;
            } catch(...) {};
            return false;
        };

        /// \brief Deserializes data from a JSON object to populate the trade result.
        /// \param j JSON object containing trade result data.
        /// \return True if deserialization was successful; false otherwise.
        virtual bool from_json(json &j) {
            try {
                option_id   = j["option_id"];
                amount      = j["amount"];
                payout      = j["payout"];
                profit      = j["profit"];
                balance     = j["balance"];
                open_price  = j["open_price"];
                close_price = j["close_price"];
                delay       = j["delay"];
                ping        = j["ping"];
                place_date  = j["place_date"];
                send_date   = j["send_date"];
                open_date   = j["open_date"];
                close_date  = j["close_date"];
                if (!to_enum(to_upper_case(j["trade_state"].get<std::string>()), trade_state)) {
                    return false;
                }
                if (!to_enum(to_upper_case(j["live_state"].get<std::string>()), live_state)) {
                    return false;
                }
                if (!to_enum(to_upper_case(j["account_type"].get<std::string>()), account_type)) {
                    return false;
                }
                if (!to_enum(to_upper_case(j["currency"].get<std::string>()), currency)) {
                    return false;
                }
                if (!to_enum(to_upper_case(j["api_type"].get<std::string>()), api_type)) {
                    return false;
                }
                return true;
            } catch(...) {};
            return false;
        };

        /// \brief Creates a unique pointer to a copy of this TradeResult.
        /// \return Unique pointer to a cloned TradeResult instance.
        virtual std::unique_ptr<TradeResult> clone_unique() const {
            return std::make_unique<TradeResult>(*this);
        }

        /// \brief Creates a shared pointer to a copy of this TradeResult.
        /// \return Shared pointer to a cloned TradeResult instance.
        virtual std::shared_ptr<TradeResult> clone_shared() const {
            return std::make_shared<TradeResult>(*this);
        }

        virtual ~TradeResult() = default;
    }; // TradeResult

    /// \typedef trade_result_t
    /// \brief Alias for a unique pointer to TradeResult.
    typedef std::unique_ptr<TradeResult> trade_result_t;

}; // namespace optionx

#endif // _OPTIONX_TRADE_RESULT_HPP_INCLUDED
