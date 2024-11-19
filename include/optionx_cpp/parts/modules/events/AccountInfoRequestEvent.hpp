#pragma once
#ifndef _OPTIONX_MODULES_ACCOUNT_INFO_REQUEST_EVENT_HPP_INCLUDED
#define _OPTIONX_MODULES_ACCOUNT_INFO_REQUEST_EVENT_HPP_INCLUDED

/// \file AccountInfoRequestEvent.hpp
/// \brief Defines the AccountInfoRequestEvent class for requesting account information updates.

#include "../../pubsub/Event.hpp"

namespace optionx {
namespace modules {

    /// \class AccountInfoRequestEvent
    /// \brief Event to request an update of account information.
    class AccountInfoRequestEvent : public Event {
    public:
        /// \brief Default constructor.
        AccountInfoRequestEvent() = default;

        /// \brief Default virtual destructor.
        virtual ~AccountInfoRequestEvent() = default;
    };

} // namespace modules
} // namespace optionx

#endif // _OPTIONX_MODULES_ACCOUNT_INFO_REQUEST_EVENT_HPP_INCLUDED
