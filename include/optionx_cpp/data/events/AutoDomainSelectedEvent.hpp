#pragma once
#ifndef _OPTIONX_MODULES_EVENTS_AUTO_DOMAIN_SELECTED_EVENT_HPP_INCLUDED
#define _OPTIONX_MODULES_EVENTS_AUTO_DOMAIN_SELECTED_EVENT_HPP_INCLUDED

/// \file AutoDomainSelectedEvent.hpp
/// \brief Defines the AutoDomainSelectedEvent class for notifying about domain auto-selection results.

namespace optionx::events {

    /// \class AutoDomainSelectedEvent
    /// \brief Event indicating the result of automatic domain discovery.
    class AutoDomainSelectedEvent : public utils::Event {
    public:
        /// \brief Selected domain or host, e.g. "https://intrade26.bar".
        std::string selected_host;

        /// \brief Whether domain selection was successful.
        bool success = false;

        /// \brief Default constructor.
        AutoDomainSelectedEvent() = default;

        /// \brief Constructor initializing all fields.
        /// \param success Whether a working domain was found.
        /// \param host The selected domain or empty if not found.
        AutoDomainSelectedEvent(bool success, std::string host)
            : selected_host(std::move(host)), success(success) {}

        /// \brief Default virtual destructor.
        virtual ~AutoDomainSelectedEvent() = default;
        
        std::type_index type() const override {
            return typeid(AutoDomainSelectedEvent);
        }

        const char* name() const override {
            return "AutoDomainSelectedEvent";
        }
    };

} // namespace optionx::events

#endif // _OPTIONX_MODULES_EVENTS_AUTO_DOMAIN_SELECTED_EVENT_HPP_INCLUDED
