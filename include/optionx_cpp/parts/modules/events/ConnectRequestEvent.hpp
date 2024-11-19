#pragma once
#ifndef _OPTIONX_MODULES_CONNECT_REQUEST_EVENT_HPP_INCLUDED
#define _OPTIONX_MODULES_CONNECT_REQUEST_EVENT_HPP_INCLUDED

/// \file ConnectRequestEvent.hpp
/// \brief Defines the ConnectRequestEvent class for handling connection requests.

#include "../../pubsub/Event.hpp"
#include "../../interfaces/IAuthData.hpp"
#include <functional>
#include <memory>
#include <string>

namespace optionx {
namespace modules {

    /// \class ConnectRequestEvent
    /// \brief Event representing a request to establish a connection with authorization data.
    class ConnectRequestEvent : public Event {
    public:
        /// \typedef callback_t
        /// \brief Defines the callback type for connection results.
        /// \param success Indicates if the connection attempt was successful.
        /// \param reason Provides additional information or an error message if the connection failed.
        /// \param auth_data Unique pointer to the authorization data used in the connection.
        using callback_t = std::function<void(bool success, const std::string& reason, std::unique_ptr<IAuthData> auth_data)>;

        callback_t callback; ///< Callback to handle the result of the connection request.

        /// \brief Constructor initializing the callback.
        /// \param callback Callback function to handle the connection result.
        explicit ConnectRequestEvent(callback_t callback)
            : callback(std::move(callback)) {}

        /// \brief Default virtual destructor.
        virtual ~ConnectRequestEvent() = default;
    };

} // namespace modules
} // namespace optionx

#endif // _OPTIONX_MODULES_CONNECT_REQUEST_EVENT_HPP_INCLUDED
