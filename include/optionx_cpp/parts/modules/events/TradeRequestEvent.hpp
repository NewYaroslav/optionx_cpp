#pragma once
#ifndef _OPTIONX_MODULES_TRADE_REQUEST_EVENT_HPP_INCLUDED
#define _OPTIONX_MODULES_TRADE_REQUEST_EVENT_HPP_INCLUDED

/// \file TradeRequestEvent.hpp
/// \brief Defines the TradeRequestEvent class for handling trade request events.

#include "../../pubsub/Event.hpp"
#include "../../utils/TradeRequest.hpp"
#include "../../utils/TradeResult.hpp"

namespace optionx {
namespace modules {

    /// \class TradeRequestEvent
    /// \brief Represents an event containing information about a trade request and its result.
    class TradeRequestEvent : public Event {
    public:
        std::shared_ptr<TradeRequest> request;  ///< Shared pointer to the trade request details.
        std::shared_ptr<TradeResult> result;    ///< Shared pointer to the trade result details.

        /// \brief Constructor initializing the trade request and result.
        /// \param trade_request Shared pointer to the trade request containing trade details.
        /// \param trade_result Shared pointer to the trade result for storing the result of the request.
        explicit TradeRequestEvent(
                std::shared_ptr<TradeRequest> trade_request,
                std::shared_ptr<TradeResult> trade_result)
            : request(std::move(trade_request)), result(std::move(trade_result)) {}

        /// \brief Default virtual destructor.
        virtual ~TradeRequestEvent() = default;
    };

} // namespace modules
} // namespace optionx

#endif // _OPTIONX_MODULES_TRADE_REQUEST_EVENT_HPP_INCLUDED
