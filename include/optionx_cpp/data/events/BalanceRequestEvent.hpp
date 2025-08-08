#pragma once
#ifndef _OPTIONX_MODULES_BALANCE_REQUEST_EVENT_HPP_INCLUDED
#define _OPTIONX_MODULES_BALANCE_REQUEST_EVENT_HPP_INCLUDED

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

#endif // _OPTIONX_MODULES_BALANCE_REQUEST_EVENT_HPP_INCLUDED
