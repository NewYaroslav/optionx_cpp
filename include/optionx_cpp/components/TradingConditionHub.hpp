#pragma once
#ifndef OPTIONX_HEADER_COMPONENTS_TRADING_CONDITION_HUB_HPP_INCLUDED
#define OPTIONX_HEADER_COMPONENTS_TRADING_CONDITION_HUB_HPP_INCLUDED

/// \file TradingConditionHub.hpp
/// \brief Defines subscriber fan-out helpers for trading-condition updates.

namespace optionx::components {

    /// \class ITradingConditionSubscriber
    /// \brief Interface for consumers that receive broker trading-condition updates.
    class ITradingConditionSubscriber {
    public:
        /// \brief Virtual destructor.
        virtual ~ITradingConditionSubscriber() = default;

        /// \brief Handles a trading-condition update.
        /// \param update Trading-condition payload.
        virtual void on_trading_condition(const TradingConditionUpdate& update) = 0;
    };

    /// \class TradingConditionHub
    /// \brief Routes trading-condition updates to multiple subscribers.
    class TradingConditionHub {
    public:
        /// \brief Constructs a trading-condition hub.
        /// \param replay_cached_updates_to_new_subscribers Whether late
        ///        subscribers receive cached latest updates immediately.
        explicit TradingConditionHub(
                bool replay_cached_updates_to_new_subscribers = true)
            : m_replay_cached_updates_to_new_subscribers(
                  replay_cached_updates_to_new_subscribers) {}

        TradingConditionHub(const TradingConditionHub&) = delete;
        TradingConditionHub& operator=(const TradingConditionHub&) = delete;
        TradingConditionHub(TradingConditionHub&&) = delete;
        TradingConditionHub& operator=(TradingConditionHub&&) = delete;

        /// \brief Adds a shared subscriber.
        /// \param subscriber Subscriber object; ignored when null.
        void add_subscriber(
                std::shared_ptr<ITradingConditionSubscriber> subscriber) {
            if (!subscriber) return;
            add_weak_subscriber(
                std::weak_ptr<ITradingConditionSubscriber>(subscriber));
        }

        /// \brief Adds a weak subscriber.
        /// \param subscriber Weak subscriber reference; ignored when expired.
        void add_weak_subscriber(
                std::weak_ptr<ITradingConditionSubscriber> subscriber) {
            const auto locked = subscriber.lock();
            if (!locked) return;

            std::vector<TradingConditionUpdate> replay_updates;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                prune_expired_no_lock();
                if (!contains_subscriber_no_lock(locked.get())) {
                    m_subscribers.push_back(std::move(subscriber));
                }
                if (m_replay_cached_updates_to_new_subscribers) {
                    replay_updates = m_cached_updates;
                }
            }

            for (const auto& update : replay_updates) {
                locked->on_trading_condition(update);
            }
        }

        /// \brief Removes a subscriber by object address.
        /// \param subscriber Subscriber object address.
        void remove_subscriber(const ITradingConditionSubscriber* subscriber) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_subscribers.erase(
                std::remove_if(
                    m_subscribers.begin(),
                    m_subscribers.end(),
                    [subscriber](const std::weak_ptr<ITradingConditionSubscriber>& item) {
                        const auto locked = item.lock();
                        return !locked || locked.get() == subscriber;
                    }),
                m_subscribers.end());
        }

        /// \brief Removes all subscribers.
        void clear_subscribers() {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_subscribers.clear();
        }

        /// \brief Returns the number of live subscribers.
        /// \return Count of non-expired subscribers.
        std::size_t subscriber_count() const {
            std::lock_guard<std::mutex> lock(m_mutex);
            std::size_t count = 0;
            for (const auto& subscriber : m_subscribers) {
                if (!subscriber.expired()) {
                    ++count;
                }
            }
            return count;
        }

        /// \brief Binds the hub to a trading-condition callback.
        /// \param callback Callback to replace with this hub dispatcher.
        void bind_to(trading_condition_callback_t& callback) {
            std::lock_guard<std::mutex> lock(m_mutex);
            callback = [this](const TradingConditionUpdate& update) {
                publish(update);
            };
            m_bound_callback = &callback;
        }

        /// \brief Unbinds the hub from a callback if it is the current binding.
        /// \param callback Callback to unbind.
        void unbind_from(trading_condition_callback_t& callback) {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_bound_callback == &callback) {
                callback = {};
                m_bound_callback = nullptr;
            }
        }

        /// \brief Publishes a trading-condition update.
        /// \param update Trading-condition update payload.
        void publish(TradingConditionUpdate update) {
            std::vector<std::shared_ptr<ITradingConditionSubscriber>> subscribers;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                upsert_cached_update_no_lock(update);
                prune_expired_no_lock();
                subscribers.reserve(m_subscribers.size());
                for (const auto& subscriber : m_subscribers) {
                    if (auto locked = subscriber.lock()) {
                        subscribers.push_back(std::move(locked));
                    }
                }
            }

            for (const auto& subscriber : subscribers) {
                subscriber->on_trading_condition(update);
            }
        }

        /// \brief Returns cached latest updates by condition key.
        /// \return Copy of cached updates.
        std::vector<TradingConditionUpdate> cached_updates() const {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_cached_updates;
        }

        /// \brief Clears cached updates.
        void clear_cached_updates() {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_cached_updates.clear();
        }

    private:
        mutable std::mutex m_mutex;
        std::vector<std::weak_ptr<ITradingConditionSubscriber>> m_subscribers;
        std::vector<TradingConditionUpdate> m_cached_updates;
        trading_condition_callback_t* m_bound_callback = nullptr;
        bool m_replay_cached_updates_to_new_subscribers = true;

        /// \brief Checks whether two updates describe the same condition scope.
        static bool same_condition_key(
                const TradingConditionUpdate& left,
                const TradingConditionUpdate& right) {
            return left.symbol == right.symbol &&
                   left.platform_type == right.platform_type &&
                   left.account_type == right.account_type &&
                   left.currency == right.currency &&
                   left.option_type == right.option_type;
        }

        /// \brief Inserts or replaces a cached update by condition key.
        void upsert_cached_update_no_lock(const TradingConditionUpdate& update) {
            const auto it = std::find_if(
                m_cached_updates.begin(),
                m_cached_updates.end(),
                [&update](const TradingConditionUpdate& cached) {
                    return same_condition_key(cached, update);
                });

            if (it == m_cached_updates.end()) {
                m_cached_updates.push_back(update);
                return;
            }

            *it = update;
        }

        /// \brief Removes expired weak subscribers.
        void prune_expired_no_lock() {
            m_subscribers.erase(
                std::remove_if(
                    m_subscribers.begin(),
                    m_subscribers.end(),
                    [](const std::weak_ptr<ITradingConditionSubscriber>& item) {
                        return item.expired();
                    }),
                m_subscribers.end());
        }

        /// \brief Checks whether a subscriber is already registered.
        bool contains_subscriber_no_lock(
                const ITradingConditionSubscriber* subscriber) const {
            return std::any_of(
                m_subscribers.begin(),
                m_subscribers.end(),
                [subscriber](const std::weak_ptr<ITradingConditionSubscriber>& item) {
                    const auto locked = item.lock();
                    return locked && locked.get() == subscriber;
                });
        }
    };

} // namespace optionx::components

#endif // OPTIONX_HEADER_COMPONENTS_TRADING_CONDITION_HUB_HPP_INCLUDED
