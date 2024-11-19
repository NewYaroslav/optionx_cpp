#pragma once
#ifndef _OPTIONX_MODULES_OPEN_TRADES_EVENT_HPP_INCLUDED
#define _OPTIONX_MODULES_OPEN_TRADES_EVENT_HPP_INCLUDED

#include "../../pubsub/Event.hpp"
#include "../TradeRequest.hpp"
#include "../TradeResult.hpp"

namespace optionx {
namespace modules {

    /// \class OpenTradesEvent
    /// \brief Represents an event to notify about changes in the open trades count.
    class OpenTradesEvent : public Event {
    public:
        int64_t open_trades;                       ///< The current count of open trades.
        std::shared_ptr<TradeRequest> request;     ///< Shared pointer to the trade request details.
        std::shared_ptr<TradeResult> result;       ///< Shared pointer to the trade result details.

        /// \brief Constructor initializing the open trades count and related details.
        /// \param open_trades The updated count of open trades.
        /// \param request Shared pointer to the trade request details.
        /// \param result Shared pointer to the trade result details.
        OpenTradesEvent(
            int64_t open_trades,
            std::shared_ptr<TradeRequest> request,
            std::shared_ptr<TradeResult> result)
            : open_trades(open_trades),
              request(std::move(request)),
              result(std::move(result)) {}

        /// \brief Default virtual destructor.
        virtual ~OpenTradesEvent() = default;
    };

} // namespace modules
} // namespace optionx

#endif // _OPTIONX_MODULES_OPEN_TRADES_EVENT_HPP_INCLUDED
