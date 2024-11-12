#pragma once
#ifndef _OPTIONX_CANDLE_HISTORY_REQUEST_HPP_INCLUDED
#define _OPTIONX_CANDLE_HISTORY_REQUEST_HPP_INCLUDED

/// \file CandleHistoryRequest.hpp
/// \brief Contains the CandleHistoryRequest class for requesting historical candlestick data.

#include <string>

namespace optionx {

    /// \class CandleHistoryRequest
    /// \brief Represents a request for historical candlestick data over a specified time range.
    class CandleHistoryRequest {
    public:
        std::string symbol;    ///< The symbol for which the historical data is requested.
        int64_t timeframe = 0; ///< Timeframe of the requested data in seconds.
        int64_t from_ts = 0;   ///< Start timestamp (Unix time) for the requested data range.
        int64_t to_ts = 0;     ///< End timestamp (Unix time) for the requested data range.

        /// \brief Default constructor.
        CandleHistoryRequest() = default;

        /// \brief Constructs a CandleHistoryRequest with all parameters.
        /// \param symbol The symbol for the data.
        /// \param timeframe Timeframe in seconds for each candlestick.
        /// \param from_ts Start timestamp for data retrieval.
        /// \param to_ts End timestamp for data retrieval.
        CandleHistoryRequest(
            const std::string& symbol,
            int64_t timeframe,
            int64_t from_ts,
            int64_t to_ts
        ) : symbol(symbol), timeframe(timeframe), from_ts(from_ts), to_ts(to_ts) {}
    };

}; // namespace optionx

#endif // _OPTIONX_CANDLE_HISTORY_REQUEST_HPP_INCLUDED
