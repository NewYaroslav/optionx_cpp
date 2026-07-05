#pragma once
#ifndef OPTIONX_HEADER_DATA_EVENTS_BALANCE_REQUEST_EVENT_HPP_INCLUDED
#define OPTIONX_HEADER_DATA_EVENTS_BALANCE_REQUEST_EVENT_HPP_INCLUDED

/// \file BalanceRequestEvent.hpp
/// \brief Defines the BalanceRequestEvent class for handling account balance request events.

namespace optionx::events {

    /// \class BalanceRequestEvent
    /// \brief Event to request account balance information, holding pointers to trade request and result details.
    class BalanceRequestEvent : public utils::Event {
    public:

        /// \brief Default virtual destructor.
        virtual ~BalanceRequestEvent() = default;
        
        std::type_index type() const override {
            return typeid(BalanceRequestEvent);
        }

        const char* name() const override {
            return "BalanceRequestEvent";
        }
    };

} // namespace optionx::events

#endif // OPTIONX_HEADER_DATA_EVENTS_BALANCE_REQUEST_EVENT_HPP_INCLUDED
