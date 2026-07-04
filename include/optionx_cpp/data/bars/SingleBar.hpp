#pragma once
#ifndef _OPTIONX_SINGLE_BAR_HPP_INCLUDED
#define _OPTIONX_SINGLE_BAR_HPP_INCLUDED

/// \file SingleBar.hpp
/// \brief Defines a normalized market bar with provider metadata.

#include <cstdint>
#include <string>
#include <utility>

namespace optionx {

    /// \struct SingleBar
    /// \brief Represents one OHLC market bar and the metadata needed to route it.
    struct SingleBar {
        Bar bar;        		///< OHLC bar payload.
		std::string symbol; 	///< Provider symbol.
		std::string provider;	///< Provider name or source identifier.
        BarTimeframe timeframe; ///< Bar timeframe in seconds; values <= 0 are invalid.
		std::uint16_t flags;     	///< Bar data flags (bitmask of BarUpdateFlags).
        std::uint16_t price_digits;  ///< Number of decimal places for price.
        std::uint16_t volume_digits; ///< Number of decimal places for volume.
        BarPriceSource price_source = BarPriceSource::MID; ///< Price stream used to build the OHLC values.


        /// \brief Constructs a single bar with all metadata fields.
        SingleBar(
            Bar b,
            std::string s,
            std::string p,
            BarTimeframe tf,
            std::uint16_t f,
            std::uint16_t d,
            std::uint16_t vd,
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
