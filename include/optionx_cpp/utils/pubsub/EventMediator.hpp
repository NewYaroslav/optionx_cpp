#pragma once
#ifndef _OPTIONX_UTILS_PUBSUB_EVENT_MEDIATOR_HPP_INCLUDED
#define _OPTIONX_UTILS_PUBSUB_EVENT_MEDIATOR_HPP_INCLUDED

/// \file EventMediator.hpp
/// \brief Defines the EventMediator class for managing subscriptions and notifications through EventBus.

namespace optionx::utils {

    /// \class EventMediator
    /// \brief Facilitates event subscriptions and notifications through an associated EventBus.
    ///
    /// Provides a unified interface for components to subscribe to, notify, and asynchronously post events.
    class EventMediator : public EventListener {
    public:
        /// \brief Constructs EventMediator with a raw pointer to an EventBus instance.
        /// \param bus Pointer to the EventBus instance.
        /// \details A null bus is accepted for defensive construction; public
        /// methods remain no-ops for the mediator lifetime.
        explicit EventMediator(EventBus* bus) : m_event_bus(bus) {}

        /// \brief Constructs EventMediator with a reference to an EventBus instance.
        /// \param bus Reference to the EventBus instance.
        explicit EventMediator(EventBus& bus) : m_event_bus(&bus) {}

        /// \brief Constructs EventMediator with a unique pointer to an EventBus instance.
        /// \param bus Unique pointer to the EventBus instance.
        /// \details If the unique pointer is empty, public methods remain no-ops
        /// for the mediator lifetime.
        explicit EventMediator(std::unique_ptr<EventBus>& bus) : m_event_bus(bus.get()) {}

        ~EventMediator() noexcept override {
            cancel_all_awaiters();
            unsubscribe_all();
        }
        
        /// \brief Subscribes to an event type with a custom callback function taking a concrete event reference.
        /// \tparam EventType Type of the event to subscribe to.
        /// \param callback Callback function accepting a const reference to the event.
        template <typename EventType>
        void subscribe(std::function<void(const EventType&)> callback) {
            if (!has_event_bus()) return;
            m_event_bus->subscribe<EventType>(this, std::move(callback));
        }

        /// \brief Subscribes to an event type with a generic callback function taking a base event pointer.
        /// \tparam EventType Type of the event to subscribe to.
        /// \param callback Callback function accepting a const pointer to the base event.
        template <typename EventType>
        void subscribe(std::function<void(const Event* const)> callback) {
            if (!has_event_bus()) return;
            m_event_bus->subscribe<EventType>(this, std::move(callback));
        }

        /// \brief Subscribes this object as a listener to the specified event type.
        /// \tparam EventType Type of the event to subscribe to.
        template <typename EventType>
        void subscribe() {
            if (!has_event_bus()) return;
            m_event_bus->subscribe<EventType>(this);
        }
        
        /// \brief Unsubscribes this mediator from a specific event type.
        /// \tparam EventType Type of the event to unsubscribe from.
        template <typename EventType>
        void unsubscribe() {
            if (!has_event_bus()) return;
            m_event_bus->unsubscribe<EventType>(this);
        }
        
        /// \brief Unsubscribes this mediator from all event types.
        void unsubscribe_all() {
            if (!has_event_bus()) return;
            m_event_bus->unsubscribe_all(this);
        }

        /// \brief Notifies all subscribers of an event (shared pointer dereferenced).
        /// \param event Shared pointer to the event.
        void notify(const std::shared_ptr<Event>& event) const {
            if (!has_event_bus()) return;
            m_event_bus->notify(event.get());
        }
        
        /// \brief Notifies all subscribers of an event (unique pointer dereferenced).
        /// \param event Unique pointer to the event.
        void notify(const std::unique_ptr<Event>& event) const {
            if (!has_event_bus()) return;
            m_event_bus->notify(event.get());
        }

        /// \brief Notifies all subscribers of an event (raw pointer).
        /// \param event Raw pointer to the event.
        void notify(const Event* const event) const {
            if (!has_event_bus()) return;
            m_event_bus->notify(event);
        }

        /// \brief Notifies all subscribers of an event (reference).
        /// \param event Reference to the event.
        void notify(const Event& event) const {
            if (!has_event_bus()) return;
            m_event_bus->notify(event);
        }

        /// \brief Queues an event for asynchronous processing.
        /// \param event Unique pointer to the event.
        void notify_async(std::unique_ptr<Event> event) {
            if (!has_event_bus()) return;
            m_event_bus->notify_async(std::move(event));
        }

        /// \brief Await a single event occurrence that matches a predicate, then auto-unsubscribe.
        /// \tparam EventType Concrete event type to await.
        /// \tparam Pred Predicate type: bool(const EventType&).
        /// \tparam Cb Callback type: void(const EventType&).
        /// \param pred Match predicate (executed on the event loop thread).
        /// \param cb   Callback executed once when predicate returns true.
        template <typename EventType, typename Pred, typename Cb>
        void await_once(Pred&& pred, Cb&& cb) {
            if (!has_event_bus()) return;
            prune_dead_awaiters();

            using AW = EventAwaiter<EventType>;
            // self-owned awaiter: живёт сам, пока активен
            auto aw = AW::create(
                *m_event_bus,
                std::forward<Pred>(pred),
                std::forward<Cb>(cb),
                /*single_shot=*/true
            );

            std::lock_guard<std::mutex> lk(m_mutex);
            m_awaiters.emplace_back(aw);
        }

        /// \brief Await the first event of EventType (no predicate).
        /// \tparam EventType Concrete event type to await.
        /// \tparam Cb Callback type: void(const EventType&).
        /// \param cb Callback executed on the first delivered EventType.
        template <typename EventType, typename Cb>
        void await_once(Cb&& cb) {
            await_once<EventType>(
                [](const EventType&) { return true; },
                std::forward<Cb>(cb)
            );
        }

    private:
        EventBus* m_event_bus; ///< Associated EventBus instance.
        std::mutex m_mutex;
        std::vector<std::weak_ptr<IAwaiter>> m_awaiters; ///< Weak list of active awaiters.

        bool has_event_bus() const noexcept {
            return m_event_bus != nullptr;
        }
    
         void prune_dead_awaiters() {
            std::lock_guard<std::mutex> lk(m_mutex);
            auto& v = m_awaiters;
            v.erase(std::remove_if(v.begin(), v.end(),
                [](const std::weak_ptr<IAwaiter>& w){
                    if (w.expired()) return true;
                    if (auto sp = w.lock()) return !sp->is_active();
                    return true;
                }), v.end());
        }

        void cancel_all_awaiters() {
            std::vector<std::shared_ptr<IAwaiter>> live;
 
            std::unique_lock<std::mutex> lock(m_mutex);
            live.reserve(m_awaiters.size());
            for (auto& w : m_awaiters) {
                if (auto sp = w.lock()) live.emplace_back(std::move(sp));
            }
            m_awaiters.clear();
            lock.unlock();

            for (auto& sp : live) sp->cancel();
        }
    };

}; // namespace optionx::utils

#endif // _OPTIONX_UTILS_PUBSUB_EVENT_MEDIATOR_HPP_INCLUDED
