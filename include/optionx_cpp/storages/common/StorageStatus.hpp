#pragma once
#ifndef _OPTIONX_STORAGE_COMMON_STORAGE_STATUS_HPP_INCLUDED
#define _OPTIONX_STORAGE_COMMON_STORAGE_STATUS_HPP_INCLUDED

/// \file StorageStatus.hpp
/// \brief Defines common storage operation status codes.

namespace optionx::storage {

    /// \enum StorageStatus
    /// \brief Status codes returned by storage services.
    enum class StorageStatus {
        SUCCESS,
        NOT_FOUND,
        INVALID_ARGUMENT,
        NOT_OPEN,
        READ_ONLY,
        QUEUE_CLOSED,
        DB_ERROR,
        STORAGE_ERROR = DB_ERROR,
        SEQUENCE_EXHAUSTED
    };

} // namespace optionx::storage

#endif // _OPTIONX_STORAGE_COMMON_STORAGE_STATUS_HPP_INCLUDED
