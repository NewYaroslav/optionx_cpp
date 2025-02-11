#pragma once
#ifndef _OPTIONX_MODULES_DISCONNECT_REQUEST_EVENT_HPP_INCLUDED
#define _OPTIONX_MODULES_DISCONNECT_REQUEST_EVENT_HPP_INCLUDED

/// \file DisconnectRequestEvent.hpp
/// \brief Defines the DisconnectRequestEvent class for handling disconnection requests.

#include <functional>
#include <memory>
#include <string>

namespace optionx::events {

    /// \class DisconnectRequestEvent
    /// \brief Event representing a request to terminate a connection.
    class DisconnectRequestEvent : public utils::Event {
    public:
        connection_callback_t callback; ///< Callback to handle the result of the disconnection request.

        /// \brief Constructor initializing the callback.
        /// \param callback Callback function to handle the disconnection result.
        explicit DisconnectRequestEvent(connection_callback_t callback)
            : callback(std::move(callback)) {}

        /// \brief Default virtual destructor.
        virtual ~DisconnectRequestEvent() = default;
    };

} // namespace optionx::events

#endif // _OPTIONX_MODULES_DISCONNECT_REQUEST_EVENT_HPP_INCLUDED
