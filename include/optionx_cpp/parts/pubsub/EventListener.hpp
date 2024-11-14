#ifndef _OPTIONX_PUBSUB_EVENT_LISTENER_HPP_INCLUDED
#define _OPTIONX_PUBSUB_EVENT_LISTENER_HPP_INCLUDED

/// \file EventListener.hpp
/// \brief Contains the EventListener class for receiving event notifications.

#include "Event.hpp"
#include <memory>

namespace optionx {

    /// \class EventListener
    /// \brief Abstract class that serves as a base for any listener interested in receiving event notifications.
    /// This class allows derived listeners to receive events either as shared pointers or as raw pointers.
    class EventListener {
    public:
        virtual ~EventListener() = default;

        /// \brief Handles an event notification received as a shared pointer.
        /// \param event The event received, passed as a shared pointer.
        virtual void on_event(const std::shared_ptr<Event>& event) = 0;

        /// \brief Handles an event notification received as a raw pointer.
        /// \param event The event received, passed as a raw pointer.
        virtual void on_event(const Event* const event) = 0;

    }; // EventListener

} // namespace optionx

#endif // _OPTIONX_PUBSUB_EVENT_LISTENER_HPP_INCLUDED
