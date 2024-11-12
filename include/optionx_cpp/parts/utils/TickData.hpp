#pragma once
#ifndef _OPTIONX_TICK_DATA_HPP_INCLUDED
#define _OPTIONX_TICK_DATA_HPP_INCLUDED

/// \file TickData.hpp
/// \brief Contains the TickData struct for representing a single tick's data.

namespace optionx {

    /// \struct TickData
    /// \brief Represents a single tick's data, including time, bid, and ask prices.
    struct TickData {
        int64_t tick_time = 0;       ///< Timestamp of the tick in milliseconds.
        int64_t receipt_time = 0;    ///< Timestamp of tick receipt in milliseconds.
        double bid = 0.0;            ///< Bid price for the tick.
        double ask = 0.0;            ///< Ask price for the tick.

        /// \brief Default constructor.
        TickData() = default;

        /// \brief Constructs a TickData instance with specified values.
        /// \param tick_time Timestamp of the tick in milliseconds.
        /// \param receipt_time Timestamp of tick receipt in milliseconds.
        /// \param bid Bid price for the tick.
        /// \param ask Ask price for the tick.
        TickData(int64_t tick_time, int64_t receipt_time, double bid, double ask)
            : tick_time(tick_time), receipt_time(receipt_time), bid(bid), ask(ask) {}
    }; // TickData

}; // namespace optionx

#endif // _OPTIONX_TICK_DATA_HPP_INCLUDED
