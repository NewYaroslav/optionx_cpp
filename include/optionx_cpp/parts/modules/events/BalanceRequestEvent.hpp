#pragma once
#ifndef _OPTIONX_MODULES_BALANCE_REQUEST_EVENT_HPP_INCLUDED
#define _OPTIONX_MODULES_BALANCE_REQUEST_EVENT_HPP_INCLUDED

/// \file BalanceRequestEvent.hpp
/// \brief Defines the BalanceRequestEvent class for handling account balance request events.

#include "../../pubsub/Event.hpp"
#include "../../utils/TradeRequest.hpp"
#include "../../utils/TradeResult.hpp"

namespace optionx {
namespace modules {

    /// \class BalanceRequestEvent
    /// \brief Event to request account balance information, holding pointers to trade request and result details.
    class BalanceRequestEvent : public Event {
    public:
        std::shared_ptr<TradeRequest> request;  ///< Shared pointer to the trade request details.
        std::shared_ptr<TradeResult> result;    ///< Shared pointer to the trade result details.

        /// \brief Constructor initializing the request and result pointers.
        /// \param trade_request Shared pointer to the trade request details.
        /// \param trade_result Shared pointer to the trade result details.
        explicit BalanceRequestEvent(
                std::shared_ptr<TradeRequest> trade_request,
                std::shared_ptr<TradeResult> trade_result)
            : request(std::move(trade_request)), result(std::move(trade_result)) {}

        /// \brief Default virtual destructor.
        virtual ~BalanceRequestEvent() = default;
    };

} // namespace modules
} // namespace optionx

#endif // _OPTIONX_MODULES_BALANCE_REQUEST_EVENT_HPP_INCLUDED
