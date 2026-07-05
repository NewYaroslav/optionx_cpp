#pragma once
#ifndef OPTIONX_HEADER_COMPONENTS_ACCOUNT_INFO_HUB_HPP_INCLUDED
#define OPTIONX_HEADER_COMPONENTS_ACCOUNT_INFO_HUB_HPP_INCLUDED

/// \file AccountInfoHub.hpp
/// \brief Defines subscriber fan-out helpers for account information updates.

namespace optionx::components {

    /// \class AccountInfoHub
    /// \brief Routes a single account-info callback to multiple subscribers.
    ///
    /// The hub is a lightweight adapter for platform-level `on_account_info()`.
    /// It does not own platform lifecycle or account storage; it only fans out
    /// immutable `AccountInfoUpdate` payloads and optionally replays the latest
    /// update to late subscribers. Subscribers are stored as weak references,
    /// so caller code must keep subscriber objects alive while they should
    /// receive callbacks.
    class AccountInfoHub {
    public:
        /// \brief Constructs an account-info hub.
        /// \param replay_last_update_to_new_subscribers Whether late subscribers
        ///        should receive the latest cached account update immediately.
        explicit AccountInfoHub(bool replay_last_update_to_new_subscribers = true)
            : m_replay_last_update_to_new_subscribers(
                  replay_last_update_to_new_subscribers) {}

        AccountInfoHub(const AccountInfoHub&) = delete;
        AccountInfoHub& operator=(const AccountInfoHub&) = delete;
        AccountInfoHub(AccountInfoHub&&) = delete;
        AccountInfoHub& operator=(AccountInfoHub&&) = delete;

        /// \brief Adds a shared subscriber.
        /// \details The hub stores only a weak reference; the caller must keep
        ///          the shared subscriber alive while it should receive account
        ///          callbacks.
        /// \param subscriber Subscriber object; ignored when null.
        void add_subscriber(std::shared_ptr<IAccountInfoSubscriber> subscriber) {
            if (!subscriber) return;
            add_weak_subscriber(std::weak_ptr<IAccountInfoSubscriber>(subscriber));
        }

        /// \brief Adds a weak subscriber.
        /// \param subscriber Weak subscriber reference; ignored when expired.
        void add_weak_subscriber(std::weak_ptr<IAccountInfoSubscriber> subscriber) {
            const auto locked = subscriber.lock();
            if (!locked) return;

            std::optional<AccountInfoUpdate> replay_update;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                prune_expired_no_lock();
                if (!contains_subscriber_no_lock(locked.get())) {
                    m_subscribers.push_back(std::move(subscriber));
                }
                if (m_replay_last_update_to_new_subscribers && m_last_update) {
                    replay_update = m_last_update;
                }
            }

            if (replay_update) {
                locked->on_account_info(*replay_update);
            }
        }

        /// \brief Removes a subscriber by object address.
        /// \param subscriber Subscriber object address.
        void remove_subscriber(const IAccountInfoSubscriber* subscriber) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_subscribers.erase(
                std::remove_if(
                    m_subscribers.begin(),
                    m_subscribers.end(),
                    [subscriber](const std::weak_ptr<IAccountInfoSubscriber>& item) {
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

        /// \brief Binds the hub to a platform/account callback.
        /// \details Replaces the callback with a dispatcher that calls
        ///          `publish()`. Call `unbind_from()` before destroying the hub
        ///          if the callback owner may outlive it.
        /// \param callback Platform account-info callback.
        void bind_to(account_info_callback_t& callback) {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_bound_callback && m_bound_callback != &callback) {
                *m_bound_callback = {};
            }
            callback = [this](const AccountInfoUpdate& update) {
                publish(update);
            };
            m_bound_callback = &callback;
        }

        /// \brief Unbinds the hub from a callback if it is the current binding.
        /// \param callback Platform account-info callback.
        void unbind_from(account_info_callback_t& callback) {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_bound_callback == &callback) {
                callback = {};
                m_bound_callback = nullptr;
            }
        }

        /// \brief Publishes an account update to all live subscribers.
        /// \param update Account update payload.
        void publish(AccountInfoUpdate update) {
            std::vector<std::shared_ptr<IAccountInfoSubscriber>> subscribers;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_last_update = update;
                prune_expired_no_lock();
                subscribers.reserve(m_subscribers.size());
                for (const auto& subscriber : m_subscribers) {
                    if (auto locked = subscriber.lock()) {
                        subscribers.push_back(std::move(locked));
                    }
                }
            }

            for (const auto& subscriber : subscribers) {
                subscriber->on_account_info(update);
            }
        }

        /// \brief Returns the latest cached update when one exists.
        /// \return Last account update, or `std::nullopt`.
        std::optional<AccountInfoUpdate> last_update() const {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_last_update;
        }

        /// \brief Clears the cached latest update.
        void clear_last_update() {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_last_update.reset();
        }

    private:
        mutable std::mutex m_mutex;
        std::vector<std::weak_ptr<IAccountInfoSubscriber>> m_subscribers;
        std::optional<AccountInfoUpdate> m_last_update;
        account_info_callback_t* m_bound_callback = nullptr;
        bool m_replay_last_update_to_new_subscribers = true;

        /// \brief Removes expired weak subscribers.
        void prune_expired_no_lock() {
            m_subscribers.erase(
                std::remove_if(
                    m_subscribers.begin(),
                    m_subscribers.end(),
                    [](const std::weak_ptr<IAccountInfoSubscriber>& item) {
                        return item.expired();
                    }),
                m_subscribers.end());
        }

        /// \brief Checks whether a subscriber is already registered.
        /// \param subscriber Subscriber object address.
        /// \return True if the subscriber is already present.
        bool contains_subscriber_no_lock(
                const IAccountInfoSubscriber* subscriber) const {
            return std::any_of(
                m_subscribers.begin(),
                m_subscribers.end(),
                [subscriber](const std::weak_ptr<IAccountInfoSubscriber>& item) {
                    const auto locked = item.lock();
                    return locked && locked.get() == subscriber;
                });
        }
    };

} // namespace optionx::components

#endif // OPTIONX_HEADER_COMPONENTS_ACCOUNT_INFO_HUB_HPP_INCLUDED
