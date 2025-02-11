#pragma once
#ifndef _OPTIONX_TICK_SEQUENCE_DATA_HPP_INCLUDED
#define _OPTIONX_TICK_SEQUENCE_DATA_HPP_INCLUDED

/// \file TickSequenceData.hpp
/// \brief Defines the TickSequenceData structure for storing a sequence of tick data with metadata.

#include <vector>
#include <string>

namespace optionx {

    /// \struct TickSequenceData
    /// \brief Represents a sequence of tick data with metadata.
    struct TickSequenceData {
        std::vector<Tick> ticks;    ///< Sequence of tick data of the specified type.
        std::string symbol;         ///< Symbol associated with the tick sequence.
        std::string provider;       ///< Data provider associated with the tick sequence.
        uint32_t price_digits;      ///< Number of decimal places for price.
        uint32_t volume_digits;     ///< Number of decimal places for volume.
        uint32_t flags;             ///< Tick data flags (bitmask of UpdateFlags).

        /// \brief Default constructor initializing all fields to default values.
        TickSequenceData() : price_digits(0), volume_digits(0), flags(0) {}

        /// \brief Constructor to initialize all fields.
        /// \param ts Sequence of tick data.
        /// \param s Symbol associated with the tick sequence.
        /// \param p Provider associated with the tick sequence.
        /// \param f Tick data flags.
        /// \param d Number of decimal places for price.
        /// \param vd Number of decimal places for volume.
        TickSequenceData(std::vector<Tick> ts, std::string s, std::string p, uint32_t d, uint32_t vd, uint32_t f)
            : ticks(std::move(ts)), symbol(std::move(s)), provider(std::move(p)), price_digits(d), volume_digits(vd), flags(f) {}
    };

} // namespace optionx

#endif // _OPTIONX_TICK_SEQUENCE_DATA_HPP_INCLUDED
