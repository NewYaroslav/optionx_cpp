#pragma once
#ifndef _OPTIONX_TICK_HPP_INCLUDED
#define _OPTIONX_TICK_HPP_INCLUDED

/// \file Tick.hpp
/// \brief Defines the Tick structure used for representing market data.

#include <cstdint>

namespace optionx {

    /// \struct Tick
    /// \brief Represents a market tick with bid, ask, volume, and derived average price.
    struct Tick {
        double ask;              ///< Ask price
        double bid;              ///< Bid price
        double volume;           ///< Trade volume (can store both whole units and high precision)
        uint64_t time_ms;        ///< Tick timestamp in milliseconds
        uint64_t received_ms;    ///< Time when tick was received from the server
        uint64_t flags;          ///< Flags representing tick characteristics (combination of TickUpdateFlags)

        /// \brief Default constructor that initializes all fields to zero or equivalent values
        Tick()
            : ask(0.0), bid(0.0), volume(0.0),
              time_ms(0), received_ms(0), flags(0) {}

        /// \brief Constructor to initialize all fields
        /// \param a Ask price
        /// \param b Bid price
        /// \param v Trade volume in whole units
        /// \param ts Tick timestamp in milliseconds
        /// \param rt Time when tick was received from the server
        /// \param f Flags representing tick characteristics
        Tick(double a, double b, double v,
             uint64_t ts, uint64_t rt, uint64_t f)
            : ask(a), bid(b), volume(v),
              time_ms(ts), received_ms(rt), flags(f) {}

        /// \brief Calculates the average price based on bid and ask.
        /// \return The mid-price (average of bid and ask).
        double mid_price() const {
            return (ask + bid) / 2.0;
        }

        /// \brief Sets a specific flag in the tick's flags.
        /// \param flag The flag to set (from TickUpdateFlags).
        void set_flag(TickUpdateFlags flag) {
            flags |= static_cast<uint64_t>(flag);
        }

        void set_flag(TickUpdateFlags flag, bool value) {
            flags |= value ? static_cast<uint64_t>(flag) : 0x00;
        }

        /// \brief Checks if a specific flag is set in the tick's flags.
        /// \param flag The flag to check (from TickUpdateFlags).
        /// \return True if the flag is set, otherwise false.
        bool has_flag(TickUpdateFlags flag) const {
            return (flags & static_cast<uint64_t>(flag)) != 0;
        }
    };

} // namespace optionx

#endif // _OPTIONX_TICK_HPP_INCLUDED
