#pragma once
#ifndef _OPTIONX_MARKET_DATA_BASE_MARKET_DATA_PROVIDER_HPP_INCLUDED
#define _OPTIONX_MARKET_DATA_BASE_MARKET_DATA_PROVIDER_HPP_INCLUDED

/// \file BaseMarketDataProvider.hpp
/// \brief Declares the public market-data provider role.

namespace optionx::market_data {

    /// \class BaseMarketDataProvider
    /// \brief Role interface for endpoints that provide live or historical market data.
    class BaseMarketDataProvider {
    public:
        using bars_callback_t = std::function<void(const std::vector<SingleBar>&)>;
        using ticks_callback_t = std::function<void(const std::vector<SingleTick>&)>;
        using bar_history_callback_t = std::function<void(BarHistoryResult)>;
        using subscription_callback_t = std::function<void(MarketDataSubscriptionResult)>;

        /// \brief Constructs a market-data provider and assigns a runtime instance ID.
        BaseMarketDataProvider() : m_provider_id(next_provider_instance_id()) {}

        BaseMarketDataProvider(const BaseMarketDataProvider&) = delete;
        BaseMarketDataProvider& operator=(const BaseMarketDataProvider&) = delete;
        BaseMarketDataProvider(BaseMarketDataProvider&&) = delete;
        BaseMarketDataProvider& operator=(BaseMarketDataProvider&&) = delete;

        /// \brief Virtual destructor for polymorphic provider implementations.
        virtual ~BaseMarketDataProvider() = default;

        /// \brief Returns the runtime provider instance ID used to bind subscription handles.
        ProviderInstanceId provider_id() const noexcept {
            return m_provider_id;
        }

        /// \brief Returns a reference to the live bar-data callback.
        /// \return Mutable callback reference, or a null callback if live bars are unsupported.
        virtual bars_callback_t& on_bar_data() {
            static bars_callback_t null_callback;
            return null_callback;
        }

        /// \brief Returns a reference to the live tick-data callback.
        /// \return Mutable callback reference, or a null callback if live ticks are unsupported.
        virtual ticks_callback_t& on_tick_data() {
            static ticks_callback_t null_callback;
            return null_callback;
        }

        /// \brief Requests a live tick stream subscription.
        /// \param request Tick subscription parameters.
        /// \param callback Callback receiving subscription acceptance or failure.
        /// \return True if the provider accepted the operation for processing; false otherwise.
        virtual bool subscribe_ticks(
                TickSubscriptionRequest request,
                subscription_callback_t callback) {
            if (!request.valid()) {
                dispatch_subscription_result(
                    std::move(callback),
                    MarketDataSubscriptionResult::failed(
                        std::move(request),
                        MarketDataSubscriptionStatus::INVALID_REQUEST,
                        "Invalid tick subscription request."));
                return false;
            }

            dispatch_subscription_result(
                std::move(callback),
                MarketDataSubscriptionResult::failed(
                    std::move(request),
                    MarketDataSubscriptionStatus::UNSUPPORTED,
                    "Tick subscriptions are not supported by this provider."));
            return false;
        }

        /// \brief Requests a live bar stream subscription.
        /// \param request Bar subscription parameters.
        /// \param callback Callback receiving subscription acceptance or failure.
        /// \return True if the provider accepted the operation for processing; false otherwise.
        virtual bool subscribe_bars(
                BarSubscriptionRequest request,
                subscription_callback_t callback) {
            if (!request.valid()) {
                dispatch_subscription_result(
                    std::move(callback),
                    MarketDataSubscriptionResult::failed(
                        std::move(request),
                        MarketDataSubscriptionStatus::INVALID_REQUEST,
                        "Invalid bar subscription request."));
                return false;
            }

            dispatch_subscription_result(
                std::move(callback),
                MarketDataSubscriptionResult::failed(
                    std::move(request),
                    MarketDataSubscriptionStatus::UNSUPPORTED,
                    "Bar subscriptions are not supported by this provider."));
            return false;
        }

        /// \brief Stops a live market-data subscription.
        /// \param subscription Subscription handle returned by a successful subscribe call.
        /// \param callback Callback receiving unsubscribe status.
        /// \return True if the provider accepted the operation for processing; false otherwise.
        virtual bool unsubscribe(
                MarketDataSubscriptionHandle subscription,
                subscription_callback_t callback) {
            if (!subscription.valid()) {
                dispatch_subscription_result(
                    std::move(callback),
                    MarketDataSubscriptionResult::failed(
                        std::move(subscription),
                        MarketDataSubscriptionStatus::INVALID_REQUEST,
                        "Invalid market-data subscription handle."));
                return false;
            }
            if (subscription.provider_id != provider_id()) {
                dispatch_subscription_result(
                    std::move(callback),
                    MarketDataSubscriptionResult::failed(
                        std::move(subscription),
                        MarketDataSubscriptionStatus::WRONG_PROVIDER,
                        "Market-data subscription handle belongs to another provider."));
                return false;
            }

            dispatch_subscription_result(
                std::move(callback),
                MarketDataSubscriptionResult::failed(
                    std::move(subscription),
                    MarketDataSubscriptionStatus::UNSUPPORTED,
                    "Market-data subscriptions are not supported by this provider."));
            return false;
        }

        /// \brief Requests historical bar data for a specified time range.
        /// \param request Historical bar-data request parameters.
        /// \param callback Callback function to receive bars or a failure reason.
        /// \return True if the request was accepted for processing; false otherwise.
        virtual bool fetch_bar_history(
                const BarHistoryRequest& request,
                bar_history_callback_t callback) {
            (void)request;
            (void)callback;
            return false;
        }

    protected:
        /// \brief Delivers a subscription operation result when a callback was supplied.
        static void dispatch_subscription_result(
                subscription_callback_t callback,
                MarketDataSubscriptionResult result) {
            if (callback) {
                callback(std::move(result));
            }
        }

    private:
        ProviderInstanceId m_provider_id = kInvalidProviderInstanceId; ///< Runtime provider instance ID.

        /// \brief Returns the next non-zero provider instance ID.
        static ProviderInstanceId next_provider_instance_id() {
            static std::atomic<ProviderInstanceId> counter{1};
            const auto id = counter.fetch_add(1, std::memory_order_relaxed);
            return id == kInvalidProviderInstanceId ? counter.fetch_add(1, std::memory_order_relaxed) : id;
        }
    };

} // namespace optionx::market_data

#endif // _OPTIONX_MARKET_DATA_BASE_MARKET_DATA_PROVIDER_HPP_INCLUDED
