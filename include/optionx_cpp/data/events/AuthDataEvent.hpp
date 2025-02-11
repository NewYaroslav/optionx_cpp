#pragma once
#ifndef _OPTIONX_MODULES_AUTH_DATA_EVENT_HPP_INCLUDED
#define _OPTIONX_MODULES_AUTH_DATA_EVENT_HPP_INCLUDED

/// \file AuthDataEvent.hpp
/// \brief Defines the AuthDataEvent class for handling authorization data events.

namespace optionx::events {

    /// \class AuthDataEvent
    /// \brief Event to provide or update authorization data.
    class AuthDataEvent : public utils::Event {
    public:
        /// \brief Shared pointer to the authorization data.
        std::shared_ptr<IAuthData> auth_data;

        /// \brief Default constructor.
        AuthDataEvent() = default;

        /// \brief Constructor to initialize the event with authorization data.
        /// \param data Shared pointer to the authorization data.
        explicit AuthDataEvent(std::shared_ptr<IAuthData> data)
            : auth_data(std::move(data)) {}

        /// \brief Default virtual destructor.
        virtual ~AuthDataEvent() = default;
    };

} // namespace optionx::events

#endif // _OPTIONX_MODULES_AUTH_DATA_EVENT_HPP_INCLUDED
