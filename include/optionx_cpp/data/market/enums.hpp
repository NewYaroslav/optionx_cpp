#pragma once
#ifndef _OPTIONX_DATA_MARKET_ENUMS_HPP_INCLUDED
#define _OPTIONX_DATA_MARKET_ENUMS_HPP_INCLUDED

/// \file enums.hpp
/// \brief Defines shared market-data payload flags.

#include <cstdint>

namespace optionx {

    /// \enum MarketDataFlags
    /// \brief Flags describing how a tick or bar entered the market-data pipeline.
    enum class MarketDataFlags : std::uint32_t {
        NONE       = 0,       ///< No market-data flags are set.
        REALTIME   = 1u << 16,///< Payload came from a live stream or polling source.
        HISTORICAL = 1u << 17,///< Payload came from a history request.
        BACKFILL   = 1u << 18,///< Payload was loaded to fill a stream gap.
        INCOMPLETE = 1u << 19,///< Bar payload is still forming.
        FINALIZED  = 1u << 20 ///< Payload is complete and will not be updated.
    };

    /// \enum MarketPriceType
    /// \brief Price stream represented by a market-data payload.
    enum class MarketPriceType : std::uint8_t {
        UNKNOWN = 0, ///< Price type is not specified.
        BID,         ///< Bid price.
        ASK,         ///< Ask price.
        MID,         ///< Bid/ask midpoint.
        LAST         ///< Last traded price.
    };

    /// \enum MarketDataUpdateSource
    /// \brief Transport-level source that produced a market-data update event.
    enum class MarketDataUpdateSource : std::uint8_t {
        UNKNOWN = 0, ///< Source is not specified.
        POLLING,     ///< Periodic snapshot/polling source.
        WEBSOCKET    ///< Websocket streaming source.
    };

    /// \brief Bit offset used to encode MarketPriceType inside payload flags.
    inline constexpr std::uint32_t MARKET_PRICE_TYPE_SHIFT = 24;

    /// \brief Bit mask reserved for the encoded MarketPriceType value.
    inline constexpr std::uint32_t MARKET_PRICE_TYPE_MASK = 0xFu << MARKET_PRICE_TYPE_SHIFT;

    /// \brief Checks whether market-data flags contain a specific flag.
    [[nodiscard]] inline bool has_flag(std::uint32_t flags, MarketDataFlags flag) noexcept {
        return (flags & static_cast<std::uint32_t>(flag)) != 0U;
    }

    /// \brief Returns a flags value with the given market-data flag set or cleared.
    [[nodiscard]] inline std::uint32_t set_flag(
            std::uint32_t flags,
            MarketDataFlags flag,
            bool value = true) noexcept {
        if (value) {
            return flags | static_cast<std::uint32_t>(flag);
        }
        return flags & ~static_cast<std::uint32_t>(flag);
    }

    /// \brief Sets or clears a market-data flag in-place.
    inline void set_flag_in_place(
            std::uint32_t& flags,
            MarketDataFlags flag,
            bool value = true) noexcept {
        flags = set_flag(flags, flag, value);
    }

    /// \brief Reads the encoded market price type from a flags value.
    [[nodiscard]] inline MarketPriceType market_price_type(std::uint32_t flags) noexcept {
        return static_cast<MarketPriceType>(
            (flags & MARKET_PRICE_TYPE_MASK) >> MARKET_PRICE_TYPE_SHIFT);
    }

    /// \brief Returns a flags value with an encoded market price type.
    [[nodiscard]] inline std::uint32_t set_market_price_type(
            std::uint32_t flags,
            MarketPriceType type) noexcept {
        flags &= ~MARKET_PRICE_TYPE_MASK;
        flags |= (static_cast<std::uint32_t>(type) << MARKET_PRICE_TYPE_SHIFT) &
                 MARKET_PRICE_TYPE_MASK;
        return flags;
    }

    /// \brief Encodes a market price type in-place.
    inline void set_market_price_type_in_place(
            std::uint32_t& flags,
            MarketPriceType type) noexcept {
        flags = set_market_price_type(flags, type);
    }

} // namespace optionx

#endif // _OPTIONX_DATA_MARKET_ENUMS_HPP_INCLUDED
