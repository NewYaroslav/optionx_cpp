#pragma once
#ifndef _OPTIONX_TRADE_RESULT_HPP_INCLUDED
#define _OPTIONX_TRADE_RESULT_HPP_INCLUDED

/// \file TradeResult.hpp
/// \brief Contains the TradeResult class representing the result of a trade request.

#include <nlohmann/json.hpp>
#include <string>
#include <memory>

namespace optionx {

    /// \class TradeResult
    /// \brief Represents the result of a trade request, including details on trade execution and outcome.
    class TradeResult {
    public:
        // Unique identifier for the trade
        uint64_t trade_id = 0;      ///< Unique ID assigned to each trade (Internal trade ID)

        // Trade execution metadata
        TradeErrorCode error_code = TradeErrorCode::SUCCESS;   ///< Error code for the trade result
        std::string error_desc;                                ///< Human-readable error description

        // Trade identification
        std::string option_hash;    ///< Unique broker-side order identifier
        int64_t option_id = 0;      ///< Numeric broker-side order ID

        // Financial parameters
        double amount = 0.0;        ///< Total investment amount
        double payout = 0.0;        ///< Payout ratio (0.0-1.0)
        double profit = 0.0;        ///< Calculated profit/loss
        double balance = 0.0;       ///< Current account balance

        // Price information
        double open_price = 0.0;    ///< Entry price at position opening
        double close_price = 0.0;   ///< Exit price at position closing

        // Timing parameters (milliseconds)
        int64_t delay = 0;          ///< Order processing delay
        int64_t ping = 0;           ///< Network latency measurement
        int64_t place_date = 0;     ///< Order creation timestamp
        int64_t send_date = 0;      ///< Order transmission timestamp
        int64_t open_date = 0;      ///< Position opening timestamp
        int64_t close_date = 0;     ///< Position closing timestamp

        // State information
        TradeState trade_state = TradeState::UNKNOWN;  ///< Current trade lifecycle state
        TradeState live_state = TradeState::UNKNOWN;   ///< Real-time market state

        // Account context
        AccountType  account_type  = AccountType::UNKNOWN;  ///< Trading account type
        CurrencyType currency      = CurrencyType::UNKNOWN;	///< Account currency type
        PlatformType platform_type = PlatformType::UNKNOWN; ///< API protocol version

        /// \brief Creates a unique pointer to a copy of this TradeResult
        virtual std::unique_ptr<TradeResult> clone_unique() const {
            return std::make_unique<TradeResult>(*this);
        }

        /// \brief Creates a shared pointer to a copy of this TradeResult
        virtual std::shared_ptr<TradeResult> clone_shared() const {
            return std::make_shared<TradeResult>(*this);
        }

        virtual ~TradeResult() = default;

        // JSON serialization/deserialization
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(
            TradeResult,
            error_code,
            error_desc,
            option_hash,
            option_id,
            amount,
            payout,
            profit,
            balance,
            open_price,
            close_price,
            delay,
            ping,
            place_date,
            send_date,
            open_date,
            close_date,
            trade_state,
            live_state,
            account_type,
            currency,
            platform_type
        )
    };

    /// \brief Alias for a unique pointer to TradeResult
    using trade_result_t = std::unique_ptr<TradeResult>;

} // namespace optionx

#endif // _OPTIONX_TRADE_RESULT_HPP_INCLUDED
