#pragma once
#ifndef _OPTIONX_SINGLE_BAR_SEQUENCE_HPP_INCLUDED
#define _OPTIONX_SINGLE_BAR_SEQUENCE_HPP_INCLUDED

/// \file BarData.hpp
/// \brief

#include <cstdint>
#include <string>

namespace optionx {

	/// \struct BarData
    /// \brief Represents a market bar with additional metadata
    struct BarData {
        Bar bar;        		///< Bar data of the specified type
		std::string symbol; 	///< Symbol
		std::string provider;	///< Provider
		uint16_t timeframe; 	///< Timeframe
        uint16_t flags;     	///< Bar data flags (bitmask of BarUpdateFlags)
        uint16_t price_digits;  ///< Number of decimal places for price
        uint16_t volume_digits; ///< Number of decimal places for volume


        /// \brief Constructor to initialize all fields
        BarData(Bar b, std::string s, std::string p, uint16_t tf, uint16_t f, uint16_t d, uint16_t vd)
            : bar(std::move(b)), symbol(std::move(s)), provider(std::move(p)), timeframe(tf), flags(f), price_digits(d), volume_digits(vd) {}
    }; // BarData

}; // namespace optionx

#endif // _OPTIONX_SINGLE_BAR_SEQUENCE_HPP_INCLUDED
