#pragma once
#ifndef _OPTIONX_MARKET_DATA_ENUMS_HPP_INCLUDED
#define _OPTIONX_MARKET_DATA_ENUMS_HPP_INCLUDED

/// \file enums.hpp
/// \brief Defines market-data subscription enums.

#include <algorithm>
#include <cctype>
#include <string>

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
        READY,        ///< Stream transport is connected and provider-specific setup is complete; payload may arrive later.
        DISCONNECTED, ///< Stream transport disconnected.
        RECONNECTING, ///< Stream is reconnecting.
        STOPPED,      ///< Stream was stopped intentionally.
        FAILED        ///< Stream failed.
    };

    /// \brief Converts MarketDataType to its string representation.
    inline const char* to_str(MarketDataType value) noexcept {
        switch (value) {
        case MarketDataType::TICKS:
            return "TICKS";
        case MarketDataType::BARS:
            return "BARS";
        case MarketDataType::UNKNOWN:
        default:
            return "UNKNOWN";
        }
    }

    /// \brief Converts MarketDataTransport to its string representation.
    inline const char* to_str(MarketDataTransport value) noexcept {
        switch (value) {
        case MarketDataTransport::WEBSOCKET:
            return "WEBSOCKET";
        case MarketDataTransport::POLLING:
            return "POLLING";
        case MarketDataTransport::HYBRID:
            return "HYBRID";
        case MarketDataTransport::AUTO:
        default:
            return "AUTO";
        }
    }

    /// \brief Parses a market-data transport token.
    /// \param value Input token such as AUTO, WEBSOCKET, WS, POLLING, POLL, or HYBRID.
    /// \param fallback Value returned for empty or unknown input.
    /// \return Parsed transport, or fallback.
    inline MarketDataTransport market_data_transport_from_string(
            std::string value,
            MarketDataTransport fallback = MarketDataTransport::AUTO) {
        value.erase(std::remove_if(
            value.begin(),
            value.end(),
            [](unsigned char ch) {
                return std::isspace(ch) != 0;
            }), value.end());
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
            return static_cast<char>(std::toupper(ch));
        });

        if (value.empty()) return fallback;
        if (value == "AUTO") return MarketDataTransport::AUTO;
        if (value == "WEBSOCKET" || value == "WS") return MarketDataTransport::WEBSOCKET;
        if (value == "POLLING" || value == "POLL") return MarketDataTransport::POLLING;
        if (value == "HYBRID") return MarketDataTransport::HYBRID;
        return fallback;
    }

    /// \brief Converts MarketDataSubscriptionStatus to its string representation.
    inline const char* to_str(MarketDataSubscriptionStatus value) noexcept {
        switch (value) {
        case MarketDataSubscriptionStatus::APPLIED:
            return "APPLIED";
        case MarketDataSubscriptionStatus::SUBSCRIBED:
            return "SUBSCRIBED";
        case MarketDataSubscriptionStatus::UNSUBSCRIBED:
            return "UNSUBSCRIBED";
        case MarketDataSubscriptionStatus::UNSUPPORTED:
            return "UNSUPPORTED";
        case MarketDataSubscriptionStatus::INVALID_REQUEST:
            return "INVALID_REQUEST";
        case MarketDataSubscriptionStatus::WRONG_PROVIDER:
            return "WRONG_PROVIDER";
        case MarketDataSubscriptionStatus::FAILED:
            return "FAILED";
        case MarketDataSubscriptionStatus::UNKNOWN:
        default:
            return "UNKNOWN";
        }
    }

    /// \brief Converts MarketDataStreamStatus to its string representation.
    inline const char* to_str(MarketDataStreamStatus value) noexcept {
        switch (value) {
        case MarketDataStreamStatus::CONNECTING:
            return "CONNECTING";
        case MarketDataStreamStatus::CONNECTED:
            return "CONNECTED";
        case MarketDataStreamStatus::READY:
            return "READY";
        case MarketDataStreamStatus::DISCONNECTED:
            return "DISCONNECTED";
        case MarketDataStreamStatus::RECONNECTING:
            return "RECONNECTING";
        case MarketDataStreamStatus::STOPPED:
            return "STOPPED";
        case MarketDataStreamStatus::FAILED:
            return "FAILED";
        case MarketDataStreamStatus::UNKNOWN:
        default:
            return "UNKNOWN";
        }
    }

} // namespace optionx::market_data

#endif // _OPTIONX_MARKET_DATA_ENUMS_HPP_INCLUDED
