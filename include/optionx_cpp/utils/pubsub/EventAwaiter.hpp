#pragma once
#ifndef OPTIONX_HEADER_UTILS_PUBSUB_EVENT_AWAITER_HPP_INCLUDED
#define OPTIONX_HEADER_UTILS_PUBSUB_EVENT_AWAITER_HPP_INCLUDED

/// \file EventAwaiter.hpp
/// \brief RAII helper to wait for a single event that matches a predicate and then auto-unsubscribe.

#include <memory>
#include <functional>
#include <atomic>
#include <mutex>

namespace optionx::utils {
    
    /// \brief Minimal awaiter interface for mediator bookkeeping.
    struct IAwaiter {
        virtual void cancel() noexcept = 0;
        virtual bool is_active() const noexcept = 0;
        virtual ~IAwaiter() = default;
    };

    /// \class EventAwaiter
    /// \brief Subscribes to a specific Event type and invokes a callback once a predicate matches.
    ///
    /// Design goals:
    /// - **Single-shot** by default: after the first match, the awaiter unsubscribes and self-releases.
    /// - **RAII**: unsubscribe in destructor to avoid leaks.
    /// - **Thread-safe** w.r.t. EventBus internal locks (does not call user code under internal mutexes).
    /// - **Same-thread dispatch**: built to run on the EventBus processing thread (no blocking).
    ///
    /// Typical usage:
    /// \code
    /// auto aw = EventAwaiter<MyEvent>::create(bus,
    ///     [cid](const MyEvent& e){ return e.correlation_id == cid; },
    ///     [this](const MyEvent& e){ /* handle */ });
    /// // keep `aw` if you may cancel; otherwise you can ignore — it self-destroys after firing
    /// \endcode
    template <typename EventType>
    class EventAwaiter : public EventListener, 
                         public IAwaiter, 
                         public std::enable_shared_from_this<EventAwaiter<EventType>> {
        static_assert(std::is_base_of_v<Event, EventType>, "EventType must derive from utils::Event");
    public:
        using predicate_t = std::function<bool(const EventType&)>;
        using callback_t  = std::function<void(const EventType&)>;

        /// \brief Factory: constructs, subscribes, and returns shared instance.
        /// \param bus Event bus to subscribe on.
        /// \param predicate Match predicate (default: match all).
        /// \param on_match Callback executed once when predicate returns true.
        /// \param single_shot Unsubscribe after first match (default true).
        [[nodiscard]] static std::shared_ptr<EventAwaiter> create(
                EventBus& bus,
                predicate_t predicate,
                callback_t on_match,
                bool single_shot = true) {
            auto self = std::shared_ptr<EventAwaiter>(new EventAwaiter(
                bus, std::move(predicate), std::move(on_match), single_shot));
            self->subscribe_internal();
            return self;
        }
        
        /// \brief 
        bool is_active() const noexcept override {
            return !m_cancelled.load(std::memory_order_relaxed);
        }

        /// \brief Cancels the awaiter (unsubscribe). Safe to call multiple times.
        void cancel() noexcept {
            bool expected = false;
            if (!m_cancelled.compare_exchange_strong(expected, true)) return;
            m_bus.template unsubscribe<EventType>(this);
            m_retain_self.reset();
        }

        /// \brief Destructor unsubscribes if still subscribed.
        ~EventAwaiter() override { cancel(); }

        // EventListener (not used directly; we subscribe via typed callback API)
        void on_event(const Event* const) override {}

    private:
        EventAwaiter(EventBus& bus, predicate_t predicate, callback_t on_match, bool single_shot)
            : m_bus(bus),
              m_predicate(std::move(predicate)),
              m_on_match(std::move(on_match)),
              m_single_shot(single_shot) {}

        void subscribe_internal() {
            // Capture weak_ptr to control lifetime until the first match.
             if (m_single_shot) m_retain_self = this->shared_from_this();
            auto weak_self = this->weak_from_this();
            m_bus.subscribe<EventType>(this, [weak_self](const EventType& ev){
                if (auto self = weak_self.lock()) {
                    self->handle_event(ev);
                }
            });
        }

        void handle_event(const EventType& ev) {
            // Fast-cancel check
            if (m_cancelled.load(std::memory_order_relaxed)) return;
            auto hold = this->shared_from_this();

            // Predicate guard
            bool matched = true;
            if (m_predicate) matched = m_predicate(ev);
            if (!matched) return;

            // Single-shot semantics: cancel BEFORE calling user code to avoid reentrancy issues
            if (m_single_shot) cancel();

            // Invoke user callback (outside of EventBus locks)
            if (m_on_match) m_on_match(ev);
        }

    private:
        EventBus&   m_bus;
        predicate_t m_predicate;
        callback_t  m_on_match;
        const bool  m_single_shot{true};
        std::atomic<bool> m_cancelled{false};
        std::shared_ptr<EventAwaiter> m_retain_self; 
    };

} // namespace optionx::utils

#endif // OPTIONX_HEADER_UTILS_PUBSUB_EVENT_AWAITER_HPP_INCLUDED
