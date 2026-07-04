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
        using subscription_batch_callback_t = market_data::BaseMarketDataProvider::subscription_batch_callback_t;

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

        /// \brief Applies a batch of live subscription changes atomically.
        bool apply_subscriptions(
                market_data::MarketDataSubscriptionBatch batch,
                subscription_batch_callback_t callback);

        /// \brief Stops all active market-data subscriptions.
        bool unsubscribe_all(subscription_batch_callback_t callback);

        /// \brief Routes price events to active tick subscriptions.
        void on_event(const utils::Event* const event) override;

        /// \brief Clears all runtime subscriptions.
        void shutdown() override;

    private:
        /// \enum TickSource
        /// \brief Provider-local concrete tick source used for Intrade Bar activation decisions.
        enum class TickSource : std::uint8_t {
            UNKNOWN = 0,   ///< Source is not specified or unsupported.
            POLLING,       ///< Periodic HTTP snapshot/polling source.
            FX_WEBSOCKET,  ///< Intrade Bar `/fxconnect` websocket source.
            BTC_WEBSOCKET  ///< Intrade Bar BTCUSDT websocket source.
        };

        market_data::ProviderInstanceId m_provider_id; ///< Owning provider instance ID.
        ticks_callback_t& m_ticks_callback; ///< Platform tick data callback.
        bars_callback_t&  m_bars_callback;  ///< Platform bar data callback.
        FxPriceWebSocketManager* m_fx_websocket_source = nullptr; ///< Optional FX websocket source.
        std::unordered_map<
            market_data::SubscriptionId,
            market_data::MarketDataSubscriptionHandle> m_tick_subscriptions; ///< Active tick subscriptions.
        market_data::SubscriptionId m_next_subscription_id = 1; ///< Next runtime handle ID.
        std::mutex m_mutex; ///< Protects subscription handles.

        /// \brief Returns the next subscription ID from a local counter value.
        static market_data::SubscriptionId next_subscription_id_from(
                market_data::SubscriptionId& value);

        /// \brief Returns true when a subscription should activate the FX websocket source.
        static bool uses_fx_websocket_source(
                const market_data::MarketDataSubscriptionHandle& subscription);

        /// \brief Selects the concrete Intrade Bar tick source for a tick request.
        static TickSource select_tick_source(
                const market_data::TickSubscriptionRequest& request);

        /// \brief Returns the public transport represented by a concrete source.
        static market_data::MarketDataTransport transport_for_source(
                TickSource source) noexcept;

        /// \brief Returns true when a source event should be routed to a subscription.
        static bool source_matches_subscription(
                const market_data::MarketDataSubscriptionHandle& subscription,
                MarketDataUpdateSource source) noexcept;

        /// \brief Delivers a subscription batch result when a callback was supplied.
        static void dispatch_subscription_batch_result(
                subscription_batch_callback_t callback,
                market_data::MarketDataSubscriptionBatchResult result);

        /// \brief Delivers the first result of a batch to a single-operation callback.
        static void dispatch_single_subscription_result(
                subscription_callback_t callback,
                market_data::MarketDataSubscriptionBatchResult result);

        /// \brief Handles incoming price update events.
        void handle_event(const events::PriceUpdateEvent& event);
    };

    inline bool MarketDataSubscriptionManager::subscribe_ticks(
            market_data::TickSubscriptionRequest request,
            subscription_callback_t callback) {
        market_data::MarketDataSubscriptionBatch batch;
        batch.subscribe_ticks(std::move(request));
        return apply_subscriptions(
            std::move(batch),
            [callback = std::move(callback)](
                    market_data::MarketDataSubscriptionBatchResult result) mutable {
                dispatch_single_subscription_result(std::move(callback), std::move(result));
            });
    }

    inline bool MarketDataSubscriptionManager::subscribe_bars(
            market_data::BarSubscriptionRequest request,
            subscription_callback_t callback) {
        market_data::MarketDataSubscriptionBatch batch;
        batch.subscribe_bars(std::move(request));
        return apply_subscriptions(
            std::move(batch),
            [callback = std::move(callback)](
                    market_data::MarketDataSubscriptionBatchResult result) mutable {
                dispatch_single_subscription_result(std::move(callback), std::move(result));
            });
    }

    inline bool MarketDataSubscriptionManager::unsubscribe(
            market_data::MarketDataSubscriptionHandle subscription,
            subscription_callback_t callback) {
        market_data::MarketDataSubscriptionBatch batch;
        batch.unsubscribe(std::move(subscription));
        return apply_subscriptions(
            std::move(batch),
            [callback = std::move(callback)](
                    market_data::MarketDataSubscriptionBatchResult result) mutable {
                dispatch_single_subscription_result(std::move(callback), std::move(result));
            });
    }

    inline bool MarketDataSubscriptionManager::apply_subscriptions(
            market_data::MarketDataSubscriptionBatch batch,
            subscription_batch_callback_t callback) {
        if (batch.empty()) {
            dispatch_subscription_batch_result(
                std::move(callback),
                market_data::MarketDataSubscriptionBatchResult::failed(
                    market_data::MarketDataSubscriptionStatus::INVALID_REQUEST,
                    "Intrade Bar market-data subscription batch is empty."));
            return false;
        }

        std::vector<market_data::MarketDataSubscriptionHandle> added_subscriptions;
        std::vector<TickSource> added_sources;
        std::vector<market_data::MarketDataSubscriptionHandle> removed_subscriptions;
        std::vector<market_data::MarketDataSubscriptionResult> results;
        market_data::MarketDataSubscriptionBatchResult failure_result;
        market_data::SubscriptionId previous_next_subscription_id =
            market_data::kInvalidSubscriptionId;
        bool failed = false;
        results.reserve(batch.changes.size());

        auto set_failure = [&failure_result, &failed](
                market_data::MarketDataSubscriptionStatus status,
                std::string message,
                std::vector<market_data::MarketDataSubscriptionResult> details) {
            failure_result = market_data::MarketDataSubscriptionBatchResult::failed(
                status,
                std::move(message),
                std::move(details));
            failed = true;
        };

        {
            std::lock_guard<std::mutex> lock(m_mutex);

            previous_next_subscription_id = m_next_subscription_id;
            auto next_id = m_next_subscription_id;
            for (auto& change : batch.changes) {
                switch (change.action) {
                case market_data::MarketDataSubscriptionAction::SUBSCRIBE_TICKS: {
                    change.tick_request.symbol = normalize_symbol_name(std::move(change.tick_request.symbol));
                    if (!change.tick_request.valid()) {
                        set_failure(
                            market_data::MarketDataSubscriptionStatus::INVALID_REQUEST,
                            "Invalid Intrade Bar tick subscription request.",
                            {market_data::MarketDataSubscriptionResult::failed(
                                std::move(change.tick_request),
                                market_data::MarketDataSubscriptionStatus::INVALID_REQUEST,
                                "Invalid Intrade Bar tick subscription request.")});
                        break;
                    }
                    const auto source = select_tick_source(change.tick_request);
                    if (source == TickSource::UNKNOWN) {
                        set_failure(
                            market_data::MarketDataSubscriptionStatus::UNSUPPORTED,
                            "Intrade Bar tick subscription source is not supported for this symbol/transport.",
                            {market_data::MarketDataSubscriptionResult::failed(
                                std::move(change.tick_request),
                                market_data::MarketDataSubscriptionStatus::UNSUPPORTED,
                                "Intrade Bar tick subscription source is not supported for this symbol/transport.")});
                        break;
                    }

                    auto handle = market_data::MarketDataSubscriptionHandle::from_tick_request(
                        m_provider_id,
                        next_subscription_id_from(next_id),
                        change.tick_request);
                    handle.transport = transport_for_source(source);
                    added_subscriptions.push_back(handle);
                    added_sources.push_back(source);
                    results.push_back(market_data::MarketDataSubscriptionResult::subscribed(handle));
                    break;
                }
                case market_data::MarketDataSubscriptionAction::SUBSCRIBE_BARS: {
                    change.bar_request.symbol = normalize_symbol_name(std::move(change.bar_request.symbol));
                    const bool valid = change.bar_request.valid();
                    const auto status = valid
                        ? market_data::MarketDataSubscriptionStatus::UNSUPPORTED
                        : market_data::MarketDataSubscriptionStatus::INVALID_REQUEST;
                    const std::string message = valid
                        ? "Intrade Bar bar subscriptions require a tick-to-bar aggregator and are not supported yet."
                        : "Invalid Intrade Bar bar subscription request.";
                    set_failure(
                        status,
                        message,
                        {market_data::MarketDataSubscriptionResult::failed(
                            std::move(change.bar_request),
                            status,
                            message)});
                    break;
                }
                case market_data::MarketDataSubscriptionAction::UNSUBSCRIBE: {
                    if (!change.subscription.valid()) {
                        set_failure(
                            market_data::MarketDataSubscriptionStatus::INVALID_REQUEST,
                            "Invalid Intrade Bar market-data subscription handle.",
                            {market_data::MarketDataSubscriptionResult::failed(
                                std::move(change.subscription),
                                market_data::MarketDataSubscriptionStatus::INVALID_REQUEST,
                                "Invalid Intrade Bar market-data subscription handle.")});
                        break;
                    }
                    if (change.subscription.provider_id != m_provider_id) {
                        set_failure(
                            market_data::MarketDataSubscriptionStatus::WRONG_PROVIDER,
                            "Intrade Bar market-data subscription handle belongs to another provider.",
                            {market_data::MarketDataSubscriptionResult::failed(
                                std::move(change.subscription),
                                market_data::MarketDataSubscriptionStatus::WRONG_PROVIDER,
                                "Intrade Bar market-data subscription handle belongs to another provider.")});
                        break;
                    }

                    auto it = m_tick_subscriptions.find(change.subscription.id);
                    if (it == m_tick_subscriptions.end()) {
                        set_failure(
                            market_data::MarketDataSubscriptionStatus::FAILED,
                            "Intrade Bar market-data subscription is not active.",
                            {market_data::MarketDataSubscriptionResult::failed(
                                std::move(change.subscription),
                                market_data::MarketDataSubscriptionStatus::FAILED,
                                "Intrade Bar market-data subscription is not active.")});
                        break;
                    }

                    removed_subscriptions.push_back(it->second);
                    results.push_back(market_data::MarketDataSubscriptionResult::unsubscribed(it->second));
                    break;
                }
                case market_data::MarketDataSubscriptionAction::UNSUBSCRIBE_ALL:
                    for (const auto& [id, subscription] : m_tick_subscriptions) {
                        (void)id;
                        removed_subscriptions.push_back(subscription);
                        results.push_back(market_data::MarketDataSubscriptionResult::unsubscribed(subscription));
                    }
                    break;
                }

                if (failed) {
                    break;
                }
            }

            if (!failed) {
                for (const auto& subscription : removed_subscriptions) {
                    m_tick_subscriptions.erase(subscription.id);
                }
                for (const auto& subscription : added_subscriptions) {
                    m_tick_subscriptions[subscription.id] = subscription;
                }
                m_next_subscription_id = next_id;
            }
        }

        if (failed) {
            dispatch_subscription_batch_result(
                std::move(callback),
                std::move(failure_result));
            return false;
        }

        if (m_fx_websocket_source) {
            for (const auto& subscription : removed_subscriptions) {
                if (uses_fx_websocket_source(subscription)) {
                    m_fx_websocket_source->remove_symbol_subscription(subscription.symbol);
                }
            }

            std::vector<market_data::MarketDataSubscriptionHandle> activated_fx_subscriptions;
            for (std::size_t i = 0; i < added_subscriptions.size(); ++i) {
                const auto& subscription = added_subscriptions[i];
                const auto source = i < added_sources.size()
                    ? added_sources[i]
                    : TickSource::UNKNOWN;
                if (source != TickSource::FX_WEBSOCKET) continue;

                if (!m_fx_websocket_source->add_symbol_subscription(subscription.symbol)) {
                    for (const auto& activated : activated_fx_subscriptions) {
                        m_fx_websocket_source->remove_symbol_subscription(activated.symbol);
                    }
                    for (const auto& removed : removed_subscriptions) {
                        if (uses_fx_websocket_source(removed) &&
                            !m_fx_websocket_source->add_symbol_subscription(removed.symbol)) {
                            LOGIT_WARN(
                                "Failed to restore Intrade Bar FX websocket tick source for ",
                                removed.symbol,
                                " after subscription batch rollback.");
                        }
                    }

                    {
                        std::lock_guard<std::mutex> lock(m_mutex);
                        for (const auto& added : added_subscriptions) {
                            m_tick_subscriptions.erase(added.id);
                        }
                        for (const auto& removed : removed_subscriptions) {
                            m_tick_subscriptions[removed.id] = removed;
                        }
                        m_next_subscription_id = previous_next_subscription_id;
                    }

                    dispatch_subscription_batch_result(
                        std::move(callback),
                        market_data::MarketDataSubscriptionBatchResult::failed(
                            market_data::MarketDataSubscriptionStatus::FAILED,
                            "Failed to activate Intrade Bar FX websocket tick source.",
                            {market_data::MarketDataSubscriptionResult::failed(
                                subscription,
                                market_data::MarketDataSubscriptionStatus::FAILED,
                                "Failed to activate Intrade Bar FX websocket tick source.")}));
                    return false;
                }
                activated_fx_subscriptions.push_back(subscription);
            }
        }

        dispatch_subscription_batch_result(
            std::move(callback),
            market_data::MarketDataSubscriptionBatchResult::applied(std::move(results)));
        return true;
    }

    inline bool MarketDataSubscriptionManager::unsubscribe_all(
            subscription_batch_callback_t callback) {
        market_data::MarketDataSubscriptionBatch batch;
        batch.unsubscribe_all();
        return apply_subscriptions(std::move(batch), std::move(callback));
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
    MarketDataSubscriptionManager::next_subscription_id_from(
            market_data::SubscriptionId& value) {
        const auto id = value;
        ++value;
        if (value == market_data::kInvalidSubscriptionId) {
            value = 1;
        }
        return id == market_data::kInvalidSubscriptionId
            ? next_subscription_id_from(value)
            : id;
    }

    inline bool MarketDataSubscriptionManager::uses_fx_websocket_source(
            const market_data::MarketDataSubscriptionHandle& subscription) {
        if (subscription.stream_type != market_data::MarketDataType::TICKS) {
            return false;
        }
        return subscription.transport == market_data::MarketDataTransport::WEBSOCKET &&
               is_fxconnect_supported_symbol(subscription.symbol);
    }

    inline MarketDataSubscriptionManager::TickSource
    MarketDataSubscriptionManager::select_tick_source(
            const market_data::TickSubscriptionRequest& request) {
        const auto normalized = normalize_symbol_name(request.symbol);
        if (normalized.empty()) return TickSource::UNKNOWN;

        const bool btc = is_btc_symbol(normalized);
        const bool fx = is_fxconnect_supported_symbol(normalized);
        if (!btc && !fx) return TickSource::UNKNOWN;

        switch (request.transport) {
        case market_data::MarketDataTransport::POLLING:
            return TickSource::POLLING;
        case market_data::MarketDataTransport::WEBSOCKET:
            return btc ? TickSource::BTC_WEBSOCKET : TickSource::FX_WEBSOCKET;
        case market_data::MarketDataTransport::AUTO:
        case market_data::MarketDataTransport::HYBRID:
            return btc ? TickSource::BTC_WEBSOCKET : TickSource::FX_WEBSOCKET;
        default:
            return TickSource::UNKNOWN;
        }
    }

    inline market_data::MarketDataTransport
    MarketDataSubscriptionManager::transport_for_source(
            TickSource source) noexcept {
        switch (source) {
        case TickSource::POLLING:
            return market_data::MarketDataTransport::POLLING;
        case TickSource::FX_WEBSOCKET:
        case TickSource::BTC_WEBSOCKET:
            return market_data::MarketDataTransport::WEBSOCKET;
        case TickSource::UNKNOWN:
        default:
            return market_data::MarketDataTransport::AUTO;
        }
    }

    inline bool MarketDataSubscriptionManager::source_matches_subscription(
            const market_data::MarketDataSubscriptionHandle& subscription,
            MarketDataUpdateSource source) noexcept {
        switch (subscription.transport) {
        case market_data::MarketDataTransport::POLLING:
            return source == MarketDataUpdateSource::POLLING;
        case market_data::MarketDataTransport::WEBSOCKET:
            return source == MarketDataUpdateSource::WEBSOCKET;
        case market_data::MarketDataTransport::AUTO:
        case market_data::MarketDataTransport::HYBRID:
            return true;
        default:
            return false;
        }
    }

    inline void MarketDataSubscriptionManager::dispatch_subscription_batch_result(
            subscription_batch_callback_t callback,
            market_data::MarketDataSubscriptionBatchResult result) {
        if (callback) {
            callback(std::move(result));
        }
    }

    inline void MarketDataSubscriptionManager::dispatch_single_subscription_result(
            subscription_callback_t callback,
            market_data::MarketDataSubscriptionBatchResult result) {
        if (!callback) return;
        if (!result.results.empty()) {
            callback(std::move(result.results.front()));
            return;
        }

        market_data::MarketDataSubscriptionResult single;
        single.status =
            market_data::MarketDataSubscriptionResult::failure_status_or_failed(result.status);
        single.error_message = std::move(result.error_message);
        callback(std::move(single));
    }

    inline void MarketDataSubscriptionManager::handle_event(
            const events::PriceUpdateEvent& event) {
        std::vector<market_data::TickDataBatch> batches;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_tick_subscriptions.empty()) return;

            for (const auto& tick : event.get_ticks()) {
                const auto normalized_symbol = normalize_symbol_name(tick.symbol);
                for (const auto& [id, subscription] : m_tick_subscriptions) {
                    (void)id;
                    if (subscription.symbol != normalized_symbol) continue;
                    if (!source_matches_subscription(subscription, event.source())) continue;

                    market_data::TickDataBatch* batch = nullptr;
                    for (auto& candidate : batches) {
                        if (candidate.subscription.id == subscription.id) {
                            batch = &candidate;
                            break;
                        }
                    }

                    if (!batch) {
                        market_data::TickDataBatch created;
                        created.subscription = subscription;
                        created.type = market_data::MarketDataType::TICKS;
                        created.symbol = subscription.symbol;
                        created.timeframe = 0;
                        created.price_digits = tick.price_digits;
                        created.volume_digits = tick.volume_digits;
                        batches.push_back(std::move(created));
                        batch = &batches.back();
                    }

                    batch->items.push_back(tick.tick);
                    batch->items.back().set_flag(MarketDataFlags::REALTIME);
                }
            }
        }

        if (m_ticks_callback) {
            for (auto& batch : batches) {
                if (!batch.empty()) {
                    m_ticks_callback(
                        std::make_unique<market_data::TickDataBatch>(std::move(batch)));
                }
            }
        }
    }

} // namespace optionx::platforms::intrade_bar

#endif // _OPTIONX_PLATFORMS_INTRADERBAR_MARKET_DATA_SUBSCRIPTION_MANAGER_HPP_INCLUDED
