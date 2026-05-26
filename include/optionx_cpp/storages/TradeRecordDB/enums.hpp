#pragma once
#ifndef _OPTIONX_STORAGE_TRADE_RECORD_DB_ENUMS_HPP_INCLUDED
#define _OPTIONX_STORAGE_TRADE_RECORD_DB_ENUMS_HPP_INCLUDED

/// \file enums.hpp
/// \brief Defines TradeRecordDB status codes.

namespace optionx::storage {

    /// \enum TradeRecordDBStatus
    /// \brief Status codes returned by TradeRecordDB operations.
    enum class TradeRecordDBStatus {
        SUCCESS,
        NOT_FOUND,
        INVALID_ARGUMENT,
        NOT_OPEN,
        READ_ONLY,
        QUEUE_CLOSED,
        DB_ERROR,
        SEQUENCE_EXHAUSTED ///< Legacy timestamp-bucket status; linear TradeRecordDB does not return it.
    };

} // namespace optionx::storage

#endif // _OPTIONX_STORAGE_TRADE_RECORD_DB_ENUMS_HPP_INCLUDED
