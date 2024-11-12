#pragma once
#ifndef _OPTIONX_SERIES_DATA_HPP_INCLUDED
#define _OPTIONX_SERIES_DATA_HPP_INCLUDED

/// \file SeriesData.hpp
/// \brief Contains the SeriesData struct for handling a series of candlestick data.

#include "Flags.hpp"
#include "CandleData.hpp"
#include <string>
#include <vector>

namespace optionx {

    /// \struct SeriesData
    /// \brief Represents a series of candlestick data for a particular symbol and timeframe.
    struct SeriesData {
        std::string symbol;             ///< Symbol for the candlestick data.
        uint32_t timeframe  = 0;        ///< Timeframe for the series.
        uint16_t digits     = 0;        ///< Number of decimal places for price values.
        uint16_t flags      = 0;        ///< Flags for additional status information.
        std::vector<CandleData> series; ///< Vector holding candlestick data.

        /// \brief Checks if the real-time flag is set.
        /// \return True if real-time flag is set, false otherwise.
        bool is_real_time() const {
            return (flags & REALTIME_FLAG) != 0;
        }

        /// \brief Checks if the initialization flag is set.
        /// \return True if initialization flag is set, false otherwise.
        bool is_initialized() const {
            return (flags & INIT_FLAG) != 0;
        }

        /// \brief Enables the real-time flag.
        void enable_real_time() {
            flags |= REALTIME_FLAG;
        }

        /// \brief Sets the initialization flag.
        void initialize() {
            flags |= INIT_FLAG;
        }
    }; // SeriesData

}; // namespace optionx

#endif // _OPTIONX_SERIES_DATA_HPP_INCLUDED
