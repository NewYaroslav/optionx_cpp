#pragma once
#ifndef OPTIONX_HEADER_DATA_EVENTS_WEB_SOCKET_REQUEST_EVENT_HPP_INCLUDED
#define OPTIONX_HEADER_DATA_EVENTS_WEB_SOCKET_REQUEST_EVENT_HPP_INCLUDED

/// \file WebSocketRequestEvent.hpp
/// \brief Defines a unified request event to control WebSocket actions.

namespace optionx::events {

    /// \enum WebSocketAction
    /// \brief Action to perform on a WebSocket connection.
    enum class WebSocketAction {
        OPEN = 0,
        CLOSE = 1,
        RECONNECT = 2
    };

    /// \class WebSocketRequestEvent
    /// \brief Unified request to control WebSocket lifecycle (open/close/reconnect).
    class WebSocketRequestEvent : public utils::Event {
    public:
        utils::CorrelationId correlation_id{}; ///< Correlation id to bind request with a resulting response (0 if not applicable).
        WebSocketAction action  = WebSocketAction::OPEN; ///< Desired action (OPEN/CLOSE/RECONNECT).
        std::string payload;         ///< Optional payload for future extensions (JSON, extra headers, etc.).

        WebSocketRequestEvent() = default;

        WebSocketRequestEvent(utils::CorrelationId cid,
                              WebSocketAction action,
                              std::string payload = {})
            : correlation_id(cid),
              action(action),
              payload(std::move(payload)) {}

        virtual ~WebSocketRequestEvent() = default;

        std::type_index type() const override { 
			return typeid(WebSocketRequestEvent); 
		}
        
		const char* name() const override { 
			return "WebSocketRequestEvent"; 
		}
    };

} // namespace optionx::events

#endif // OPTIONX_HEADER_DATA_EVENTS_WEB_SOCKET_REQUEST_EVENT_HPP_INCLUDED
