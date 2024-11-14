#pragma once
#ifndef _OPTIONX_PUBSUB_EVENT_HUB_HPP_INCLUDED
#define _OPTIONX_PUBSUB_EVENT_HUB_HPP_INCLUDED

/// \file EventHub.hpp
/// \brief Contains the EventHub class for event-based communication between modules.

#include "EventListener.hpp"
#include <unordered_map>
#include <vector>
#include <memory>
#include <functional>
#include <typeindex>
#include <algorithm>

namespace optionx {

    /// \class EventHub
    /// \brief Manages subscriptions and notifications for event-based communication.
    class EventHub {
    public:
        using CallbackFunction = std::function<void(std::shared_ptr<Event>)>;
        using CallbackList = std::vector<CallbackFunction>;
        using ListenerList = std::vector<EventListener*>;

        /// \brief Subscribe to an event type with a custom callback function.
        /// \tparam EventType Type of the event to subscribe to.
        /// \param callback Callback function to invoke when the event is published.
        template <typename EventType>
        void subscribe(std::function<void(std::shared_ptr<EventType>)> callback) {
            static_assert(std::is_base_of<Event, EventType>::value, "EventType must be derived from Event");

            auto type = std::type_index(typeid(EventType));
            m_event_callbacks[type].push_back([callback](std::shared_ptr<Event> e) {
                callback(std::static_pointer_cast<EventType>(e));
            });
        }

        /// \brief Subscribe using an EventListener-derived object to handle event notifications.
        /// \tparam EventType Type of the event the listener subscribes to.
        /// \param listener EventListener object to receive event notifications.
        template <typename EventType>
        void subscribe(EventListener* listener) {
            static_assert(std::is_base_of<Event, EventType>::value, "EventType must be derived from Event");

            auto type = std::type_index(typeid(EventType));
            auto& listener_list = m_event_listeners[type];

            // Ensure the listener isn't already in the list
            if (std::find(listener_list.begin(), listener_list.end(), listener) == listener_list.end()) {
                listener_list.push_back(listener);
            }
        }

        /// \brief Notify all subscribers of an event by shared pointer.
        /// \param event Shared pointer to the event to notify subscribers of.
        void notify(std::shared_ptr<Event> event) const {
            auto type = std::type_index(typeid(*event));

            // Notify callbacks
            auto it = m_event_callbacks.find(type);
            if (it != m_event_callbacks.end()) {
                for (const auto& callback : it->second) {
                    callback(event);
                }
            }

            // Notify listeners
            auto it_listeners = m_event_listeners.find(type);
            if (it_listeners != m_event_listeners.end()) {
                for (auto& listener : it_listeners->second) {
                    listener->on_event(event);
                }
            }
        }

        /// \brief Notify all subscribers of an event using a raw pointer.
        /// \param event Raw pointer to the event to notify subscribers of.
        void notify(const Event* const event) const {
            auto type = std::type_index(typeid(*event));

            auto it = m_event_listeners.find(type);
            if (it != m_event_listeners.end()) {
                for (auto& listener : it->second) {
                    listener->on_event(event);
                }
            }
        }

        /// \brief Notify subscribers of an event by reference, internally uses the raw pointer method.
        /// \param event Reference to the event to notify subscribers of.
        void notify(const Event& event) const {
            notify(&event);
        }

    private:
        std::unordered_map<std::type_index, CallbackList> m_event_callbacks;    ///< Map of event callbacks by event type.
        std::unordered_map<std::type_index, ListenerList> m_event_listeners;    ///< Map of event listeners by event type.
    }; // EventHub

}; // namespace optionx

#endif // _OPTIONX_PUBSUB_EVENT_HUB_HPP_INCLUDED
