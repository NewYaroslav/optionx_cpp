#pragma once
#ifndef _OPTIONX_MODULES_TRADE_TRANSACTION_EVENT_HPP_INCLUDED
#define _OPTIONX_MODULES_TRADE_TRANSACTION_EVENT_HPP_INCLUDED

/// \file TradeTransactionEvent.hpp
/// \brief Defines the TradeTransactionEvent class used for handling trade transaction events.

namespace optionx::events {

    /// \class TradeTransactionEvent
    /// \brief Represents an event containing information about a trade transaction.
    class TradeTransactionEvent : public utils::Event {
    public:
        std::shared_ptr<TradeRequest> request;  ///< Shared pointer to the trade request details.
        std::shared_ptr<TradeResult> result;    ///< Shared pointer to the trade result details.

        /// \brief Constructs a TradeTransactionEvent with a trade request and API type.
        /// \param trade_request A unique pointer to the trade request that initializes the event.
        /// \param platform_type
        TradeTransactionEvent(std::unique_ptr<TradeRequest> &trade_request, PlatformType platform_type) {
            result = trade_request->create_trade_result_shared();
            result->place_date = OPTIONX_TIMESTAMP_MS;
            result->platform_type = platform_type;
            request = std::shared_ptr<TradeRequest>(trade_request.release());
        }

        /// \brief Default virtual destructor.
        virtual ~TradeTransactionEvent() = default;
    };

} // namespace optionx::events

#endif // _OPTIONX_MODULES_TRADE_TRANSACTION_EVENT_HPP_INCLUDED
