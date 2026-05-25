#pragma once
#ifndef _OPTIONX_TRADE_RECORD_QUERY_HPP_INCLUDED
#define _OPTIONX_TRADE_RECORD_QUERY_HPP_INCLUDED

/// \file TradeRecordQuery.hpp
/// \brief Defines TradeRecordQuery and related enums for historical trade lookups.

#include <cstdint>
#include <cstddef>
#include <string>

#include "TradeRecordFilter.hpp"
#include "TradeRecord.hpp"

namespace optionx {

    /// \enum TradeRecordTimeField
    /// \brief Selects which timestamp field to use for range queries.
    enum class TradeRecordTimeField {
        AUTO,        ///< Automatically select: open_date, place_date, send_date, close_date, expiry_date.
        PLACE_DATE,  ///< Use place_date field.
        SEND_DATE,   ///< Use send_date field.
        OPEN_DATE,   ///< Use open_date field.
        CLOSE_DATE,  ///< Use close_date field.
        EXPIRY_DATE  ///< Use expiry_date field.
    };

    /// \enum TimeRangeMode
    /// \brief Defines how the time range bounds are interpreted.
    enum class TimeRangeMode {
        NONE,       ///< No time range filtering.
        CLOSED,     ///< start_ms <= t <= stop_ms (inclusive both ends).
        HALF_OPEN   ///< start_ms <= t < stop_ms (inclusive start, exclusive stop).
    };

    /// \brief Extracts the selected timestamp from a TradeRecord.
    /// \param record Trade record to inspect.
    /// \param field Which timestamp field to use.
    /// \return The selected timestamp in milliseconds, or 0 if unavailable.
    inline std::int64_t select_timestamp_ms(
            const TradeRecord& record,
            TradeRecordTimeField field) noexcept {
        switch (field) {
            case TradeRecordTimeField::PLACE_DATE:
                return record.place_date;
            case TradeRecordTimeField::SEND_DATE:
                return record.send_date;
            case TradeRecordTimeField::OPEN_DATE:
                return record.open_date;
            case TradeRecordTimeField::CLOSE_DATE:
                return record.close_date;
            case TradeRecordTimeField::EXPIRY_DATE:
                return record.expiry_date;
            case TradeRecordTimeField::AUTO:
            default:
                if (record.place_date > 0) return record.place_date;
                if (record.send_date > 0) return record.send_date;
                if (record.open_date > 0) return record.open_date;
                if (record.close_date > 0) return record.close_date;
                if (record.expiry_date > 0) return record.expiry_date;
                return 0;
        }
    }

    /// \class TradeRecordQuery
    /// \brief Parameters for querying historical trade records from TradeRecordDB.
    class TradeRecordQuery {
    public:
        std::int64_t start_ms = 0;       ///< Range start in milliseconds (UTC).
        std::int64_t stop_ms = 0;        ///< Range end in milliseconds (UTC).
        std::int64_t time_zone_sec = 0;  ///< Time zone offset in seconds for local-time filtering.

        TradeRecordTimeField time_field = TradeRecordTimeField::AUTO; ///< Which timestamp to query on.
        TimeRangeMode range_mode = TimeRangeMode::CLOSED;             ///< Range inclusion mode.

        std::size_t limit = 0;       ///< Maximum records to return (0 = unlimited).
        std::size_t offset = 0;      ///< Skip first N matched records.
        bool ascending = true;       ///< Sort direction (true = ascending by time).

        TradeRecordFilter filter;    ///< Field-level filter predicates.

        bool fix_stale_status = true;        ///< Apply stale-status correction to results.
        std::int64_t wait_status_sec = 30;   ///< Staleness threshold in seconds.
    };

} // namespace optionx

#endif // _OPTIONX_TRADE_RECORD_QUERY_HPP_INCLUDED
