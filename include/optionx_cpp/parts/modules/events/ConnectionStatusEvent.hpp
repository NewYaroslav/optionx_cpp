#pragma once
#ifndef _OPTIONX_MODULES_CONNECTION_STATUS_EVENT_HPP_INCLUDED
#define _OPTIONX_MODULES_CONNECTION_STATUS_EVENT_HPP_INCLUDED

/// \file ConnectionStatusEvent.hpp
/// \brief Defines the ConnectionStatusEvent class for reporting connection status updates.

#include "../../pubsub/Event.hpp"
#include <string>

namespace optionx {
namespace modules {

    /// \class ConnectionStatusEvent
    /// \brief Event representing the status of a connection.
    class ConnectionStatusEvent : public Event {
    public:

        /// \enum Status
        /// \brief Represents the type of account update or status.
        enum class Status {
            CONNECTING,           ///< Connecting to the account.
            CONNECTED,            ///< Connection to the account established.
            DISCONNECTED,         ///< Connection to the account disconnected.
        };

        std::shared_ptr<IAccountInfoData> account_info; ///< Shared pointer to the updated account information.
        Status      status;  ///< Current connection status.
        std::string message; ///< Additional information or error message.

        /// \brief Constructor initializing the connection status and optional message.
        /// \param info Shared pointer to the updated account information.
        /// \param status The current connection status.
        /// \param message Optional message providing additional context.
        explicit ConnectionStatusEvent(std::shared_ptr<IAccountInfoData> info, Status status, std::string message = "")
            : account_info(std::move(info)), status(status), message(std::move(message)) {}

        /// \brief Default virtual destructor.
        virtual ~ConnectionStatusEvent() = default;
    };

} // namespace modules
} // namespace optionx

#endif // _OPTIONX_MODULES_CONNECTION_STATUS_EVENT_HPP_INCLUDED
