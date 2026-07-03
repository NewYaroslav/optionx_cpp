#pragma once
#ifndef _OPTIONX_MARKET_DATA_SUBSCRIPTION_HPP_INCLUDED
#define _OPTIONX_MARKET_DATA_SUBSCRIPTION_HPP_INCLUDED

/// \file MarketDataSubscription.hpp
/// \brief Defines market-data subscription request, handle, and result DTOs.

namespace optionx::market_data {

    /// \brief Persistent runtime identifier assigned by a market-data provider.
    using MarketDataSubscriptionId = std::uint64_t;

    /// \brief Invalid market-data subscription identifier.
    inline constexpr MarketDataSubscriptionId kInvalidMarketDataSubscriptionId = 0;

    /// \struct MarketDataSubscriptionRequest
    /// \brief Request for a live tick or bar market-data stream.
    struct MarketDataSubscriptionRequest {
        std::string symbol; ///< Broker/provider symbol.
        MarketDataStreamType stream_type = MarketDataStreamType::UNKNOWN; ///< Requested stream type.
        std::int64_t timeframe = 0; ///< Bar timeframe in seconds; ignored for tick streams.
        BarPriceSource price_source = BarPriceSource::MID; ///< Price source for bar streams.
        MarketDataTransport transport = MarketDataTransport::AUTO; ///< Preferred transport.

        /// \brief Creates a tick-stream request.
        static MarketDataSubscriptionRequest ticks(
                std::string symbol,
                MarketDataTransport transport = MarketDataTransport::AUTO) {
            MarketDataSubscriptionRequest request;
            request.symbol = std::move(symbol);
            request.stream_type = MarketDataStreamType::TICKS;
            request.transport = transport;
            return request;
        }

        /// \brief Creates a bar-stream request.
        static MarketDataSubscriptionRequest bars(
                std::string symbol,
                std::int64_t timeframe,
                BarPriceSource price_source = BarPriceSource::MID,
                MarketDataTransport transport = MarketDataTransport::AUTO) {
            MarketDataSubscriptionRequest request;
            request.symbol = std::move(symbol);
            request.stream_type = MarketDataStreamType::BARS;
            request.timeframe = timeframe;
            request.price_source = price_source;
            request.transport = transport;
            return request;
        }

        /// \brief Returns true when the request describes a valid live stream.
        bool valid() const {
            if (symbol.empty()) return false;
            if (stream_type == MarketDataStreamType::TICKS) return true;
            if (stream_type == MarketDataStreamType::BARS) return timeframe > 0;
            return false;
        }
    };

    /// \struct MarketDataSubscriptionHandle
    /// \brief Opaque handle returned for an accepted live market-data subscription.
    struct MarketDataSubscriptionHandle {
        MarketDataSubscriptionId id = kInvalidMarketDataSubscriptionId; ///< Provider-assigned ID.
        std::string symbol; ///< Subscribed symbol.
        MarketDataStreamType stream_type = MarketDataStreamType::UNKNOWN; ///< Stream type.
        std::int64_t timeframe = 0; ///< Bar timeframe in seconds, or 0 for ticks.
        BarPriceSource price_source = BarPriceSource::MID; ///< Price source for bar streams.
        MarketDataTransport transport = MarketDataTransport::AUTO; ///< Transport chosen or requested.

        /// \brief Returns true if the handle can identify a provider subscription.
        bool valid() const {
            return id != kInvalidMarketDataSubscriptionId;
        }

        /// \brief Builds a handle from a request and provider-assigned ID.
        static MarketDataSubscriptionHandle from_request(
                MarketDataSubscriptionId id,
                const MarketDataSubscriptionRequest& request) {
            MarketDataSubscriptionHandle handle;
            handle.id = id;
            handle.symbol = request.symbol;
            handle.stream_type = request.stream_type;
            handle.timeframe = request.timeframe;
            handle.price_source = request.price_source;
            handle.transport = request.transport;
            return handle;
        }
    };

    /// \struct MarketDataSubscriptionResult
    /// \brief Typed result for market-data subscribe and unsubscribe operations.
    struct MarketDataSubscriptionResult {
        bool success = false; ///< True if the operation succeeded.
        MarketDataSubscriptionStatus status = MarketDataSubscriptionStatus::UNKNOWN; ///< Operation status.
        std::string error_message; ///< Failure details, if any.
        MarketDataSubscriptionHandle subscription; ///< Related subscription handle.

        /// \brief Allows result objects to be used in boolean contexts.
        explicit operator bool() const {
            return success;
        }

        /// \brief Creates a successful subscribe result.
        static MarketDataSubscriptionResult subscribed(MarketDataSubscriptionHandle handle) {
            MarketDataSubscriptionResult result;
            result.success = true;
            result.status = MarketDataSubscriptionStatus::SUBSCRIBED;
            result.subscription = std::move(handle);
            return result;
        }

        /// \brief Creates a successful unsubscribe result.
        static MarketDataSubscriptionResult unsubscribed(MarketDataSubscriptionHandle handle) {
            MarketDataSubscriptionResult result;
            result.success = true;
            result.status = MarketDataSubscriptionStatus::UNSUBSCRIBED;
            result.subscription = std::move(handle);
            return result;
        }

        /// \brief Creates a failure result for a request.
        static MarketDataSubscriptionResult failed(
                MarketDataSubscriptionRequest request,
                MarketDataSubscriptionStatus status,
                std::string message) {
            MarketDataSubscriptionResult result;
            result.success = false;
            result.status = status;
            result.error_message = std::move(message);
            result.subscription = MarketDataSubscriptionHandle::from_request(
                kInvalidMarketDataSubscriptionId,
                request);
            return result;
        }

        /// \brief Creates a failure result for an existing handle.
        static MarketDataSubscriptionResult failed(
                MarketDataSubscriptionHandle handle,
                MarketDataSubscriptionStatus status,
                std::string message) {
            MarketDataSubscriptionResult result;
            result.success = false;
            result.status = status;
            result.error_message = std::move(message);
            result.subscription = std::move(handle);
            return result;
        }
    };

} // namespace optionx::market_data

#endif // _OPTIONX_MARKET_DATA_SUBSCRIPTION_HPP_INCLUDED
