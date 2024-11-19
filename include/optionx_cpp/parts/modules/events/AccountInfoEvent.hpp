#pragma once
#ifndef _OPTIONX_MODULES_ACCOUNT_INFO_EVENT_HPP_INCLUDED
#define _OPTIONX_MODULES_ACCOUNT_INFO_EVENT_HPP_INCLUDED

/// \file AccountInfoEvent.hpp
/// \brief Defines the AccountInfoEvent class for managing account information updates.

#include "../../pubsub/Event.hpp"
#include "../../interfaces/IAccountInfoData.hpp"
#include <memory>

namespace optionx {
namespace modules {

    /// \class AccountInfoEvent
    /// \brief Represents an event containing updated account information data.
    ///
    /// This event is used to transmit updated account information
    /// encapsulated within the `IAccountInfoData` interface.
    class AccountInfoEvent : public Event {
    public:
        std::shared_ptr<IAccountInfoData> account_info; ///< Shared pointer to the updated account information.

        /// \brief Default constructor.
        AccountInfoEvent() = default;

        /// \brief Constructor to initialize account information data.
        /// \param info Shared pointer to the updated `IAccountInfoData`.
        explicit AccountInfoEvent(std::shared_ptr<IAccountInfoData> info)
            : account_info(std::move(info)) {}

        /// \brief Default virtual destructor.
        virtual ~AccountInfoEvent() = default;
    };

} // namespace modules
} // namespace optionx

#endif // _OPTIONX_MODULES_ACCOUNT_INFO_EVENT_HPP_INCLUDED
