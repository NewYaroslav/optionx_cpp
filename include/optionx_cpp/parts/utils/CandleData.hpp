#pragma once
#ifndef _OPTIONX_CANDLE_DATA_HPP_INCLUDED
#define _OPTIONX_CANDLE_DATA_HPP_INCLUDED

/// \file CandleData.hpp
/// \brief Contains the CandleData struct for storing individual candlestick data.

namespace optionx {

    /// \struct CandleData
    /// \brief Represents the data for a single candlestick, including open, high, low, close, and volume.
    struct CandleData {
        int64_t timestamp;  ///< Unix timestamp representing the start time of the candle (Unix, ms).
        double open;        ///< Opening price of the candle.
        double high;        ///< Highest price during the candle period.
        double low;         ///< Lowest price during the candle period.
        double close;       ///< Closing price of the candle.
        double volume;      ///< Volume traded during the candle period.
    }; // CandleData

}; // namespace optionx

#endif // _OPTIONX_CANDLE_DATA_HPP_INCLUDED
