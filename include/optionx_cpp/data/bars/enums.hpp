#pragma once
#ifndef _OPTIONX_BAR_ENUMS_HPP_INCLUDED
#define _OPTIONX_BAR_ENUMS_HPP_INCLUDED

/// \file enums.hpp
/// \brief Defines bar data enums.

#include <cstdint>

namespace optionx {

    /// \enum BarPriceSource
    /// \brief Defines which price stream was used to build a single OHLC bar.
    enum class BarPriceSource : std::uint8_t {
        UNKNOWN = 0, ///< Price source is not specified.
        BID,         ///< Bar was built from bid prices.
        ASK,         ///< Bar was built from ask prices.
        MID,         ///< Bar was built from the bid/ask midpoint.
        LAST         ///< Bar was built from last trade prices.
    };

} // namespace optionx

#endif // _OPTIONX_BAR_ENUMS_HPP_INCLUDED
