#pragma once
#ifndef _OPTIONX_STORAGE_TRADE_RECORD_DB_UTILS_HPP_INCLUDED
#define _OPTIONX_STORAGE_TRADE_RECORD_DB_UTILS_HPP_INCLUDED

/// \file utils.hpp
/// \brief Defines internal helpers used only by TradeRecordDB.

#include <chrono>
#include <cstdint>
#include <string>
#include <utility>

#include "data.hpp"

namespace optionx::storage::trade_record_db_detail {

    /// \brief Selects canonical timestamp used by TradeRecordDB timestamp queries.
    inline std::int64_t selected_timestamp_ms(const TradeRecord& record) noexcept {
        if (record.open_date > 0) return record.open_date;
        if (record.place_date > 0) return record.place_date;
        if (record.send_date > 0) return record.send_date;
        if (record.close_date > 0) return record.close_date;
        if (record.expiry_date > 0) return record.expiry_date;
        return 0;
    }

    /// \brief Returns current wall-clock time in milliseconds.
    inline std::int64_t now_ms() noexcept {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    /// \brief Parses an integer or returns fallback when the string is malformed.
    inline std::int64_t parse_int_or(const std::string& value, std::int64_t fallback) noexcept {
        try {
            std::size_t pos = 0;
            const auto parsed = std::stoll(value, &pos);
            return pos == value.size() ? parsed : fallback;
        } catch (...) {
            return fallback;
        }
    }

    /// \brief Parses an unsigned integer or returns fallback when the string is malformed.
    inline std::uint64_t parse_uint_or(const std::string& value, std::uint64_t fallback) noexcept {
        try {
            std::size_t pos = 0;
            const auto parsed = std::stoull(value, &pos);
            return pos == value.size() ? parsed : fallback;
        } catch (...) {
            return fallback;
        }
    }

    /// \brief Creates successful write result.
    inline TradeRecordDBWriteResult write_success(TradeRecord record) {
        TradeRecordDBWriteResult result;
        result.status = TradeRecordDBStatus::SUCCESS;
        result.record = std::move(record);
        return result;
    }

    /// \brief Creates failed write result.
    inline TradeRecordDBWriteResult write_error(TradeRecordDBStatus status, TradeRecord record, std::string message) {
        TradeRecordDBWriteResult result;
        result.status = status;
        result.record = std::move(record);
        result.message = std::move(message);
        return result;
    }

    /// \brief Creates failed read result.
    inline TradeRecordDBReadResult read_error(TradeRecordDBStatus status, std::string message) {
        TradeRecordDBReadResult result;
        result.status = status;
        result.message = std::move(message);
        return result;
    }

    /// \brief Creates failed list result.
    inline TradeRecordDBListResult list_error(TradeRecordDBStatus status, std::string message) {
        TradeRecordDBListResult result;
        result.status = status;
        result.message = std::move(message);
        return result;
    }

} // namespace optionx::storage::trade_record_db_detail

#endif // _OPTIONX_STORAGE_TRADE_RECORD_DB_UTILS_HPP_INCLUDED
