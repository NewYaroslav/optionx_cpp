#pragma once
#ifndef _OPTIONX_MODULES_TRADE_STATUS_EVENT_HPP_INCLUDED
#define _OPTIONX_MODULES_TRADE_STATUS_EVENT_HPP_INCLUDED

/// \file TradeStatusEvent.hpp
/// \brief Defines the TradeStatusEvent class for handling trade status checks.

#include "../../pubsub/Event.hpp"
#include "../../utils/TradeRequest.hpp"
#include "../../utils/TradeResult.hpp"

namespace optionx {
namespace modules {

    /// \class TradeStatusEvent
    /// \brief Event to check the status of a trade.
    class TradeStatusEvent : public Event {
    public:
        std::shared_ptr<TradeRequest> request;  ///< Shared pointer to the trade request details.
        std::shared_ptr<TradeResult> result;    ///< Shared pointer to the trade result details.

        /// \brief Constructor initializing the trade request and result.
        /// \param trade_request Shared pointer to the trade request.
        /// \param trade_result Shared pointer to the trade result.
        explicit TradeStatusEvent(
            std::shared_ptr<TradeRequest> trade_request,
            std::shared_ptr<TradeResult> trade_result)
            : request(std::move(trade_request)), result(std::move(trade_result)) {}

        /// \brief Default virtual destructor.
        virtual ~TradeStatusEvent() = default;
    };

} // namespace modules
} // namespace optionx

#endif // _OPTIONX_MODULES_TRADE_STATUS_EVENT_HPP_INCLUDED
