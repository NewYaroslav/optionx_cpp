#pragma once
#ifndef _OPTIONX_MARKET_DATA_ENUMS_HPP_INCLUDED
#define _OPTIONX_MARKET_DATA_ENUMS_HPP_INCLUDED

/// \file enums.hpp
/// \brief Defines market-data subscription enums.

namespace optionx::market_data {

    /// \enum MarketDataType
    /// \brief Type of market-data payload requested from a provider.
    enum class MarketDataType {
        UNKNOWN = 0, ///< Invalid or unspecified stream type.
        TICKS,       ///< Tick stream.
        BARS         ///< Bar stream.
    };

    /// \enum MarketDataTransport
    /// \brief Preferred transport for live market-data subscriptions.
    enum class MarketDataTransport {
        AUTO = 0,  ///< Let the provider choose the best available transport.
        WEBSOCKET, ///< Prefer websocket streaming.
        POLLING,   ///< Prefer periodic snapshot polling.
        HYBRID     ///< Use streaming with polling fallback where supported.
    };

    /// \enum MarketDataSubscriptionStatus
    /// \brief Lifecycle status of a market-data subscription request.
    enum class MarketDataSubscriptionStatus {
        UNKNOWN = 0,     ///< No status has been assigned.
        APPLIED,         ///< Subscription batch was applied successfully.
        SUBSCRIBED,      ///< Desired subscription was accepted; stream readiness is reported separately.
        UNSUBSCRIBED,    ///< Subscription was stopped.
        UNSUPPORTED,     ///< Provider does not support the requested subscription.
        INVALID_REQUEST, ///< Request or handle is invalid.
        WRONG_PROVIDER,  ///< Subscription handle belongs to another provider instance.
        FAILED           ///< Provider attempted the operation but it failed.
    };

    /// \enum MarketDataStreamStatus
    /// \brief Connection/readiness status of a live market-data stream.
    enum class MarketDataStreamStatus {
        UNKNOWN = 0,  ///< No status has been assigned.
        CONNECTING,   ///< Stream connection is being established.
        CONNECTED,    ///< Transport is connected.
        READY,        ///< Stream transport is connected and source-specific subscription is ready.
        DISCONNECTED, ///< Stream transport disconnected.
        RECONNECTING, ///< Stream is reconnecting.
        STOPPED,      ///< Stream was stopped intentionally.
        FAILED        ///< Stream failed.
    };

    /// \brief Converts MarketDataType to its string representation.
    inline const std::string& to_str(MarketDataType value) noexcept {
        static const std::vector<std::string> names = {
            "UNKNOWN",
            "TICKS",
            "BARS"
        };
        return utils::enum_string_or_unknown(names, static_cast<std::size_t>(value));
    }

    /// \brief Converts MarketDataTransport to its string representation.
    inline const std::string& to_str(MarketDataTransport value) noexcept {
        static const std::vector<std::string> names = {
            "AUTO",
            "WEBSOCKET",
            "POLLING",
            "HYBRID"
        };
        return utils::enum_string_or_unknown(names, static_cast<std::size_t>(value));
    }

    /// \brief Converts MarketDataSubscriptionStatus to its string representation.
    inline const std::string& to_str(MarketDataSubscriptionStatus value) noexcept {
        static const std::vector<std::string> names = {
            "UNKNOWN",
            "APPLIED",
            "SUBSCRIBED",
            "UNSUBSCRIBED",
            "UNSUPPORTED",
            "INVALID_REQUEST",
            "WRONG_PROVIDER",
            "FAILED"
        };
        return utils::enum_string_or_unknown(names, static_cast<std::size_t>(value));
    }

    /// \brief Converts MarketDataStreamStatus to its string representation.
    inline const std::string& to_str(MarketDataStreamStatus value) noexcept {
        static const std::vector<std::string> names = {
            "UNKNOWN",
            "CONNECTING",
            "CONNECTED",
            "READY",
            "DISCONNECTED",
            "RECONNECTING",
            "STOPPED",
            "FAILED"
        };
        return utils::enum_string_or_unknown(names, static_cast<std::size_t>(value));
    }

} // namespace optionx::market_data

#endif // _OPTIONX_MARKET_DATA_ENUMS_HPP_INCLUDED
