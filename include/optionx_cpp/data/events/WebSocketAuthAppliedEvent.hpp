#pragma once
#ifndef _OPTIONX_MODULES_EVENTS_WEBSOCKET_AUTH_APPLIED_EVENT_HPP_INCLUDED
#define _OPTIONX_MODULES_EVENTS_WEBSOCKET_AUTH_APPLIED_EVENT_HPP_INCLUDED

namespace optionx::events {

    /// \class WebSocketAuthAppliedEvent
    /// \brief Fired after WebSocketAuthDataEvent has been processed by WS module.
    class WebSocketAuthAppliedEvent : public utils::Event {
    public:
        bool success = false;      ///< True if auth data successfully applied and ready for WS connection.
        std::string error_message; ///< Error message (if success == false)

        WebSocketAuthAppliedEvent() = default;

        /// \brief Constructor to set success and error text.
        WebSocketAuthAppliedEvent(bool success, std::string error_msg = {})
            : success(success), error_message(std::move(error_msg)) {}

        virtual ~WebSocketAuthAppliedEvent() = default;

        std::type_index type() const override {
            return typeid(WebSocketAuthAppliedEvent);
        }

        const char* name() const override {
            return "WebSocketAuthAppliedEvent";
        }
    };

} // namespace optionx::events

#endif // _OPTIONX_MODULES_EVENTS_WEBSOCKET_AUTH_APPLIED_EVENT_HPP_INCLUDED
