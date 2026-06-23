#pragma once
#ifndef _OPTIONX_STORAGE_TRADE_RECORD_DB_DATA_HPP_INCLUDED
#define _OPTIONX_STORAGE_TRADE_RECORD_DB_DATA_HPP_INCLUDED

/// \file data.hpp
/// \brief Defines TradeRecordDB operation result DTOs.

#include <string>
#include <vector>

#include "data/trading.hpp"
#include "enums.hpp"

namespace optionx::storage {

    /// \struct TradeRecordDBMeta
    /// \brief Singleton metadata stored via ValueTable in TradeRecordDB.
    struct TradeRecordDBMeta {
        std::uint64_t db_version = 1;      ///< Database schema version.
        std::uint32_t next_trade_uid = 1;  ///< Next monotonic trade ID (1..UINT32_MAX), stored in low 32 bits of composite key.
        std::int64_t  last_update_ms = 0;  ///< Last wall-clock update time in milliseconds.
    };
    static_assert(std::is_trivially_copyable_v<TradeRecordDBMeta>,
                  "TradeRecordDBMeta must be trivially copyable for ValueTable storage");

    /// \struct TradeRecordDBWriteResult
    /// \brief Result of a TradeRecord write operation.
    struct TradeRecordDBWriteResult {
        TradeRecordDBStatus status = TradeRecordDBStatus::NOT_OPEN; ///< Operation status.
        TradeRecord record;                                         ///< Written or attempted record.
        std::string message;                                        ///< Diagnostic message.

        /// \brief Returns true if operation completed successfully.
        bool ok() const noexcept {
            return status == TradeRecordDBStatus::SUCCESS;
        }
    };

    /// \struct TradeRecordDBReadResult
    /// \brief Result of a single-record TradeRecord read operation.
    struct TradeRecordDBReadResult {
        TradeRecordDBStatus status = TradeRecordDBStatus::NOT_OPEN; ///< Operation status.
        TradeRecord record;                                         ///< Found record, when available.
        bool found = false;                                         ///< True when record was found.
        std::string message;                                        ///< Diagnostic message.

        /// \brief Returns true if the read operation succeeded and found a record.
        bool ok() const noexcept {
            return status == TradeRecordDBStatus::SUCCESS && found;
        }
    };

    /// \struct TradeRecordDBListResult
    /// \brief Result of a multi-record TradeRecord read operation.
    struct TradeRecordDBListResult {
        TradeRecordDBStatus status = TradeRecordDBStatus::NOT_OPEN; ///< Operation status.
        std::vector<TradeRecord> records;                           ///< Found records.
        std::string message;                                        ///< Diagnostic message.

        /// \brief Returns true if the list operation completed successfully.
        bool ok() const noexcept {
            return status == TradeRecordDBStatus::SUCCESS;
        }
    };

} // namespace optionx::storage

#endif // _OPTIONX_STORAGE_TRADE_RECORD_DB_DATA_HPP_INCLUDED
