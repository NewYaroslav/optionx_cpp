#pragma once
#ifndef _OPTIONX_TRADE_RECORD_QUERY_HPP_INCLUDED
#define _OPTIONX_TRADE_RECORD_QUERY_HPP_INCLUDED

/// \file TradeRecordQuery.hpp
/// \brief Defines TradeRecordQuery and related enums for historical trade lookups.

#include <cstdint>
#include <cstddef>
#include <string>

#include "TradeRecordTimeRange.hpp"
#include "TradeTimeZone.hpp"
#include "TradeRecordFilter.hpp"
#include "TradeRecord.hpp"

namespace optionx {

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
            case TradeRecordTimeField::AUTO:
            default:
                if (record.place_date > 0) return record.place_date;
                if (record.send_date > 0) return record.send_date;
                if (record.open_date > 0) return record.open_date;
                if (record.close_date > 0) return record.close_date;
                return 0;
        }
    }

    /// \class TradeRecordQuery
    /// \brief Parameters for querying historical trade records from TradeRecordDB.
    class TradeRecordQuery {
    public:
        std::int64_t start_ms = 0;       ///< Range start in milliseconds (UTC).
        std::int64_t stop_ms = 0;        ///< Range end in milliseconds (UTC).
        TradeTimeZone time_zone;         ///< Local-time context for component filters.

        TradeRecordTimeField time_field = TradeRecordTimeField::AUTO; ///< Which timestamp to query on.
        TimeRangeMode range_mode = TimeRangeMode::NONE;               ///< Range inclusion mode.

        std::size_t limit = 0;       ///< Maximum records to return (0 = unlimited).
        std::size_t offset = 0;      ///< Skip first N matched records.
        bool ascending = true;       ///< Sort direction (true = ascending by time).

        TradeRecordFilter filter;    ///< Field-level filter predicates.

        bool fix_stale_status = true;        ///< Apply stale-status correction to results.
        std::int64_t wait_status_sec = 30;   ///< Staleness threshold in seconds.
        std::int64_t coarse_expansion_ms = 86400000; ///< Scan expansion for non-AUTO time_field (default 24h).

        /// \brief Creates a query that returns all records matching the field filters.
        /// \return Query without a time range.
        static TradeRecordQuery all() {
            return TradeRecordQuery();
        }
    };

} // namespace optionx

#endif // _OPTIONX_TRADE_RECORD_QUERY_HPP_INCLUDED
