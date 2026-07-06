#pragma once
#ifndef OPTIONX_HEADER_DATA_TRADING_TRADE_RECORD_TIME_RANGE_HPP_INCLUDED
#define OPTIONX_HEADER_DATA_TRADING_TRADE_RECORD_TIME_RANGE_HPP_INCLUDED

/// \file TradeRecordTimeRange.hpp
/// \brief Defines timestamp-field and range-mode enums for trade-history lookups.

namespace optionx {

    /// \enum TradeRecordTimeField
    /// \brief Selects which timestamp field to use for range queries.
    enum class TradeRecordTimeField {
        AUTO,        ///< Automatically select the first available timestamp.
        PLACE_DATE,  ///< Use place_date field.
        SEND_DATE,   ///< Use send_date field.
        OPEN_DATE,   ///< Use open_date field.
        CLOSE_DATE   ///< Use close_date field.
    };

    /// \enum TimeRangeMode
    /// \brief Defines how the time range bounds are interpreted.
    enum class TimeRangeMode {
        NONE,       ///< No time range filtering.
        CLOSED,     ///< start_ms <= t <= stop_ms (inclusive both ends).
        HALF_OPEN   ///< start_ms <= t < stop_ms (inclusive start, exclusive stop).
    };

} // namespace optionx

#endif // OPTIONX_HEADER_DATA_TRADING_TRADE_RECORD_TIME_RANGE_HPP_INCLUDED
