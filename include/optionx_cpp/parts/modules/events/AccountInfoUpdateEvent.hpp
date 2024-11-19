#pragma once
#ifndef _OPTIONX_MODULES_ACCOUNT_INFO_UPDATE_EVENT_HPP_INCLUDED
#define _OPTIONX_MODULES_ACCOUNT_INFO_UPDATE_EVENT_HPP_INCLUDED

/// \file AccountInfoUpdateEvent.hpp
/// \brief Defines the AccountInfoUpdateEvent class for providing updated account information.

#include "../../pubsub/Event.hpp"
#include "../../interfaces/IAccountInfoData.hpp"
#include <memory>

namespace optionx {
namespace modules {

    /// \class AccountInfoUpdateEvent
    /// \brief Event containing updated account information.
    class AccountInfoUpdateEvent : public Event {
    public:
        /// \brief Shared pointer to the updated account information.
        std::shared_ptr<IAccountInfoData> account_info;

        /// \brief Constructor initializing the account information.
        /// \param info Shared pointer to the updated account information.
        explicit AccountInfoUpdateEvent(std::shared_ptr<IAccountInfoData> info)
            : account_info(std::move(info)) {}

        /// \brief Default virtual destructor.
        virtual ~AccountInfoUpdateEvent() = default;
    };

} // namespace modules
} // namespace optionx

#endif // _OPTIONX_MODULES_ACCOUNT_INFO_UPDATE_EVENT_HPP_INCLUDED
