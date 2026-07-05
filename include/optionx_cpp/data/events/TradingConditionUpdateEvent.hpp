#pragma once
#ifndef OPTIONX_HEADER_DATA_EVENTS_TRADING_CONDITION_UPDATE_EVENT_HPP_INCLUDED
#define OPTIONX_HEADER_DATA_EVENTS_TRADING_CONDITION_UPDATE_EVENT_HPP_INCLUDED

/// \file TradingConditionUpdateEvent.hpp
/// \brief Defines the event carrying broker trading-condition updates.

namespace optionx::events {

    /// \class TradingConditionUpdateEvent
    /// \brief Event containing updated broker trading conditions.
    class TradingConditionUpdateEvent : public utils::Event, public TradingConditionUpdate {
    public:
        /// \brief Constructs an event from a trading-condition update.
        /// \param update Trading-condition update payload.
        explicit TradingConditionUpdateEvent(TradingConditionUpdate update)
            : TradingConditionUpdate(std::move(update)) {}

        /// \brief Default virtual destructor.
        virtual ~TradingConditionUpdateEvent() = default;

        /// \brief Returns the runtime event type.
        std::type_index type() const override {
            return typeid(TradingConditionUpdateEvent);
        }

        /// \brief Returns the diagnostic event name.
        const char* name() const override {
            return "TradingConditionUpdateEvent";
        }
    };

} // namespace optionx::events

#endif // OPTIONX_HEADER_DATA_EVENTS_TRADING_CONDITION_UPDATE_EVENT_HPP_INCLUDED
