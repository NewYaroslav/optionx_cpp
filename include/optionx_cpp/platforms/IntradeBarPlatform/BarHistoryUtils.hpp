#pragma once
#ifndef _OPTIONX_PLATFORMS_INTRADEBAR_BAR_HISTORY_UTILS_HPP_INCLUDED
#define _OPTIONX_PLATFORMS_INTRADEBAR_BAR_HISTORY_UTILS_HPP_INCLUDED

/// \file BarHistoryUtils.hpp
/// \brief Helpers for Intrade Bar historical bar request slicing.

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "SymbolUtils.hpp"

namespace optionx::platforms::intrade_bar {

    /// \brief Maximum bars requested from the Intrade FX history endpoint at once.
    inline constexpr std::int64_t FX_HISTORY_MAX_BARS_PER_REQUEST = 1000;

    /// \brief Maximum klines requested from Binance at once.
    inline constexpr std::int64_t BINANCE_KLINES_MAX_BARS_PER_REQUEST = 1000;

    /// \brief Inclusive time range for one backend request.
    struct BarHistoryChunk {
        std::int64_t from_ts = 0; ///< Chunk start timestamp in seconds.
        std::int64_t to_ts = 0;   ///< Chunk end timestamp in seconds.
    };

    /// \brief Checks whether historical bars for a symbol must be requested from Binance.
    /// \param symbol Public or normalized symbol.
    /// \return True when the symbol maps to BTCUSDT.
    inline bool uses_binance_bar_history(const std::string& symbol) {
        return is_btc_symbol(symbol);
    }

    /// \brief Converts a normalized FX symbol to the slash form expected by /fxhis.
    /// \param symbol Public or normalized symbol.
    /// \return Broker query symbol such as "NZD/USD".
    inline std::string fx_history_query_symbol(const std::string& symbol) {
        const auto normalized = normalize_symbol_name(symbol);
        if (normalized.size() == 6) {
            return normalized.substr(0, 3) + "/" + normalized.substr(3, 3);
        }
        return normalized;
    }

    /// \brief Returns an empirically known earliest history timestamp.
    /// \param symbol Public or normalized symbol.
    /// \return Minimum supported timestamp in seconds, or 0 when no local limit is known.
    inline std::int64_t minimum_bar_history_from_ts(const std::string& symbol) {
        const auto normalized = normalize_symbol_name(symbol);

        // Binance BTCUSDT history begins in August 2017. FX limits differ by
        // symbol and should be added here only after broker-side verification.
        if (normalized == "BTCUSDT") return 1502942400;

        return 0;
    }

    /// \brief Converts a timeframe in seconds to the Binance kline interval.
    /// \param timeframe_sec Requested timeframe in seconds.
    /// \return Binance interval string, or an empty string when unsupported.
    inline std::string binance_interval_from_timeframe(std::int64_t timeframe_sec) {
        switch (timeframe_sec) {
        case 60: return "1m";
        case 180: return "3m";
        case 300: return "5m";
        case 900: return "15m";
        case 1800: return "30m";
        case 3600: return "1h";
        case 7200: return "2h";
        case 14400: return "4h";
        case 21600: return "6h";
        case 28800: return "8h";
        case 43200: return "12h";
        case 86400: return "1d";
        case 259200: return "3d";
        case 604800: return "1w";
        default:
            return {};
        }
    }

    /// \brief Splits an inclusive history range into backend-sized chunks.
    /// \param from_ts Inclusive start timestamp in seconds.
    /// \param to_ts Inclusive end timestamp in seconds.
    /// \param timeframe_sec Bar timeframe in seconds.
    /// \param max_bars_per_request Backend limit for one request.
    /// \return Ordered chunks without duplicated bar timestamps.
    inline std::vector<BarHistoryChunk> build_bar_history_chunks(
            std::int64_t from_ts,
            std::int64_t to_ts,
            std::int64_t timeframe_sec,
            std::int64_t max_bars_per_request) {
        std::vector<BarHistoryChunk> chunks;
        if (from_ts <= 0 ||
            to_ts < from_ts ||
            timeframe_sec <= 0 ||
            max_bars_per_request <= 0) {
            return chunks;
        }

        const auto chunk_span =
            timeframe_sec * std::max<std::int64_t>(0, max_bars_per_request - 1);
        for (auto current = from_ts; current <= to_ts;) {
            const auto remaining = to_ts - current;
            const auto current_to = current + std::min(remaining, chunk_span);
            chunks.push_back({current, current_to});

            if (current_to > (std::numeric_limits<std::int64_t>::max)() - timeframe_sec) {
                break;
            }
            current = current_to + timeframe_sec;
        }

        return chunks;
    }

} // namespace optionx::platforms::intrade_bar

#endif // _OPTIONX_PLATFORMS_INTRADEBAR_BAR_HISTORY_UTILS_HPP_INCLUDED
