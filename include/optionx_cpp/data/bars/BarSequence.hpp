#pragma once
#ifndef _OPTIONX_BAR_SEQUENCE_HPP_INCLUDED
#define _OPTIONX_BAR_SEQUENCE_HPP_INCLUDED

/// \file BarSequence.hpp
/// \brief Defines a normalized sequence of market bars with provider metadata.

#include <cstdint>
#include <vector>
#include <string>
#include <utility>

namespace optionx {

    /// \struct BarSequence
    /// \brief Represents an ordered sequence of OHLC market bars.
    struct BarSequence {
		std::vector<Bar> bars;	///< Ordered bar payloads.
        std::string symbol; 	///< Provider symbol.
		std::string provider;	///< Provider name or source identifier.
        BarTimeframe timeframe; ///< Bar timeframe in seconds; values <= 0 are invalid.
		std::uint16_t flags;         ///< Bar data flags (bitmask of BarUpdateFlags).
        std::uint16_t price_digits;  ///< Number of decimal places for price.
        std::uint16_t volume_digits; ///< Number of decimal places for volume.
        BarPriceSource price_source = BarPriceSource::MID; ///< Price stream used to build the OHLC values.

        /// \brief Default constructor initializes metadata fields to zero.
        BarSequence() : timeframe(0), flags(0), price_digits(0), volume_digits(0) {}

        /// \brief Constructs a bar sequence with all metadata fields.
        BarSequence(
            std::vector<Bar> bs,
            std::string s,
            std::string p,
            BarTimeframe tf,
            std::uint16_t f,
            std::uint16_t d,
            std::uint16_t vd,
            BarPriceSource ps = BarPriceSource::MID)
            : bars(std::move(bs)),
              symbol(std::move(s)),
              provider(std::move(p)),
              timeframe(tf),
              flags(f),
              price_digits(d),
              volume_digits(vd),
              price_source(ps) {}
    }; // BarSequence

} // namespace optionx

#endif // _OPTIONX_BAR_SEQUENCE_HPP_INCLUDED
