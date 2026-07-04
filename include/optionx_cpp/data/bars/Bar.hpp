#pragma once
#ifndef _OPTIONX_BAR_HPP_INCLUDED
#define _OPTIONX_BAR_HPP_INCLUDED

/// \file Bar.hpp
/// \brief Defines the normalized OHLCV bar payload.

#include <cstdint>

namespace optionx {

    /// \struct Bar
    /// \brief Represents a bar (OHLCV)
    struct Bar {
        double open;       ///< Opening price
        double high;       ///< Highest price
        double low;        ///< Lowest price
        double close;      ///< Closing price
        double volume;        ///< Volume traded
        std::uint64_t time_ms; ///< Bar start timestamp in milliseconds.
        std::uint32_t flags;  ///< Market-data flags and encoded price type.

        /// \brief Default constructor that initializes all fields to zero
        Bar() : open(0.0), high(0.0), low(0.0), close(0.0), volume(0.0), time_ms(0), flags(0) {}

        /// \brief Constructor to initialize all fields
        Bar(double o, double h, double l, double c, double v, std::uint64_t ts, std::uint32_t f = 0)
            : open(o), high(h), low(l), close(c), volume(v), time_ms(ts), flags(f) {}

        /// \brief Sets or clears a market-data payload flag.
        /// \param flag Market-data flag to update.
        /// \param value Whether the flag should be set.
        void set_flag(MarketDataFlags flag, bool value = true) noexcept {
            set_flag_in_place(flags, flag, value);
        }

        /// \brief Checks whether a market-data payload flag is set.
        /// \param flag Market-data flag to check.
        /// \return True if the flag is present.
        [[nodiscard]] bool has_flag(MarketDataFlags flag) const noexcept {
            return optionx::has_flag(flags, flag);
        }

        /// \brief Returns the price type encoded in the bar flags.
        [[nodiscard]] MarketPriceType price_type() const noexcept {
            return market_price_type(flags);
        }

        /// \brief Encodes the bar price type in the flags.
        void set_price_type(MarketPriceType type) noexcept {
            set_market_price_type_in_place(flags, type);
        }
    }; // Bar

} // namespace optionx

#endif // _OPTIONX_BAR_HPP_INCLUDED
