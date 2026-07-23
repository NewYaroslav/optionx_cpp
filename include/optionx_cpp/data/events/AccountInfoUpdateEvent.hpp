#pragma once
#ifndef OPTIONX_HEADER_DATA_EVENTS_ACCOUNT_INFO_UPDATE_EVENT_HPP_INCLUDED
#define OPTIONX_HEADER_DATA_EVENTS_ACCOUNT_INFO_UPDATE_EVENT_HPP_INCLUDED

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

        /// \brief Constructs an event for a specific internal OptionX account.
        /// \param info Shared pointer to the updated account information.
        /// \param status The type of account update.
        /// \param account_id Stable OptionX account ID.
        /// \param message Optional message providing additional context.
        explicit AccountInfoUpdateEvent(
                std::shared_ptr<BaseAccountInfoData> info,
                Status status,
                std::int64_t account_id,
                std::string message = {})
            : AccountInfoUpdate(std::move(info), status, account_id, std::move(message)) {}

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

#endif // OPTIONX_HEADER_DATA_EVENTS_ACCOUNT_INFO_UPDATE_EVENT_HPP_INCLUDED
