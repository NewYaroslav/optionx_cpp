#pragma once
#ifndef _OPTIONX_PUBSUB_EVENT_MEDIATOR_HPP_INCLUDED
#define _OPTIONX_PUBSUB_EVENT_MEDIATOR_HPP_INCLUDED

/// \file EventMediator.hpp
/// \brief Defines the EventMediator class for managing subscriptions and notifications through EventHub.

#include "EventHub.hpp"

namespace optionx {

    /// \class EventMediator
    /// \brief Provides an interface for subscribing to and notifying events through EventHub.
    class EventMediator : public EventListener {
    public:
        /// \brief Constructs EventMediator with a raw pointer to an EventHub instance.
        /// \param hub Pointer to the EventHub instance.
        explicit EventMediator(EventHub* hub) : m_event_hub(hub) {};

        /// \brief Constructs EventMediator with a reference to an EventHub instance.
        /// \param hub Reference to the EventHub instance.
        explicit EventMediator(EventHub& hub) : m_event_hub(&hub) {};

        /// \brief Constructs EventMediator with a unique pointer to an EventHub instance.
        /// \param hub Unique pointer to the EventHub instance.
        explicit EventMediator(std::unique_ptr<EventHub>& hub) : m_event_hub(hub.get()) {};

        /// \brief Destructor for EventMediator.
        virtual ~EventMediator() = default;

        /// \brief Subscribe to an event type with a custom callback function.
        /// \tparam EventType Type of the event to subscribe to.
        /// \param callback Callback function to invoke when the specified event is published.
        template <typename EventType>
        void subscribe(std::function<void(std::shared_ptr<EventType>)> callback) {
            m_event_hub->subscribe<EventType>(callback);
        }

        /// \brief Subscribe to an event type using an EventListener-derived object.
        /// \tparam EventType Type of the event the listener subscribes to.
        /// \param listener EventListener object to receive notifications for the specified event type.
        template <typename EventType>
        void subscribe(EventListener* listener) {
            m_event_hub->subscribe<EventType>(listener);
        }

        /// \brief Notify all subscribers of an event using a shared pointer.
        /// \param event Shared pointer to the event to be published.
        void notify(std::shared_ptr<Event> event) const {
            m_event_hub->notify(std::move(event));
        }

        /// \brief Notify all subscribers of an event using a raw pointer.
        /// \param event Raw pointer to the event to be published.
        void notify(const Event* const event) const {
            m_event_hub->notify(event);
        }

        /// \brief Notify all subscribers of an event by reference.
        /// \param event Reference to the event to be published.
        /// \details Internally calls the notify method with a raw pointer to the event.
        void notify(const Event& event) const {
            m_event_hub->notify(event);
        }

    private:
        EventHub* m_event_hub; ///< Pointer to the EventHub instance for managing event subscriptions and notifications.
    }; // EventMediator

}; // namespace optionx

#endif // _OPTIONX_PUBSUB_EVENT_MEDIATOR_HPP_INCLUDED
