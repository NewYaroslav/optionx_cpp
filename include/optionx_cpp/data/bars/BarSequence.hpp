#pragma once
#ifndef _OPTIONX_BAR_SEQUENCE_HPP_INCLUDED
#define _OPTIONX_BAR_SEQUENCE_HPP_INCLUDED

/// \file BarSequence.hpp
/// \brief

#include <cstdint>
#include <vector>
#include <string>

namespace optionx {

    /// \struct BarSequence
    /// \brief Represents a sequence of market bars with additional metadata
    struct BarSequence {
		std::vector<Bar> bars;	///< Sequence of bar data of the specified type
        std::string symbol; 	///< Symbol
		std::string provider;	///< Provider
		uint16_t timeframe; 	///< Timeframe
		uint16_t flags;         ///< Bar data flags (bitmask of BarUpdateFlags)
        uint16_t price_digits;  ///< Number of decimal places for price
        uint16_t volume_digits; ///< Number of decimal places for volume

        /// \brief Default constructor initializes metadata fields to zero
        BarSequence() : flags(0), price_digits(0), volume_digits(0) {}

        /// \brief Constructor to initialize all fields
        BarSequence(std::vector<Bar> bs, std::string s, std::string p, uint16_t tf, uint16_t f, uint16_t d, uint16_t vd)
            : bars(std::move(bs)), symbol(std::move(s)), provider(std::move(p)), timeframe(tf), flags(f), price_digits(d), volume_digits(vd) {}
    }; // BarSequence

} // namespace optionx

#endif // _OPTIONX_BAR_SEQUENCE_HPP_INCLUDED
