#pragma once
#ifndef _OPTIONX_ACCOUNT_INFO_REQUEST_HPP_INCLUDED
#define _OPTIONX_ACCOUNT_INFO_REQUEST_HPP_INCLUDED

/// \file AccountInfoRequest.hpp
/// \brief Contains the AccountInfoRequest class for account data requests.

#include "TradeRequest.hpp"

namespace optionx {

    /// \class AccountInfoRequest
    /// \brief Handles request parameters for retrieving account information.
    class AccountInfoRequest {
    public:
        AccountInfoType type        = AccountInfoType::UNKNOWN; ///< Type of account information requested.
        std::string     symbol;                                 ///< Trade symbol.
        double          amount      = 0.0;                      ///< Option amount.
        double          refund      = 0.0;                      ///< Refund percentage (0 to 1.0).
        double          min_payout  = 0.0;                      ///< Minimum payout percentage, if supported (from 0 to 1.0).
        int64_t         account_id  = 0;                        ///< Identifier for the account.
        AccountType     account_type = AccountType::UNKNOWN;    ///< Account type, if supported.
        CurrencyType    currency    = CurrencyType::UNKNOWN;    ///< Account currency, if supported.
        OptionType      option_type = OptionType::UNKNOWN;      ///< Option type.
        OrderType       order_type  = OrderType::UNKNOWN;       ///< Order type.
        int             duration    = 0;                        ///< Option duration.
        int64_t         expiry_time = 0;                        ///< Expiration timestamp (Unix, sec).
        int64_t         timestamp   = 0;                        ///< General timestamp (Unix, sec).

        /// \brief Sets data based on a TradeRequest instance.
        /// \param request The trade request containing data to copy.
        /// \param info_type The type of account information to request.
        inline void set_data(const TradeRequest &request, AccountInfoType info_type = AccountInfoType::UNKNOWN) {
            type        = info_type;
            symbol      = request.symbol;
            amount      = request.amount;
            refund      = request.refund;
            min_payout  = request.min_payout;
            account_id = request.account_id;
            account_type = request.account_type;
            currency    = request.currency;
            option_type = request.option_type;
            order_type  = request.order_type;
            duration    = request.duration;
            expiry_time = request.expiry_time;
        }

        /// \brief Sets data based on a pointer to a TradeRequest instance.
        /// \param request Pointer to the trade request containing data to copy.
        /// \param info_type The type of account information to request.
        inline void set_data(const TradeRequest* request, AccountInfoType info_type = AccountInfoType::UNKNOWN) {
            if (request) {
                type        = info_type;
                symbol      = request->symbol;
                amount      = request->amount;
                refund      = request->refund;
                min_payout  = request->min_payout;
                account_id  = request->account_id;
                account_type = request->account_type;
                currency    = request->currency;
                option_type = request->option_type;
                order_type  = request->order_type;
                duration    = request->duration;
                expiry_time = request->expiry_time;
            }
        }

        AccountInfoRequest() = default;

        /// \brief Constructs AccountInfoRequest from a TradeRequest instance.
        /// \param request The trade request containing initial data.
        /// \param info_type The type of account information to request.
        AccountInfoRequest(const TradeRequest& request, AccountInfoType info_type = AccountInfoType::UNKNOWN) {
            set_data(request, info_type);
        }

        /// \brief Constructs AccountInfoRequest from a pointer to TradeRequest.
        /// \param request Pointer to the trade request containing initial data.
        /// \param info_type The type of account information to request.
        AccountInfoRequest(const TradeRequest* request, AccountInfoType info_type = AccountInfoType::UNKNOWN) {
            set_data(request, info_type);
        }

        /// \brief Constructs AccountInfoRequest from a unique pointer to TradeRequest.
        /// \param request Unique pointer to the trade request containing initial data.
        /// \param info_type The type of account information to request.
        AccountInfoRequest(std::unique_ptr<TradeRequest>& request, AccountInfoType info_type = AccountInfoType::UNKNOWN) {
            set_data(request.get(), info_type);
        }

        /// \brief Constructs AccountInfoRequest from a shared pointer to TradeRequest.
        /// \param request Shared pointer to the trade request containing initial data.
        /// \param info_type The type of account information to request.
        AccountInfoRequest(std::shared_ptr<TradeRequest>& request, AccountInfoType info_type = AccountInfoType::UNKNOWN) {
            set_data(request.get(), info_type);
        }

        virtual ~AccountInfoRequest() = default;
    }; // AccountInfoRequest

}; // namespace optionx

#endif // _OPTIONX_ACCOUNT_INFO_REQUEST_HPP_INCLUDED
