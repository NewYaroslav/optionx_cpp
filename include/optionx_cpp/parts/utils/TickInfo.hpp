#pragma once
#ifndef _OPTIONX_TICK_INFO_HPP_INCLUDED
#define _OPTIONX_TICK_INFO_HPP_INCLUDED

/// \file TickInfo.hpp
/// \brief Contains the TickInfo struct for storing detailed information about a single tick.

#include "Flags.hpp"
#include "TickData.hpp"
#include <string>

namespace optionx {

    /// \struct TickInfo
    /// \brief Stores information about a single tick, including symbol, flags, and tick data.
    struct TickInfo {
        std::string symbol;     ///< Symbol name associated with the tick.
        uint16_t digits = 0;    ///< Number of decimal places for price values.
        uint16_t flags = 0;     ///< Flags for additional status information.
        TickData tick;          ///< Tick data including timestamp, bid, and ask prices.

        /// \brief Default constructor.
        TickInfo() = default;

        /// \brief Constructs a TickInfo instance with specified values.
        /// \param symbol Symbol name associated with the tick.
        /// \param digits Number of decimal places for price values.
        /// \param flags Status flags for the tick.
        /// \param tick Tick data including timestamp, bid, and ask prices.
        TickInfo(const std::string& symbol, uint16_t digits, uint16_t flags, const TickData& tick)
            : symbol(symbol), digits(digits), flags(flags), tick(tick) {}

        /// \brief Checks if the real-time flag is set.
        /// \return True if the real-time flag is set; false otherwise.
        bool is_real_time() const {
            return (flags & REALTIME_FLAG) != 0;
        }

        /// \brief Checks if the initialization flag is set.
        /// \return True if the initialization flag is set; false otherwise.
        bool is_initialized() const {
            return (flags & INIT_FLAG) != 0;
        }

        /// \brief Sets the real-time flag.
        void enable_real_time() {
            flags |= REALTIME_FLAG;
        }

        /// \brief Sets the initialization flag.
        void initialize() {
            flags |= INIT_FLAG;
        }
    }; // TickInfo

}; // namespace optionx

#endif // _OPTIONX_TICK_INFO_HPP_INCLUDED
