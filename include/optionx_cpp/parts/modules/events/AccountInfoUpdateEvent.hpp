#pragma once
#ifndef _OPTIONX_MODULES_ACCOUNT_INFO_UPDATE_EVENT_HPP_INCLUDED
#define _OPTIONX_MODULES_ACCOUNT_INFO_UPDATE_EVENT_HPP_INCLUDED

/// \file AccountInfoUpdateEvent.hpp
/// \brief Defines the AccountInfoUpdateEvent class for providing updated account information and statuses.

#include "../../pubsub/Event.hpp"
#include "../../interfaces/IAccountInfoData.hpp"
#include <memory>
#include <string>

namespace optionx {
namespace modules {

    /// \class AccountInfoUpdateEvent
    /// \brief Event containing updated account information and status changes.
    class AccountInfoUpdateEvent : public Event {
    public:
        /// \enum Status
        /// \brief Represents the type of account update or status.
        enum class Status {
            BALANCE_UPDATED,      ///< Account balance was updated.
            ACCOUNT_TYPE_CHANGED, ///< Account type was changed.
            OPEN_TRADES_CHANGED,  ///< Number of open trades was changed.
            CONNECTING,           ///< Connecting to the account.
            CONNECTED,            ///< Connection to the account established.
            DISCONNECTED,         ///< Connection to the account disconnected.
            ///ERROR                 ///< An error occurred.
        };

        std::shared_ptr<IAccountInfoData> account_info; ///< Shared pointer to the updated account information.
        Status      status;                            ///< Current account status or update type.
        std::string message;                           ///< Additional information or error message.

        /// \brief Constructor for account information and status.
        /// \param info Shared pointer to the updated account information.
        /// \param status The type of account update or status.
        /// \param message Additional message providing context or error information (optional).
        explicit AccountInfoUpdateEvent(
            std::shared_ptr<IAccountInfoData> info,
            Status status,
            std::string message = std::string())
            : account_info(std::move(info)), status(status), message(std::move(message)) {}

        /// \brief Default virtual destructor.
        virtual ~AccountInfoUpdateEvent() = default;
    };

} // namespace modules
} // namespace optionx

#endif // _OPTIONX_MODULES_ACCOUNT_INFO_UPDATE_EVENT_HPP_INCLUDED
