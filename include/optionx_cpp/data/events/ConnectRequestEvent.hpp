#pragma once
#ifndef _OPTIONX_MODULES_CONNECT_REQUEST_EVENT_HPP_INCLUDED
#define _OPTIONX_MODULES_CONNECT_REQUEST_EVENT_HPP_INCLUDED

/// \file ConnectRequestEvent.hpp
/// \brief Defines the ConnectRequestEvent class for handling connection requests.

namespace optionx::events {

    /// \class ConnectRequestEvent
    /// \brief Event representing a request to establish a connection with authorization data.
    class ConnectRequestEvent : public utils::Event {
    public:
        connection_callback_t callback; ///< Callback to handle the result of the connection request.

        /// \brief Constructor initializing the callback.
        /// \param callback Callback function to handle the connection result.
        explicit ConnectRequestEvent(connection_callback_t callback)
            : callback(std::move(callback)) {}

        /// \brief Default virtual destructor.
        virtual ~ConnectRequestEvent() = default;
    };

} // namespace optionx::events

#endif // _OPTIONX_MODULES_CONNECT_REQUEST_EVENT_HPP_INCLUDED
