#pragma once
#ifndef _OPTIONX_MODULES_EVENTS_WEBSOCKET_RESULT_EVENT_HPP_INCLUDED
#define _OPTIONX_MODULES_EVENTS_WEBSOCKET_RESULT_EVENT_HPP_INCLUDED

/// \file WebSocketResultEvent.hpp
/// \brief Defines a unified result event reporting the outcome of a WebSocket request.

namespace optionx::events {

    /// \class WebSocketResultEvent
    /// \brief Result/response corresponding to a WebSocketRequestEvent.
    class WebSocketResultEvent : public utils::Event {
    public:
        utils::CorrelationId correlation_id{}; ///< Correlation id matching the originating request.
        WebSocketAction action  = WebSocketAction::OPEN; ///< Action that was executed.
        bool success = false;   ///< Operation status.
        std::string error_message; /// < Error message

        WebSocketResultEvent() = default;

        WebSocketResultEvent(utils::CorrelationId cid,
                             WebSocketAction action,
                             bool success,
                             std::string error_msg = {})
            : correlation_id(cid),
              action(action),
              success(success),
              error_message(std::move(error_msg)) {}

        virtual ~WebSocketResultEvent() = default;

        std::type_index type() const override { 
            return typeid(WebSocketResultEvent); 
        }
        
        const char* name() const override { 
            return "WebSocketResultEvent"; 
        }
    };

} // namespace optionx::events

#endif // _OPTIONX_MODULES_EVENTS_WEBSOCKET_RESULT_EVENT_HPP_INCLUDED
