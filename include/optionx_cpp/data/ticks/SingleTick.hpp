#pragma once
#ifndef _OPTIONX_SINGLE_TICK_HPP_INCLUDED
#define _OPTIONX_SINGLE_TICK_HPP_INCLUDED

/// \file SingleTick.hpp
/// \brief Defines the SingleTick structure for storing individual tick data with metadata.

#include <cstdint>
#include <string>
#include <utility>

namespace optionx {

    /// \struct SingleTick
    /// \brief Represents a single market tick with associated metadata.
    struct SingleTick {
        Tick tick;              ///< Tick data of the specified type.
        std::string symbol;     ///< Symbol associated with the tick.
        std::string provider;   ///< Data provider associated with the tick.
        std::uint32_t price_digits;  ///< Number of decimal places for price.
        std::uint32_t volume_digits; ///< Number of decimal places for volume.
        std::uint32_t flags;         ///< Tick data flags (bitmask of UpdateFlags).

        /// \brief Default constructor initializing all fields to default values.
        SingleTick() : price_digits(0), volume_digits(0), flags(0) {}

        /// \brief Constructor to initialize all fields.
        /// \param t Tick data.
        /// \param s Symbol associated with the tick.
        /// \param p Provider associated with the tick.
        /// \param d Number of decimal places for price.
        /// \param vd Number of decimal places for volume.
        /// \param f Tick data flags.
        SingleTick(
                Tick t,
                std::string s,
                std::string p,
                std::uint32_t d,
                std::uint32_t vd,
                std::uint32_t f)
            : tick(std::move(t)), symbol(std::move(s)), provider(std::move(p)), price_digits(d), volume_digits(vd), flags(f) {}

        /// \brief Calculates the average price based on bid and ask.
        /// \return The mid-price (average of bid and ask).
        double mid_price() const {
            return utils::normalize_double(tick.mid_price(), price_digits);
        }

        /// \brief Sets a specific flag in the tick's flags.
        /// \param flag The flag to set (from TickStatusFlags).
        void set_flag(TickStatusFlags flag) {
            flags |= static_cast<std::uint32_t>(flag);
        }

        /// \brief Sets or clears a tick status flag.
        /// \param flag The flag to update.
        /// \param value Whether the flag should be set.
        void set_flag(TickStatusFlags flag, bool value) {
            flags = value
                ? flags | static_cast<std::uint32_t>(flag)
                : flags & ~static_cast<std::uint32_t>(flag);
        }

        /// \brief Checks if a specific flag is set in the tick's flags.
        /// \param flag The flag to check (from TickStatusFlags).
        /// \return True if the flag is set, otherwise false.
        bool has_flag(TickStatusFlags flag) const {
            return (flags & static_cast<std::uint32_t>(flag)) != 0;
        }
    };

} // namespace optionx

#endif // _OPTIONX_SINGLE_TICK_HPP_INCLUDED
