#pragma once
#ifndef _OPTIONX_PLATFORMS_INTRADERBAR_MARKET_DATA_SUBSCRIPTION_MANAGER_HPP_INCLUDED
#define _OPTIONX_PLATFORMS_INTRADERBAR_MARKET_DATA_SUBSCRIPTION_MANAGER_HPP_INCLUDED

/// \file MarketDataSubscriptionManager.hpp
/// \brief Defines Intrade Bar live market-data subscription routing.

namespace optionx::platforms::intrade_bar {

    /// \class MarketDataSubscriptionManager
    /// \brief Routes Intrade Bar price update events to market-data subscription callbacks.
    class MarketDataSubscriptionManager final : public components::BaseComponent {
    public:
        using bars_callback_t = market_data::BaseMarketDataProvider::bars_callback_t;
        using ticks_callback_t = market_data::BaseMarketDataProvider::ticks_callback_t;
        using subscription_callback_t = market_data::BaseMarketDataProvider::subscription_callback_t;

        /// \brief Constructs the subscription manager.
        /// \param platform Owning platform facade.
        /// \param ticks_callback Tick stream data callback owned by the platform.
        /// \param bars_callback Bar stream data callback owned by the platform.
        MarketDataSubscriptionManager(
                BaseTradingPlatform& platform,
                market_data::ProviderInstanceId provider_id,
                ticks_callback_t& ticks_callback,
                bars_callback_t& bars_callback,
                FxPriceWebSocketManager* fx_websocket_source = nullptr)
                : BaseComponent(platform.event_bus()),
                  m_provider_id(provider_id),
                  m_ticks_callback(ticks_callback),
                  m_bars_callback(bars_callback),
                  m_fx_websocket_source(fx_websocket_source) {
            subscribe<events::PriceUpdateEvent>();
            platform.register_component(this);
        }

        /// \brief Requests a live tick subscription.
        bool subscribe_ticks(
                market_data::TickSubscriptionRequest request,
                subscription_callback_t callback);

        /// \brief Requests a live bar subscription.
        bool subscribe_bars(
                market_data::BarSubscriptionRequest request,
                subscription_callback_t callback);

        /// \brief Stops a live market-data subscription.
        bool unsubscribe(
                market_data::MarketDataSubscriptionHandle subscription,
                subscription_callback_t callback);

        /// \brief Routes price events to active tick subscriptions.
        void on_event(const utils::Event* const event) override;

        /// \brief Clears all runtime subscriptions.
        void shutdown() override;

    private:
        market_data::ProviderInstanceId m_provider_id; ///< Owning provider instance ID.
        ticks_callback_t& m_ticks_callback; ///< Platform tick data callback.
        bars_callback_t&  m_bars_callback;  ///< Platform bar data callback.
        FxPriceWebSocketManager* m_fx_websocket_source = nullptr; ///< Optional FX websocket source.
        std::unordered_map<
            market_data::SubscriptionId,
            market_data::MarketDataSubscriptionHandle> m_tick_subscriptions; ///< Active tick subscriptions.
        market_data::SubscriptionId m_next_subscription_id = 1; ///< Next runtime handle ID.
        std::mutex m_mutex; ///< Protects subscription handles.

        /// \brief Returns the next non-zero subscription ID.
        market_data::SubscriptionId next_subscription_id();

        /// \brief Returns true when any active tick subscription matches the symbol.
        bool has_tick_subscription_no_lock(const std::string& symbol) const;

        /// \brief Returns true when a subscription should activate the FX websocket source.
        static bool uses_fx_websocket_source(
                const market_data::MarketDataSubscriptionHandle& subscription);

        /// \brief Delivers a subscription operation result when a callback was supplied.
        static void dispatch_subscription_result(
                subscription_callback_t callback,
                market_data::MarketDataSubscriptionResult result);

        /// \brief Handles incoming price update events.
        void handle_event(const events::PriceUpdateEvent& event);
    };

    inline bool MarketDataSubscriptionManager::subscribe_ticks(
            market_data::TickSubscriptionRequest request,
            subscription_callback_t callback) {
        request.symbol = normalize_symbol_name(std::move(request.symbol));
        if (!request.valid()) {
            dispatch_subscription_result(
                std::move(callback),
                market_data::MarketDataSubscriptionResult::failed(
                    std::move(request),
                    market_data::MarketDataSubscriptionStatus::INVALID_REQUEST,
                    "Invalid Intrade Bar tick subscription request."));
            return false;
        }

        market_data::MarketDataSubscriptionHandle handle;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            handle = market_data::MarketDataSubscriptionHandle::from_tick_request(
                m_provider_id,
                next_subscription_id(),
                request);
            m_tick_subscriptions[handle.id] = handle;
        }

        if (m_fx_websocket_source && uses_fx_websocket_source(handle)) {
            m_fx_websocket_source->add_symbol_subscription(handle.symbol);
        }

        dispatch_subscription_result(
            std::move(callback),
            market_data::MarketDataSubscriptionResult::subscribed(handle));
        return true;
    }

    inline bool MarketDataSubscriptionManager::subscribe_bars(
            market_data::BarSubscriptionRequest request,
            subscription_callback_t callback) {
        request.symbol = normalize_symbol_name(std::move(request.symbol));
        if (!request.valid()) {
            dispatch_subscription_result(
                std::move(callback),
                market_data::MarketDataSubscriptionResult::failed(
                    std::move(request),
                    market_data::MarketDataSubscriptionStatus::INVALID_REQUEST,
                    "Invalid Intrade Bar bar subscription request."));
            return false;
        }

        dispatch_subscription_result(
            std::move(callback),
            market_data::MarketDataSubscriptionResult::failed(
                std::move(request),
                market_data::MarketDataSubscriptionStatus::UNSUPPORTED,
                "Intrade Bar bar subscriptions require a tick-to-bar aggregator and are not supported yet."));
        return false;
    }

    inline bool MarketDataSubscriptionManager::unsubscribe(
            market_data::MarketDataSubscriptionHandle subscription,
            subscription_callback_t callback) {
        if (!subscription.valid()) {
            dispatch_subscription_result(
                std::move(callback),
                market_data::MarketDataSubscriptionResult::failed(
                    std::move(subscription),
                    market_data::MarketDataSubscriptionStatus::INVALID_REQUEST,
                    "Invalid Intrade Bar market-data subscription handle."));
            return false;
        }
        if (subscription.provider_id != m_provider_id) {
            dispatch_subscription_result(
                std::move(callback),
                market_data::MarketDataSubscriptionResult::failed(
                    std::move(subscription),
                    market_data::MarketDataSubscriptionStatus::WRONG_PROVIDER,
                    "Intrade Bar market-data subscription handle belongs to another provider."));
            return false;
        }

        bool removed = false;
        market_data::MarketDataSubscriptionHandle removed_subscription;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_tick_subscriptions.find(subscription.id);
            if (it != m_tick_subscriptions.end()) {
                removed_subscription = it->second;
                m_tick_subscriptions.erase(it);
                removed = true;
            }
        }

        if (!removed) {
            dispatch_subscription_result(
                std::move(callback),
                market_data::MarketDataSubscriptionResult::failed(
                    std::move(subscription),
                    market_data::MarketDataSubscriptionStatus::FAILED,
                    "Intrade Bar market-data subscription is not active."));
            return false;
        }

        if (m_fx_websocket_source && uses_fx_websocket_source(removed_subscription)) {
            m_fx_websocket_source->remove_symbol_subscription(removed_subscription.symbol);
        }

        dispatch_subscription_result(
            std::move(callback),
            market_data::MarketDataSubscriptionResult::unsubscribed(std::move(subscription)));
        return true;
    }

    inline void MarketDataSubscriptionManager::on_event(const utils::Event* const event) {
        if (const auto* msg = dynamic_cast<const events::PriceUpdateEvent*>(event)) {
            handle_event(*msg);
        }
    }

    inline void MarketDataSubscriptionManager::shutdown() {
        std::vector<market_data::MarketDataSubscriptionHandle> subscriptions;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            subscriptions.reserve(m_tick_subscriptions.size());
            for (const auto& [id, subscription] : m_tick_subscriptions) {
                (void)id;
                subscriptions.push_back(subscription);
            }
        }

        if (m_fx_websocket_source) {
            for (const auto& subscription : subscriptions) {
                if (uses_fx_websocket_source(subscription)) {
                    m_fx_websocket_source->remove_symbol_subscription(subscription.symbol);
                }
            }
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        m_tick_subscriptions.clear();
    }

    inline market_data::SubscriptionId
    MarketDataSubscriptionManager::next_subscription_id() {
        const auto id = m_next_subscription_id;
        ++m_next_subscription_id;
        if (m_next_subscription_id == market_data::kInvalidSubscriptionId) {
            m_next_subscription_id = 1;
        }
        return id;
    }

    inline bool MarketDataSubscriptionManager::has_tick_subscription_no_lock(
            const std::string& symbol) const {
        for (const auto& [id, subscription] : m_tick_subscriptions) {
            (void)id;
            if (subscription.symbol == symbol) {
                return true;
            }
        }
        return false;
    }

    inline bool MarketDataSubscriptionManager::uses_fx_websocket_source(
            const market_data::MarketDataSubscriptionHandle& subscription) {
        if (subscription.stream_type != market_data::MarketDataStreamType::TICKS) {
            return false;
        }
        if (subscription.transport == market_data::MarketDataTransport::POLLING) {
            return false;
        }
        return !is_btc_symbol(subscription.symbol);
    }

    inline void MarketDataSubscriptionManager::dispatch_subscription_result(
            subscription_callback_t callback,
            market_data::MarketDataSubscriptionResult result) {
        if (callback) {
            callback(std::move(result));
        }
    }

    inline void MarketDataSubscriptionManager::handle_event(
            const events::PriceUpdateEvent& event) {
        std::vector<SingleTick> ticks;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_tick_subscriptions.empty()) return;

            for (const auto& tick : event.get_ticks()) {
                if (has_tick_subscription_no_lock(normalize_symbol_name(tick.symbol))) {
                    ticks.push_back(tick);
                }
            }
        }

        if (!ticks.empty() && m_ticks_callback) {
            m_ticks_callback(ticks);
        }
    }

} // namespace optionx::platforms::intrade_bar

#endif // _OPTIONX_PLATFORMS_INTRADERBAR_MARKET_DATA_SUBSCRIPTION_MANAGER_HPP_INCLUDED
