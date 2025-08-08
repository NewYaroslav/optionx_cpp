#pragma once
#ifndef _OPTIONX_MODULES_ACCOUNT_INFO_UPDATE_EVENT_HPP_INCLUDED
#define _OPTIONX_MODULES_ACCOUNT_INFO_UPDATE_EVENT_HPP_INCLUDED

/// \file AccountInfoUpdateEvent.hpp
/// \brief Defines the AccountInfoUpdateEvent class for handling account updates as events.

namespace optionx::events {

    /// \class AccountInfoUpdateEvent
    /// \brief Represents an event containing updated account information and status changes.
    class AccountInfoUpdateEvent : public utils::Event, public AccountInfoUpdate {
    public:
        using Status = optionx::AccountUpdateStatus;

        /// \brief Constructs an event for account information updates.
        /// \param info Shared pointer to the updated account information.
        /// \param status The type of account update.
        /// \param message Optional message providing additional context.
        explicit AccountInfoUpdateEvent(
                std::shared_ptr<BaseAccountInfoData> info,
                Status status,
                std::string message = {})
            : AccountInfoUpdate(std::move(info), status, std::move(message)) {}

        /// \brief Default virtual destructor.
        virtual ~AccountInfoUpdateEvent() = default;
        
        
        std::type_index type() const override {
            return typeid(AccountInfoUpdateEvent);
        }

        const char* name() const override {
            return "AccountInfoUpdateEvent";
        }
    };

} // namespace optionx::events

#endif // _OPTIONX_MODULES_ACCOUNT_INFO_UPDATE_EVENT_HPP_INCLUDED
