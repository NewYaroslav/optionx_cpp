#pragma once
#ifndef _OPTIONX_TICK_DATA_HPP_INCLUDED
#define _OPTIONX_TICK_DATA_HPP_INCLUDED

/// \file TickData.hpp
/// \brief Defines the TickData structure for storing individual tick data with metadata.

#include <cstdint>
#include <string>

namespace optionx {

    /// \struct TickData
    /// \brief Represents a single market tick with associated metadata.
    struct TickData {
        Tick tick;              ///< Tick data of the specified type.
        std::string symbol;     ///< Symbol associated with the tick.
        std::string provider;   ///< Data provider associated with the tick.
        uint32_t price_digits;  ///< Number of decimal places for price.
        uint32_t volume_digits; ///< Number of decimal places for volume.
        uint32_t flags;         ///< Tick data flags (bitmask of UpdateFlags).

        /// \brief Default constructor initializing all fields to default values.
        TickData() : price_digits(0), volume_digits(0), flags(0) {}

        /// \brief Constructor to initialize all fields.
        /// \param t Tick data.
        /// \param s Symbol associated with the tick.
        /// \param p Provider associated with the tick.
        /// \param d Number of decimal places for price.
        /// \param vd Number of decimal places for volume.
        /// \param f Tick data flags.
        TickData(Tick t, std::string s, std::string p, uint32_t d, uint32_t vd, uint32_t f)
            : tick(std::move(t)), symbol(std::move(s)), provider(std::move(p)), price_digits(d), volume_digits(vd), flags(f) {}

        /// \brief Calculates the average price based on bid and ask.
        /// \return The mid-price (average of bid and ask).
        double mid_price() const {
            return utils::normalize_double(tick.mid_price(), price_digits);
        }

        /// \brief Sets a specific flag in the tick's flags.
        /// \param flag The flag to set (from TickStatusFlags).
        void set_flag(TickStatusFlags flag) {
            flags |= static_cast<uint64_t>(flag);
        }

        void set_flag(TickStatusFlags flag, bool value) {
            flags |= value ? static_cast<uint64_t>(flag) : 0x00;
        }

        /// \brief Checks if a specific flag is set in the tick's flags.
        /// \param flag The flag to check (from TickStatusFlags).
        /// \return True if the flag is set, otherwise false.
        bool has_flag(TickStatusFlags flag) const {
            return (flags & static_cast<uint64_t>(flag)) != 0;
        }
    };

} // namespace optionx

#endif // _OPTIONX_TICK_DATA_HPP_INCLUDED

