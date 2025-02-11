#pragma once
#ifndef _OPTIONX_BAR_HPP_INCLUDED
#define _OPTIONX_BAR_HPP_INCLUDED

/// \file Bar.hpp
/// \brief 

#include <cstdint>

namespace optionx {

    /// \struct Bar
    /// \brief Represents a bar (OHLCV)
    struct Bar {
        double open;       ///< Opening price
        double high;       ///< Highest price
        double low;        ///< Lowest price
        double close;      ///< Closing price
        double volume;     ///< Volume traded
        uint64_t time_ms;  ///< Bar start timestamp in milliseconds

        /// \brief Default constructor that initializes all fields to zero
        Bar() : open(0.0), high(0.0), low(0.0), close(0.0), volume(0.0), time_ms(0) {}

        /// \brief Constructor to initialize all fields
        Bar(double o, double h, double l, double c, double v, uint64_t ts)
            : open(o), high(h), low(l), close(c), volume(v), time_ms(ts) {}
    }; // Bar

}; // namespace optionx

#endif // _OPTIONX_BAR_HPP_INCLUDED