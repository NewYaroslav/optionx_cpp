#pragma once
#ifndef _OPTIONX_MODULES_EVENTS_WEBSOCKET_AUTH_DATA_EVENT_HPP_INCLUDED
#define _OPTIONX_MODULES_EVENTS_WEBSOCKET_AUTH_DATA_EVENT_HPP_INCLUDED

/// \file WebSocketAuthDataEvent.hpp
/// \brief Defines the WebSocketAuthDataEvent to deliver auth data, API token and cookies to WS modules.

namespace optionx::events {

    /// \class WebSocketAuthDataEvent
    /// \brief Event carrying authorization data for WebSocket handshake.
    ///
    /// Provides three pieces:
    /// 1) Shared pointer to structured auth data (IAuthData),
    /// 2) API token string (e.g., value for "X-API-TOKEN"),
    /// 3) Cookie header string (e.g., "nip-auth-token=...; multibrand_session=...").
    class WebSocketAuthDataEvent : public utils::Event {
    public:
        /// \brief Shared pointer to authorization data.
        std::shared_ptr<IAuthData> auth_data; ///< Shared pointer to authorization data.
        std::string token;                    ///< API token used in headers (e.g., X-API-TOKEN).
        std::string cookies;                  ///< Cookies header line to be sent during WS handshake.

        /// \brief Default constructor.
        WebSocketAuthDataEvent() = default;

        /// \brief Initializes all fields.
        /// \param data Shared pointer to authorization data object.
        /// \param token API token string.
        /// \param cookies Cookie header string (already composed line).
        WebSocketAuthDataEvent(std::shared_ptr<IAuthData> data,
                               std::string token,
                               std::string cookies)
            : auth_data(std::move(data)), 
              token(std::move(token)), 
              cookies(std::move(cookies)) {}

        /// \brief Default virtual destructor.
        virtual ~WebSocketAuthDataEvent() = default;

        /// \brief Quick check that event contains non-empty token and cookies.
        /// \return True if token and cookie header are not empty.
        bool has_credentials() const {
            return !token.empty() && !cookies.empty();
        }

        /// \brief RTTI type id used by the EventBus.
        std::type_index type() const override {
            return typeid(WebSocketAuthDataEvent);
        }

        /// \brief Human-readable event name.
        const char* name() const override {
            return "WebSocketAuthDataEvent";
        }
    };

} // namespace optionx::events

#endif // _OPTIONX_MODULES_EVENTS_WEBSOCKET_AUTH_DATA_EVENT_HPP_INCLUDED
