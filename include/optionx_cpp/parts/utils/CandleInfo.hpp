#pragma once
#ifndef _OPTIONX_CANDLE_INFO_HPP_INCLUDED
#define _OPTIONX_CANDLE_INFO_HPP_INCLUDED

/// \file CandleInfo.hpp
/// \brief Contains the CandleInfo struct for storing information about a single candlestick.

#include "Flags.hpp"
#include "CandleData.hpp"
#include <string>

namespace optionx {

    /// \struct CandleInfo
    /// \brief Stores metadata and data for a single candlestick.
    struct CandleInfo {
    public:
        std::string symbol;         ///< Symbol for the candlestick data.
        uint32_t timeframe  = 0;    ///< Timeframe for the candlestick data.
        uint16_t digits     = 0;    ///< Number of decimal places for price values.
        uint16_t flags      = 0;    ///< Flags for additional status information.
        CandleData candle;          ///< Data for the candlestick.

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
    }; // CandleInfo

}; // namespace optionx

#endif // _OPTIONX_CANDLE_INFO_HPP_INCLUDED
