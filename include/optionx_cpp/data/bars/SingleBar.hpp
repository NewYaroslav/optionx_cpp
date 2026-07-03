#pragma once
#ifndef _OPTIONX_SINGLE_BAR_HPP_INCLUDED
#define _OPTIONX_SINGLE_BAR_HPP_INCLUDED

/// \file SingleBar.hpp
/// \brief

#include <cstdint>
#include <string>
#include <utility>

namespace optionx {

	/// \struct SingleBar
    /// \brief Represents a market bar with additional metadata
    struct SingleBar {
        Bar bar;        		///< Bar data of the specified type
		std::string symbol; 	///< Symbol
		std::string provider;	///< Provider
        BarTimeframe timeframe; ///< Bar timeframe in seconds.
		uint16_t flags;     	///< Bar data flags (bitmask of BarUpdateFlags)
        uint16_t price_digits;  ///< Number of decimal places for price
        uint16_t volume_digits; ///< Number of decimal places for volume
        BarPriceSource price_source = BarPriceSource::MID; ///< Price stream used to build the OHLC values.


        /// \brief Constructor to initialize all fields
        SingleBar(
            Bar b,
            std::string s,
            std::string p,
            BarTimeframe tf,
            uint16_t f,
            uint16_t d,
            uint16_t vd,
            BarPriceSource ps = BarPriceSource::MID)
            : bar(std::move(b)),
              symbol(std::move(s)),
              provider(std::move(p)),
              timeframe(tf),
              flags(f),
              price_digits(d),
              volume_digits(vd),
              price_source(ps) {}
    }; // SingleBar

}; // namespace optionx

#endif // _OPTIONX_SINGLE_BAR_HPP_INCLUDED
