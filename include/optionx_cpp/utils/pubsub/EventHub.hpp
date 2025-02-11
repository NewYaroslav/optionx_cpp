#pragma once
#ifndef _OPTIONX_UTILS_PUBSUB_EVENT_HUB_HPP_INCLUDED
#define _OPTIONX_UTILS_PUBSUB_EVENT_HUB_HPP_INCLUDED

/// \file EventHub.hpp
/// \brief Contains the EventHub class for event-based communication between modules.

#include <unordered_map>
#include <vector>
#include <queue>
#include <mutex>
#include <functional>
#include <typeindex>
#include <algorithm>

namespace optionx::utils {

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

        /// \brief Adds an event to the queue for asynchronous processing.
        /// \param event Unique pointer to the event (ensures exclusive ownership and prevents race conditions).
        void notify_async(std::unique_ptr<Event> event) {
            std::lock_guard<std::mutex> lock(m_queue_mutex);
            m_event_queue.push(std::move(event));
        }

        /// \brief Processes queued events in a thread-safe manner.
        /// \details This method should be called from the main thread to ensure event processing
        ///          occurs in a controlled execution context.
        void process() {
            std::queue<std::unique_ptr<Event>> local_queue;
            std::unique_lock<std::mutex> lock(m_queue_mutex);
            std::swap(local_queue, m_event_queue);
            lock.unlock();

            while (!local_queue.empty()) {
                notify(local_queue.front().get());
                local_queue.pop();
            }
        }

    private:
        std::unordered_map<std::type_index, CallbackList> m_event_callbacks;    ///< Map of event callbacks by event type.
        std::unordered_map<std::type_index, ListenerList> m_event_listeners;    ///< Map of event listeners by event type.

        mutable std::mutex                 m_queue_mutex;   ///< Mutex to protect access to the event queue.
        std::queue<std::unique_ptr<Event>> m_event_queue;   ///< Queue of events for asynchronous processing.
    }; // EventHub

}; // namespace optionx::utils

#endif // _OPTIONX_UTILS_PUBSUB_EVENT_HUB_HPP_INCLUDED
