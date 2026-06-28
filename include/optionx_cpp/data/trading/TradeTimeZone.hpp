#pragma once
#ifndef _OPTIONX_TRADE_TIME_ZONE_HPP_INCLUDED
#define _OPTIONX_TRADE_TIME_ZONE_HPP_INCLUDED

/// \file TradeTimeZone.hpp
/// \brief Defines a local-time context for trade queries and statistics.

#include <cstdint>

#include <time_shield/constants.hpp>
#include <time_shield/date_time_conversions.hpp>
#include <time_shield/enums.hpp>
#include <time_shield/time_zone_conversions.hpp>
#include <time_shield/types.hpp>

namespace optionx {

    /// \enum TradeTimeZoneMode
    /// \brief Selects whether local-time calculations use a fixed offset or a named zone.
    enum class TradeTimeZoneMode {
        FIXED_OFFSET, ///< Fixed UTC offset in seconds.
        NAMED_ZONE    ///< time-shield named zone with historical/DST offset resolution.
    };

    /// \class TradeTimeZone
    /// \brief Time-zone context used by local-time filters, buckets and day queries.
    ///
    /// Fixed-offset mode preserves the old `time_zone_sec` behavior. Named-zone
    /// mode delegates offset resolution to time-shield and therefore accounts for
    /// DST transitions supported by `time_shield::TimeZone`.
    class TradeTimeZone {
    public:
        TradeTimeZoneMode mode = TradeTimeZoneMode::FIXED_OFFSET;
        std::int64_t offset_sec = 0;
        time_shield::TimeZone zone = time_shield::UTC;

        /// \brief Creates UTC fixed-offset context.
        static TradeTimeZone utc() noexcept {
            return fixed_offset(0);
        }

        /// \brief Creates fixed-offset context.
        /// \param offset_seconds UTC offset in seconds.
        static TradeTimeZone fixed_offset(std::int64_t offset_seconds) noexcept {
            TradeTimeZone result;
            result.mode = TradeTimeZoneMode::FIXED_OFFSET;
            result.offset_sec = offset_seconds;
            result.zone = time_shield::UTC;
            return result;
        }

        /// \brief Creates named-zone context.
        /// \param value time-shield zone value. UNKNOWN falls back to UTC.
        static TradeTimeZone named(time_shield::TimeZone value) noexcept {
            if (value == time_shield::UNKNOWN) {
                return utc();
            }

            TradeTimeZone result;
            result.mode = TradeTimeZoneMode::NAMED_ZONE;
            result.offset_sec = 0;
            result.zone = value;
            return result;
        }

        /// \brief Returns true when this context uses time-shield named-zone rules.
        bool is_named_zone() const noexcept {
            return mode == TradeTimeZoneMode::NAMED_ZONE &&
                   zone != time_shield::UNKNOWN;
        }

        /// \brief Resolves UTC offset for the provided UTC timestamp.
        /// \param utc_ms UTC timestamp in milliseconds.
        /// \return Offset in seconds. Unsupported named zones fall back to UTC.
        std::int64_t offset_at_utc_ms(std::int64_t utc_ms) const noexcept {
            if (!is_named_zone()) {
                return offset_sec;
            }

            time_shield::tz_t resolved = 0;
            return time_shield::zone_offset_at_utc_ms(
                static_cast<time_shield::ts_ms_t>(utc_ms),
                zone,
                resolved)
                    ? static_cast<std::int64_t>(resolved)
                    : 0;
        }

        /// \brief Converts UTC milliseconds to this context's local civil milliseconds.
        std::int64_t to_local_ms(std::int64_t utc_ms) const noexcept {
            return utc_ms + offset_at_utc_ms(utc_ms) * time_shield::MS_PER_SEC;
        }

        /// \brief Converts local civil milliseconds in this context back to UTC.
        std::int64_t to_utc_ms(std::int64_t local_ms) const noexcept {
            if (!is_named_zone()) {
                return local_ms - offset_sec * time_shield::MS_PER_SEC;
            }

            const auto utc_ms = time_shield::zone_to_gmt_ms(
                static_cast<time_shield::ts_ms_t>(local_ms),
                zone,
                time_shield::AmbiguousTimePolicy::first_occurrence,
                time_shield::NonexistentTimePolicy::shift_forward);
            if (utc_ms == time_shield::ERROR_TIMESTAMP) {
                return local_ms;
            }
            return static_cast<std::int64_t>(utc_ms);
        }

        /// \brief Returns UTC timestamp for local day start containing utc_ms.
        std::int64_t start_of_local_day_utc_ms(std::int64_t utc_ms) const noexcept {
            const auto local_ms = to_local_ms(utc_ms);
            return to_utc_ms(time_shield::start_of_day_ms(local_ms));
        }

        /// \brief Returns UTC timestamp for next local day start after the day containing utc_ms.
        std::int64_t next_local_day_start_utc_ms(std::int64_t utc_ms) const noexcept {
            const auto local_ms = to_local_ms(utc_ms);
            const auto day_start_local = time_shield::start_of_day_ms(local_ms);
            return to_utc_ms(day_start_local + time_shield::MS_PER_DAY);
        }

        /// \brief Returns UTC timestamp for local hour start containing utc_ms.
        std::int64_t start_of_local_hour_utc_ms(std::int64_t utc_ms) const noexcept {
            const auto local_ms = to_local_ms(utc_ms);
            return to_utc_ms(time_shield::start_of_hour_ms(local_ms));
        }
    };

} // namespace optionx

#endif // _OPTIONX_TRADE_TIME_ZONE_HPP_INCLUDED
