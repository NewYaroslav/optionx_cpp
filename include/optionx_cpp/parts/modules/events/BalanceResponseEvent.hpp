#pragma once
#ifndef _OPTIONX_MODULES_BALANCE_RESPONSE_EVENT_HPP_INCLUDED
#define _OPTIONX_MODULES_BALANCE_RESPONSE_EVENT_HPP_INCLUDED

/// \file BalanceResponseEvent.hpp
/// \brief Defines the BalanceResponseEvent class for responding to balance requests.

#include "../../pubsub/Event.hpp"
#include "../../utils/TradeRequest.hpp"
#include "../../utils/TradeResult.hpp"

namespace optionx {
namespace modules {

    /// \class BalanceResponseEvent
    /// \brief Represents a response event containing information about account balance.
    class BalanceResponseEvent : public Event {
    public:
        std::shared_ptr<TradeRequest> request;      ///< Shared pointer to the original trade request.
        std::shared_ptr<TradeResult>  result;       ///< Shared pointer to the trade result details.
        double                        balance;      ///< Retrieved account balance.
        CurrencyType                  currency;     ///< Currency type of the account.
        AccountType                   account_type; ///< Type of account (e.g., demo or real).

        /// \brief Constructor initializing response details for a balance request.
        /// \param trade_request Shared pointer to the original trade request.
        /// \param trade_result Shared pointer to the trade result details.
        /// \param balance The retrieved account balance.
        /// \param currency The currency type of the account.
        /// \param account_type The type of account (e.g., demo or real).
        explicit BalanceResponseEvent(
                std::shared_ptr<TradeRequest> trade_request,
                std::shared_ptr<TradeResult> trade_result,
                double balance,
                CurrencyType currency,
                AccountType account_type)
            : request(std::move(trade_request)),
              result(std::move(trade_result)),
              balance(balance),
              currency(currency),
              account_type(account_type) {}

        /// \brief Default virtual destructor.
        virtual ~BalanceResponseEvent() = default;
    };

} // namespace modules
} // namespace optionx

#endif // _OPTIONX_MODULES_BALANCE_RESPONSE_EVENT_HPP_INCLUDED
