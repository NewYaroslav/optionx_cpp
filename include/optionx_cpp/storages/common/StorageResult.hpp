#pragma once
#ifndef OPTIONX_HEADER_STORAGES_COMMON_STORAGE_RESULT_HPP_INCLUDED
#define OPTIONX_HEADER_STORAGES_COMMON_STORAGE_RESULT_HPP_INCLUDED

/// \file StorageResult.hpp
/// \brief Defines common storage operation result DTO templates.

#include <string>
#include <vector>

#include "StorageStatus.hpp"

namespace optionx::storage {

    /// \struct StorageWriteResult
    /// \brief Result of a single-record storage write operation.
    template<class Record>
    struct StorageWriteResult {
        StorageStatus status = StorageStatus::NOT_OPEN; ///< Operation status.
        Record record;                                  ///< Written or attempted record.
        std::string message;                            ///< Diagnostic message.

        /// \brief Returns true if operation completed successfully.
        bool ok() const noexcept {
            return status == StorageStatus::SUCCESS;
        }

        /// \brief Returns true if operation completed successfully.
        explicit operator bool() const noexcept {
            return ok();
        }
    };

    /// \struct StorageReadResult
    /// \brief Result of a single-record storage read operation.
    template<class Record>
    struct StorageReadResult {
        StorageStatus status = StorageStatus::NOT_OPEN; ///< Operation status.
        Record record;                                  ///< Found record, when available.
        bool found = false;                             ///< True when record was found.
        std::string message;                            ///< Diagnostic message.

        /// \brief Returns true if the read operation succeeded and found a record.
        bool ok() const noexcept {
            return status == StorageStatus::SUCCESS && found;
        }

        /// \brief Returns true if the read operation succeeded and found a record.
        explicit operator bool() const noexcept {
            return ok();
        }
    };

    /// \struct StorageListResult
    /// \brief Result of a multi-record storage read operation.
    template<class Record>
    struct StorageListResult {
        StorageStatus status = StorageStatus::NOT_OPEN; ///< Operation status.
        std::vector<Record> records;                    ///< Found records.
        std::string message;                            ///< Diagnostic message.

        /// \brief Returns true if the list operation completed successfully.
        bool ok() const noexcept {
            return status == StorageStatus::SUCCESS;
        }

        /// \brief Returns true if the list operation completed successfully.
        explicit operator bool() const noexcept {
            return ok();
        }
    };

} // namespace optionx::storage

#endif // OPTIONX_HEADER_STORAGES_COMMON_STORAGE_RESULT_HPP_INCLUDED
