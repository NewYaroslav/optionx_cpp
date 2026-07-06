#pragma once
#ifndef OPTIONX_HEADER_DATA_TICKS_SINGLE_TICK_HPP_INCLUDED
#define OPTIONX_HEADER_DATA_TICKS_SINGLE_TICK_HPP_INCLUDED

/// \file SingleTick.hpp
/// \brief Defines the SingleTick structure for storing individual tick data with metadata.

#include <cstdint>
#include <string>
#include <utility>

namespace optionx {

    /// \struct SingleTick
    /// \brief Represents a single market tick with associated metadata.
    struct SingleTick {
        Tick tick;                   ///< Tick payload; payload flags live in tick.flags.
        std::string symbol;          ///< Symbol associated with the tick.
        std::string provider;        ///< Data provider associated with the tick.
        std::uint32_t price_digits;  ///< Number of decimal places for price.
        std::uint32_t volume_digits; ///< Number of decimal places for volume.

        /// \brief Default constructor initializing all fields to default values.
        SingleTick() : price_digits(0), volume_digits(0) {}

        /// \brief Constructor to initialize all fields.
        /// \param t Tick data.
        /// \param s Symbol associated with the tick.
        /// \param p Provider associated with the tick.
        /// \param d Number of decimal places for price.
        /// \param vd Number of decimal places for volume.
        SingleTick(
                Tick t,
                std::string s,
                std::string p,
                std::uint32_t d,
                std::uint32_t vd)
            : tick(std::move(t)),
              symbol(std::move(s)),
              provider(std::move(p)),
              price_digits(d),
              volume_digits(vd) {}

        /// \brief Returns the normalized quote midpoint or last traded price.
        /// \return The normalized value returned by Tick::mid_price().
        double mid_price() const {
            return tick.mid_price(price_digits);
        }

        /// \brief Checks whether a market-data payload flag is set.
        /// \param flag Market-data flag to check.
        /// \return True if the wrapped tick has the flag.
        [[nodiscard]] bool has_flag(MarketDataFlags flag) const noexcept {
            return tick.has_flag(flag);
        }

        /// \brief Checks whether a tick update flag is set.
        /// \param flag Tick update flag to check.
        /// \return True if the wrapped tick has the flag.
        [[nodiscard]] bool has_flag(TickUpdateFlags flag) const noexcept {
            return tick.has_flag(flag);
        }
    };

} // namespace optionx

#endif // OPTIONX_HEADER_DATA_TICKS_SINGLE_TICK_HPP_INCLUDED
