#pragma once
#ifndef _OPTIONX_MODULES_RESTART_AUTH_EVENT_HPP_INCLUDED
#define _OPTIONX_MODULES_RESTART_AUTH_EVENT_HPP_INCLUDED

/// \file RestartAuthEvent.hpp
/// \brief

namespace optionx::events {

    /// \class RestartAuthEvent
    /// \brief Event to provide or update authorization data.
    class RestartAuthEvent : public utils::Event {
    public:

        /// \brief Default constructor.
        RestartAuthEvent() = default;

        /// \brief Default virtual destructor.
        virtual ~RestartAuthEvent() = default;
		
        std::type_index type() const override {
            return typeid(RestartAuthEvent);
        }

        const char* name() const override {
            return "RestartAuthEvent";
        }
    };

} // namespace optionx::events

#endif // _OPTIONX_MODULES_RESTART_AUTH_EVENT_HPP_INCLUDED
