#pragma once
#ifndef _OPTIONX_MARKET_DATA_ENUMS_HPP_INCLUDED
#define _OPTIONX_MARKET_DATA_ENUMS_HPP_INCLUDED

/// \file enums.hpp
/// \brief Defines market-data subscription enums.

namespace optionx::market_data {

    /// \enum MarketDataStreamType
    /// \brief Type of live market-data stream requested from a provider.
    enum class MarketDataStreamType {
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
        SUBSCRIBED,      ///< Subscription was accepted.
        UNSUBSCRIBED,    ///< Subscription was stopped.
        UNSUPPORTED,     ///< Provider does not support the requested subscription.
        INVALID_REQUEST, ///< Request or handle is invalid.
        FAILED           ///< Provider attempted the operation but it failed.
    };

    /// \brief Converts MarketDataStreamType to its string representation.
    inline const std::string& to_str(MarketDataStreamType value) noexcept {
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
            "SUBSCRIBED",
            "UNSUBSCRIBED",
            "UNSUPPORTED",
            "INVALID_REQUEST",
            "FAILED"
        };
        return utils::enum_string_or_unknown(names, static_cast<std::size_t>(value));
    }

} // namespace optionx::market_data

#endif // _OPTIONX_MARKET_DATA_ENUMS_HPP_INCLUDED
