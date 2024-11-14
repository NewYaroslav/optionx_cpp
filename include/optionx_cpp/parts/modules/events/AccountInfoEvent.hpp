#pragma once
#ifndef _OPTIONX_MODULES_ACCOUNT_INFO_EVENT_HPP_INCLUDED
#define _OPTIONX_MODULES_ACCOUNT_INFO_EVENT_HPP_INCLUDED

/// \file AccountInfoEvent.hpp
/// \brief Defines the AccountInfoEvent class for managing account information updates.

#include "../../pubsub/Event.hpp"
#include <memory>

namespace optionx {
namespace modules {

    /// \class AccountInfoEvent
    /// \brief Event containing updated account information data.
    class AccountInfoEvent : public IAccountInfoData, public Event {
    public:

        /// \brief Default virtual destructor.
        virtual ~AccountInfoEvent() = default;
    };

} // namespace modules
} // namespace optionx

#endif // _OPTIONX_MODULES_ACCOUNT_INFO_EVENT_HPP_INCLUDED
