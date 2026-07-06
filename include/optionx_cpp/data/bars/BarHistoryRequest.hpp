#pragma once
#ifndef OPTIONX_HEADER_DATA_BARS_BAR_HISTORY_REQUEST_HPP_INCLUDED
#define OPTIONX_HEADER_DATA_BARS_BAR_HISTORY_REQUEST_HPP_INCLUDED

/// \file BarHistoryRequest.hpp
/// \brief Contains the BarHistoryRequest class for requesting historical bar data.

#include <cstdint>
#include <string>

namespace optionx {

    /// \class BarHistoryRequest
    /// \brief Represents a request for historical bar data over a specified time range.
    class BarHistoryRequest {
    public:
        std::string symbol;    ///< The symbol for which the historical data is requested.
        BarTimeframe timeframe = 0; ///< Requested bar timeframe in seconds; values <= 0 are invalid.
        int64_t from_ts = 0;   ///< Start timestamp (Unix time) for the requested data range.
        int64_t to_ts = 0;     ///< End timestamp (Unix time) for the requested data range.
        BarPriceSource price_source = BarPriceSource::MID; ///< Requested price stream for OHLC values.

        /// \brief Default constructor.
        BarHistoryRequest() = default;

        /// \brief Constructs a BarHistoryRequest with all parameters.
        /// \param symbol The symbol for the data.
        /// \param timeframe Timeframe in seconds for each bar.
        /// \param from_ts Start timestamp for data retrieval.
        /// \param to_ts End timestamp for data retrieval.
        /// \param price_source Requested price stream for OHLC values.
        BarHistoryRequest(
            const std::string& symbol,
            BarTimeframe timeframe,
            int64_t from_ts,
            int64_t to_ts,
            BarPriceSource price_source = BarPriceSource::MID
        ) : symbol(symbol),
            timeframe(timeframe),
            from_ts(from_ts),
            to_ts(to_ts),
            price_source(price_source) {}
    };

}; // namespace optionx

#endif // OPTIONX_HEADER_DATA_BARS_BAR_HISTORY_REQUEST_HPP_INCLUDED
