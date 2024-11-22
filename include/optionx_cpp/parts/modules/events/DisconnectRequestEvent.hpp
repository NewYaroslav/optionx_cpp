#pragma once
#ifndef _OPTIONX_MODULES_DISCONNECT_REQUEST_EVENT_HPP_INCLUDED
#define _OPTIONX_MODULES_DISCONNECT_REQUEST_EVENT_HPP_INCLUDED

/// \file DisconnectRequestEvent.hpp
/// \brief Defines the DisconnectRequestEvent class for handling disconnection requests.

#include "../../pubsub/Event.hpp"
#include <functional>
#include <memory>
#include <string>

namespace optionx {
namespace modules {

    /// \class DisconnectRequestEvent
    /// \brief Event representing a request to terminate a connection.
    class DisconnectRequestEvent : public Event {
    public:
        /// \typedef callback_t
        /// \brief Defines the callback type for disconnection results.
        /// \param success Indicates if the disconnection attempt was successful.
        /// \param reason Provides additional information or an error message if the disconnection failed.
        using callback_t = std::function<void(bool success, const std::string& reason)>;

        callback_t callback; ///< Callback to handle the result of the disconnection request.

        /// \brief Constructor initializing the callback.
        /// \param callback Callback function to handle the disconnection result.
        explicit DisconnectRequestEvent(callback_t callback)
            : callback(std::move(callback)) {}

        /// \brief Default virtual destructor.
        virtual ~DisconnectRequestEvent() = default;
    };

} // namespace modules
} // namespace optionx

#endif // _OPTIONX_MODULES_DISCONNECT_REQUEST_EVENT_HPP_INCLUDED
