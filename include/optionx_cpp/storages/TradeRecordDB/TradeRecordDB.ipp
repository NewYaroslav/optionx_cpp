#pragma once
#ifndef _OPTIONX_STORAGE_TRADE_RECORD_DB_IPP_INCLUDED
#define _OPTIONX_STORAGE_TRADE_RECORD_DB_IPP_INCLUDED

/// \file TradeRecordDB.ipp
/// \brief Inline implementation of TradeRecordDB.

#include <algorithm>
#include <exception>
#include <limits>
#include <utility>

#include "optionx_cpp/utils.hpp"

namespace optionx::storage {

    inline mdbxc::Config TradeRecordDB::default_config() {
        mdbxc::Config config;
        config.pathname = OPTIONX_TRADE_RECORD_DB_FILE;
        config.max_dbs = 3;
        config.no_subdir = false;
        config.relative_to_exe = true;
        return config;
    }

    inline TradeRecordDB::TradeRecordDB()
        : TradeRecordDB(default_config()) {}

    inline TradeRecordDB::TradeRecordDB(
        mdbxc::Config config,
        std::string records_table,
        std::string uid_index_table,
        std::string meta_table)
        : m_config(std::move(config)),
          m_records_table_name(std::move(records_table)),
          m_uid_index_table_name(std::move(uid_index_table)),
          m_meta_table_name(std::move(meta_table)) {
        open();
    }

    inline TradeRecordDB::~TradeRecordDB() {
        shutdown();
    }

    inline bool TradeRecordDB::is_open() const {
        std::lock_guard<std::mutex> lock(m_db_mutex);
        return is_open_no_lock();
    }

    inline std::uint64_t TradeRecordDB::get_trade_id() {
        std::lock_guard<std::mutex> lock(m_db_mutex);
        try {
            if (!is_open_no_lock()) return 0;
            if (m_read_only) return 0;

            auto txn = m_connection->transaction(mdbxc::TransactionMode::WRITABLE);
            const auto trade_id = reserve_trade_id_no_lock(txn.handle());
            if (trade_id == 0) return 0;
            update_last_update_no_lock(txn.handle());
            txn.commit();
            return trade_id;
        } catch (const mdbxc::MdbxException& ex) {
            LOGIT_PRINT_ERROR("TradeRecordDB trade_id database error: ", ex);
        } catch (const std::exception& ex) {
            LOGIT_PRINT_ERROR("TradeRecordDB trade_id error: ", ex);
        }
        return 0;
    }

    inline bool TradeRecordDB::assign_trade_id(TradeRequest& request) {
        if (request.trade_id > 0) return true;
        request.trade_id = get_trade_id();
        return request.trade_id > 0;
    }

    inline std::int64_t TradeRecordDB::get_trade_uid() {
        const auto trade_id = get_trade_id();
        if (trade_id > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
            return 0;
        }
        return static_cast<std::int64_t>(trade_id);
    }

    inline bool TradeRecordDB::assign_trade_uid(TradeRequest& request) {
        return assign_trade_id(request);
    }

    inline TradeRecordDBWriteResult TradeRecordDB::upsert(TradeRecord record) {
        std::lock_guard<std::mutex> lock(m_db_mutex);
        try {
            return upsert_no_lock(std::move(record));
        } catch (const mdbxc::MdbxException& ex) {
            LOGIT_PRINT_ERROR("TradeRecordDB upsert database error: ", ex);
            return trade_record_db_detail::write_error(TradeRecordDBStatus::DB_ERROR, std::move(record), ex.what());
        } catch (const std::exception& ex) {
            LOGIT_PRINT_ERROR("TradeRecordDB upsert error: ", ex);
            return trade_record_db_detail::write_error(TradeRecordDBStatus::DB_ERROR, std::move(record), ex.what());
        }
    }

    inline TradeRecordDBWriteResult TradeRecordDB::write(TradeRecord record) {
        std::lock_guard<std::mutex> lock(m_db_mutex);
        try {
            return write_no_lock(std::move(record));
        } catch (const mdbxc::MdbxException& ex) {
            LOGIT_PRINT_ERROR("TradeRecordDB write database error: ", ex);
            return trade_record_db_detail::write_error(TradeRecordDBStatus::DB_ERROR, std::move(record), ex.what());
        } catch (const std::exception& ex) {
            LOGIT_PRINT_ERROR("TradeRecordDB write error: ", ex);
            return trade_record_db_detail::write_error(TradeRecordDBStatus::DB_ERROR, std::move(record), ex.what());
        }
    }

    inline TradeRecordDBReadResult TradeRecordDB::find(std::uint64_t record_id) const {
        std::lock_guard<std::mutex> lock(m_db_mutex);
        try {
            return find_no_lock(record_id);
        } catch (const mdbxc::MdbxException& ex) {
            LOGIT_PRINT_ERROR("TradeRecordDB find database error: ", ex);
            return trade_record_db_detail::read_error(TradeRecordDBStatus::DB_ERROR, ex.what());
        } catch (const std::exception& ex) {
            LOGIT_PRINT_ERROR("TradeRecordDB find error: ", ex);
            return trade_record_db_detail::read_error(TradeRecordDBStatus::DB_ERROR, ex.what());
        }
    }

    inline TradeRecordDBReadResult TradeRecordDB::find_by_uid(std::int64_t request_unique_id) const {
        std::lock_guard<std::mutex> lock(m_db_mutex);
        try {
            return find_by_uid_no_lock(request_unique_id);
        } catch (const mdbxc::MdbxException& ex) {
            LOGIT_PRINT_ERROR("TradeRecordDB find_by_uid database error: ", ex);
            return trade_record_db_detail::read_error(TradeRecordDBStatus::DB_ERROR, ex.what());
        } catch (const std::exception& ex) {
            LOGIT_PRINT_ERROR("TradeRecordDB find_by_uid error: ", ex);
            return trade_record_db_detail::read_error(TradeRecordDBStatus::DB_ERROR, ex.what());
        }
    }

    inline TradeRecordDBListResult TradeRecordDB::find_by_timestamp(std::int64_t timestamp_ms) const {
        std::lock_guard<std::mutex> lock(m_db_mutex);
        try {
            return find_by_timestamp_no_lock(timestamp_ms);
        } catch (const mdbxc::MdbxException& ex) {
            LOGIT_PRINT_ERROR("TradeRecordDB find_by_timestamp database error: ", ex);
            return trade_record_db_detail::list_error(TradeRecordDBStatus::DB_ERROR, ex.what());
        } catch (const std::exception& ex) {
            LOGIT_PRINT_ERROR("TradeRecordDB find_by_timestamp error: ", ex);
            return trade_record_db_detail::list_error(TradeRecordDBStatus::DB_ERROR, ex.what());
        }
    }

    inline TradeRecordDBListResult TradeRecordDB::find_range(std::int64_t start_ms, std::int64_t stop_ms) const {
        std::lock_guard<std::mutex> lock(m_db_mutex);
        try {
            return find_range_no_lock(start_ms, stop_ms);
        } catch (const mdbxc::MdbxException& ex) {
            LOGIT_PRINT_ERROR("TradeRecordDB find_range database error: ", ex);
            return trade_record_db_detail::list_error(TradeRecordDBStatus::DB_ERROR, ex.what());
        } catch (const std::exception& ex) {
            LOGIT_PRINT_ERROR("TradeRecordDB find_range error: ", ex);
            return trade_record_db_detail::list_error(TradeRecordDBStatus::DB_ERROR, ex.what());
        }
    }

    inline TradeRecordDBStatus TradeRecordDB::erase(std::uint64_t record_id) {
        std::lock_guard<std::mutex> lock(m_db_mutex);
        try {
            return erase_no_lock(record_id);
        } catch (const mdbxc::MdbxException& ex) {
            LOGIT_PRINT_ERROR("TradeRecordDB erase database error: ", ex);
        } catch (const std::exception& ex) {
            LOGIT_PRINT_ERROR("TradeRecordDB erase error: ", ex);
        }
        return TradeRecordDBStatus::DB_ERROR;
    }

    inline TradeRecordDBStatus TradeRecordDB::clear() {
        std::lock_guard<std::mutex> lock(m_db_mutex);
        try {
            return clear_no_lock();
        } catch (const mdbxc::MdbxException& ex) {
            LOGIT_PRINT_ERROR("TradeRecordDB clear database error: ", ex);
        } catch (const std::exception& ex) {
            LOGIT_PRINT_ERROR("TradeRecordDB clear error: ", ex);
        }
        return TradeRecordDBStatus::DB_ERROR;
    }

    inline std::size_t TradeRecordDB::count() const {
        std::lock_guard<std::mutex> lock(m_db_mutex);
        try {
            if (!is_open_no_lock()) return 0;
            return m_records->count();
        } catch (const mdbxc::MdbxException& ex) {
            LOGIT_PRINT_ERROR("TradeRecordDB count database error: ", ex);
        } catch (const std::exception& ex) {
            LOGIT_PRINT_ERROR("TradeRecordDB count error: ", ex);
        }
        return 0;
    }

    inline TradeRecordDBStatus TradeRecordDB::enqueue_upsert(TradeRecord record, write_callback_t callback) {
        return enqueue_command([this, record = std::move(record), callback = std::move(callback)]() mutable {
            auto result = upsert(std::move(record));
            enqueue_callback([callback = std::move(callback), result = std::move(result)]() mutable {
                if (callback) callback(std::move(result));
            });
        });
    }

    inline TradeRecordDBStatus TradeRecordDB::enqueue_write(TradeRecord record, write_callback_t callback) {
        return enqueue_command([this, record = std::move(record), callback = std::move(callback)]() mutable {
            auto result = write(std::move(record));
            enqueue_callback([callback = std::move(callback), result = std::move(result)]() mutable {
                if (callback) callback(std::move(result));
            });
        });
    }

    inline TradeRecordDBStatus TradeRecordDB::enqueue_find(std::uint64_t record_id, read_callback_t callback) {
        return enqueue_command([this, record_id, callback = std::move(callback)]() mutable {
            auto result = find(record_id);
            enqueue_callback([callback = std::move(callback), result = std::move(result)]() mutable {
                if (callback) callback(std::move(result));
            });
        });
    }

    inline TradeRecordDBStatus TradeRecordDB::enqueue_find_by_uid(
        std::int64_t request_unique_id,
        read_callback_t callback) {
        return enqueue_command([this, request_unique_id, callback = std::move(callback)]() mutable {
            auto result = find_by_uid(request_unique_id);
            enqueue_callback([callback = std::move(callback), result = std::move(result)]() mutable {
                if (callback) callback(std::move(result));
            });
        });
    }

    inline TradeRecordDBStatus TradeRecordDB::enqueue_find_by_timestamp(
        std::int64_t timestamp_ms,
        list_callback_t callback) {
        return enqueue_command([this, timestamp_ms, callback = std::move(callback)]() mutable {
            auto result = find_by_timestamp(timestamp_ms);
            enqueue_callback([callback = std::move(callback), result = std::move(result)]() mutable {
                if (callback) callback(std::move(result));
            });
        });
    }

    inline TradeRecordDBStatus TradeRecordDB::enqueue_find_range(
        std::int64_t start_ms,
        std::int64_t stop_ms,
        list_callback_t callback) {
        return enqueue_command([this, start_ms, stop_ms, callback = std::move(callback)]() mutable {
            auto result = find_range(start_ms, stop_ms);
            enqueue_callback([callback = std::move(callback), result = std::move(result)]() mutable {
                if (callback) callback(std::move(result));
            });
        });
    }

    inline TradeRecordDBStatus TradeRecordDB::enqueue_erase(std::uint64_t record_id, status_callback_t callback) {
        return enqueue_command([this, record_id, callback = std::move(callback)]() mutable {
            auto status = erase(record_id);
            enqueue_callback([callback = std::move(callback), status]() mutable {
                if (callback) callback(status);
            });
        });
    }

    inline TradeRecordDBStatus TradeRecordDB::enqueue_clear(status_callback_t callback) {
        return enqueue_command([this, callback = std::move(callback)]() mutable {
            auto status = clear();
            enqueue_callback([callback = std::move(callback), status]() mutable {
                if (callback) callback(status);
            });
        });
    }

    inline bool TradeRecordDB::run() {
        if (!is_open()) return false;
        {
            std::lock_guard<std::mutex> lock(m_queue_mutex);
            if (!m_accepting || m_worker_running) return false;
            m_worker_stop = false;
            m_worker_running = true;
        }

        try {
            m_worker_thread = std::thread([this]() { worker_loop(); });
        } catch (const std::exception& ex) {
            LOGIT_PRINT_ERROR("TradeRecordDB worker start error: ", ex);
            std::lock_guard<std::mutex> lock(m_queue_mutex);
            m_worker_running = false;
            return false;
        }
        return true;
    }

    inline void TradeRecordDB::process() {
        if (!worker_running()) {
            drain_work_on_caller();
        }
        deliver_callbacks();
    }

    inline void TradeRecordDB::flush() {
        if (worker_running()) {
            std::unique_lock<std::mutex> lock(m_queue_mutex);
            m_idle_cv.wait(lock, [this]() {
                return m_work_queue.empty() && m_active_work == 0;
            });
        } else {
            drain_work_on_caller();
        }
        deliver_callbacks();
    }

    inline void TradeRecordDB::shutdown() {
        {
            std::lock_guard<std::mutex> lock(m_queue_mutex);
            if (!m_accepting && !m_worker_running && !m_open) return;
            m_accepting = false;
            m_worker_stop = true;
        }
        m_work_cv.notify_all();

        if (m_worker_thread.joinable()) {
            m_worker_thread.join();
        }
        {
            std::lock_guard<std::mutex> lock(m_queue_mutex);
            m_worker_running = false;
        }

        drain_work_on_caller();
        deliver_callbacks();

        std::lock_guard<std::mutex> lock(m_db_mutex);
        try {
            m_records.reset();
            m_uid_index.reset();
            m_meta.reset();
            if (m_connection && m_connection->is_connected()) {
                m_connection->disconnect();
            }
            m_connection.reset();
        } catch (const mdbxc::MdbxException& ex) {
            LOGIT_PRINT_ERROR("TradeRecordDB shutdown database error: ", ex);
        } catch (const std::exception& ex) {
            LOGIT_PRINT_ERROR("TradeRecordDB shutdown error: ", ex);
        }
        m_open = false;
    }

    inline void TradeRecordDB::open() {
        std::lock_guard<std::mutex> lock(m_db_mutex);
        try {
            if (m_config.max_dbs < 3) {
                m_config.max_dbs = 3;
            }
            m_read_only = m_config.read_only;
            m_connection = mdbxc::Connection::create(m_config);
            m_records = std::make_unique<records_table_t>(m_connection, m_records_table_name);
            m_uid_index = std::make_unique<uid_index_table_t>(m_connection, m_uid_index_table_name);
            m_meta = std::make_unique<meta_table_t>(m_connection, m_meta_table_name);
            m_open = true;

            if (!m_read_only) {
                auto txn = m_connection->transaction(mdbxc::TransactionMode::WRITABLE);
                init_meta_no_lock(txn.handle());
                txn.commit();
            }
        } catch (const mdbxc::MdbxException& ex) {
            LOGIT_PRINT_ERROR("TradeRecordDB connection error: ", ex);
            m_open = false;
        } catch (const std::exception& ex) {
            LOGIT_PRINT_ERROR("TradeRecordDB initialization error: ", ex);
            m_open = false;
        }
    }

    inline bool TradeRecordDB::is_open_no_lock() const noexcept {
        return m_open && m_connection && m_connection->is_connected() &&
               m_records && m_uid_index && m_meta;
    }

    inline std::uint64_t TradeRecordDB::get_meta_uint_no_lock(
        const std::string& key,
        std::uint64_t fallback,
        MDBX_txn* txn) const {
        const auto value = m_meta->find(key, txn);
        if (!value) return fallback;
        return trade_record_db_detail::parse_uint_or(*value, fallback);
    }

    inline void TradeRecordDB::init_meta_no_lock(MDBX_txn* txn) {
        if (!m_meta->contains(kMetaDbVersion, txn)) {
            m_meta->insert_or_assign(kMetaDbVersion, kMetaDbVersionValue, txn);
        }
        if (!m_meta->contains(kMetaNextTradeUid, txn)) {
            m_meta->insert_or_assign(kMetaNextTradeUid, std::string("1"), txn);
        }
        if (!m_meta->contains(kMetaLastUpdateMs, txn)) {
            m_meta->insert_or_assign(kMetaLastUpdateMs, std::to_string(trade_record_db_detail::now_ms()), txn);
        }
    }

    inline void TradeRecordDB::update_last_update_no_lock(MDBX_txn* txn) {
        m_meta->insert_or_assign(kMetaLastUpdateMs, std::to_string(trade_record_db_detail::now_ms()), txn);
    }

    inline std::uint64_t TradeRecordDB::reserve_trade_id_no_lock(MDBX_txn* txn) {
        const auto max_id = std::numeric_limits<std::uint64_t>::max();
        auto candidate = get_meta_uint_no_lock(kMetaNextTradeUid, 1, txn);
        if (candidate == 0) candidate = 1;

        while (candidate != 0 && candidate != max_id && m_records->contains(candidate, txn)) {
            ++candidate;
        }
        if (candidate == 0 || candidate == max_id) return 0;

        m_meta->insert_or_assign(kMetaNextTradeUid, std::to_string(candidate + 1), txn);
        return candidate;
    }

    inline void TradeRecordDB::bump_next_trade_id_no_lock(std::uint64_t used_trade_id, MDBX_txn* txn) {
        if (used_trade_id == 0 || used_trade_id == std::numeric_limits<std::uint64_t>::max()) return;

        auto next_trade_id = get_meta_uint_no_lock(kMetaNextTradeUid, 1, txn);
        if (next_trade_id == 0) next_trade_id = 1;
        if (next_trade_id <= used_trade_id) {
            m_meta->insert_or_assign(kMetaNextTradeUid, std::to_string(used_trade_id + 1), txn);
        }
    }

    inline TradeRecordDBWriteResult TradeRecordDB::write_no_lock(TradeRecord record) {
        if (!is_open_no_lock()) {
            return trade_record_db_detail::write_error(
                TradeRecordDBStatus::NOT_OPEN,
                std::move(record),
                "TradeRecordDB is not open");
        }
        if (m_read_only) {
            return trade_record_db_detail::write_error(
                TradeRecordDBStatus::READ_ONLY,
                std::move(record),
                "TradeRecordDB is read-only");
        }
        if (record.record_id == 0 && record.trade_id != 0) {
            record.record_id = record.trade_id;
        }
        if (record.trade_id == 0 && record.record_id != 0) {
            record.trade_id = record.record_id;
        }
        if (record.record_id != 0) {
            record.trade_id = record.record_id;
        }
        if (record.record_id == 0) {
            return trade_record_db_detail::write_error(
                TradeRecordDBStatus::INVALID_ARGUMENT,
                std::move(record),
                "TradeRecord record_id is required");
        }

        auto txn = m_connection->transaction(mdbxc::TransactionMode::WRITABLE);
        const auto existing = m_records->find(record.record_id, txn);
        if (existing && existing->request_unique_id > 0 &&
            existing->request_unique_id != record.request_unique_id) {
            m_uid_index->erase(existing->request_unique_id, txn);
        }

        m_records->insert_or_assign(record.record_id, record, txn);
        if (record.request_unique_id > 0) {
            m_uid_index->insert_or_assign(record.request_unique_id, record.record_id, txn);
        }
        bump_next_trade_id_no_lock(record.record_id, txn.handle());
        update_last_update_no_lock(txn.handle());
        txn.commit();

        return trade_record_db_detail::write_success(std::move(record));
    }

    inline TradeRecordDBWriteResult TradeRecordDB::upsert_no_lock(TradeRecord record) {
        if (!is_open_no_lock()) {
            return trade_record_db_detail::write_error(
                TradeRecordDBStatus::NOT_OPEN,
                std::move(record),
                "TradeRecordDB is not open");
        }
        if (m_read_only) {
            return trade_record_db_detail::write_error(
                TradeRecordDBStatus::READ_ONLY,
                std::move(record),
                "TradeRecordDB is read-only");
        }

        auto txn = m_connection->transaction(mdbxc::TransactionMode::WRITABLE);

        std::uint64_t selected_key = record.record_id;
        if (selected_key == 0 && record.trade_id != 0) {
            selected_key = record.trade_id;
        }

        if (selected_key == 0 && record.request_unique_id > 0) {
            const auto indexed_key = m_uid_index->find(record.request_unique_id, txn);
            if (indexed_key) {
                const auto indexed_record = m_records->find(*indexed_key, txn);
                if (indexed_record) {
                    selected_key = *indexed_key;
                } else {
                    m_uid_index->erase(record.request_unique_id, txn);
                }
            }
        }

        if (selected_key == 0 && record.has_broker_identity()) {
            selected_key = find_broker_identity_key_no_lock(record, txn.handle());
        }

        if (selected_key == 0) {
            selected_key = reserve_trade_id_no_lock(txn.handle());
            if (selected_key == 0) {
                return trade_record_db_detail::write_error(
                    TradeRecordDBStatus::DB_ERROR,
                    std::move(record),
                    "TradeRecordDB could not reserve a trade_id");
            }
        }

        record.record_id = selected_key;
        record.trade_id = selected_key;

        const auto existing_at_target = m_records->find(selected_key, txn);
        if (existing_at_target && existing_at_target->request_unique_id > 0 &&
            existing_at_target->request_unique_id != record.request_unique_id) {
            m_uid_index->erase(existing_at_target->request_unique_id, txn);
        }

        m_records->insert_or_assign(record.record_id, record, txn);
        if (record.request_unique_id > 0) {
            m_uid_index->insert_or_assign(record.request_unique_id, record.record_id, txn);
        }
        bump_next_trade_id_no_lock(record.record_id, txn.handle());
        update_last_update_no_lock(txn.handle());
        txn.commit();

        return trade_record_db_detail::write_success(std::move(record));
    }

    inline TradeRecordDBReadResult TradeRecordDB::find_no_lock(std::uint64_t record_id) const {
        if (!is_open_no_lock()) {
            return trade_record_db_detail::read_error(TradeRecordDBStatus::NOT_OPEN, "TradeRecordDB is not open");
        }
        if (record_id == 0) {
            return trade_record_db_detail::read_error(TradeRecordDBStatus::INVALID_ARGUMENT, "TradeRecord record_id is required");
        }

        auto txn = m_connection->transaction(mdbxc::TransactionMode::READ_ONLY);
        const auto record = m_records->find(record_id, txn);
        txn.commit();
        if (!record) {
            return trade_record_db_detail::read_error(TradeRecordDBStatus::NOT_FOUND, "TradeRecord was not found");
        }

        TradeRecordDBReadResult result;
        result.status = TradeRecordDBStatus::SUCCESS;
        result.record = *record;
        result.found = true;
        return result;
    }

    inline TradeRecordDBReadResult TradeRecordDB::find_by_uid_no_lock(std::int64_t request_unique_id) const {
        if (!is_open_no_lock()) {
            return trade_record_db_detail::read_error(TradeRecordDBStatus::NOT_OPEN, "TradeRecordDB is not open");
        }
        if (request_unique_id <= 0) {
            return trade_record_db_detail::read_error(
                TradeRecordDBStatus::INVALID_ARGUMENT,
                "TradeRecord request_unique_id is required");
        }

        auto txn = m_connection->transaction(mdbxc::TransactionMode::READ_ONLY);
        const auto indexed_key = m_uid_index->find(request_unique_id, txn);
        if (!indexed_key) {
            txn.commit();
            return trade_record_db_detail::read_error(
                TradeRecordDBStatus::NOT_FOUND,
                "TradeRecord UID index entry was not found");
        }

        const auto record = m_records->find(*indexed_key, txn);
        txn.commit();
        if (!record) {
            return trade_record_db_detail::read_error(
                TradeRecordDBStatus::NOT_FOUND,
                "TradeRecord UID index points to missing record");
        }
        if (record->request_unique_id != request_unique_id) {
            return trade_record_db_detail::read_error(
                TradeRecordDBStatus::NOT_FOUND,
                "TradeRecord UID index points to a different request_unique_id");
        }

        TradeRecordDBReadResult result;
        result.status = TradeRecordDBStatus::SUCCESS;
        result.record = *record;
        result.found = true;
        return result;
    }

    inline TradeRecordDBListResult TradeRecordDB::find_by_timestamp_no_lock(std::int64_t timestamp_ms) const {
        if (!is_open_no_lock()) {
            return trade_record_db_detail::list_error(TradeRecordDBStatus::NOT_OPEN, "TradeRecordDB is not open");
        }
        if (timestamp_ms < 0) {
            return trade_record_db_detail::list_error(TradeRecordDBStatus::INVALID_ARGUMENT, "TradeRecord timestamp is invalid");
        }

        auto txn = m_connection->transaction(mdbxc::TransactionMode::READ_ONLY);
        std::vector<std::pair<std::uint64_t, TradeRecord>> rows;
        m_records->load(rows, txn);
        txn.commit();

        TradeRecordDBListResult result;
        result.status = TradeRecordDBStatus::SUCCESS;
        for (const auto& row : rows) {
            if (trade_record_db_detail::selected_timestamp_ms(row.second) == timestamp_ms) {
                result.records.push_back(row.second);
            }
        }
        std::sort(result.records.begin(), result.records.end(), [](const TradeRecord& lhs, const TradeRecord& rhs) {
            const auto lhs_timestamp = trade_record_db_detail::selected_timestamp_ms(lhs);
            const auto rhs_timestamp = trade_record_db_detail::selected_timestamp_ms(rhs);
            if (lhs_timestamp != rhs_timestamp) return lhs_timestamp < rhs_timestamp;
            return lhs.record_id < rhs.record_id;
        });
        return result;
    }

    inline TradeRecordDBListResult TradeRecordDB::find_range_no_lock(std::int64_t start_ms, std::int64_t stop_ms) const {
        if (!is_open_no_lock()) {
            return trade_record_db_detail::list_error(TradeRecordDBStatus::NOT_OPEN, "TradeRecordDB is not open");
        }
        if (start_ms < 0 || stop_ms < 0 || start_ms > stop_ms) {
            return trade_record_db_detail::list_error(TradeRecordDBStatus::INVALID_ARGUMENT, "TradeRecord timestamp range is invalid");
        }
        auto txn = m_connection->transaction(mdbxc::TransactionMode::READ_ONLY);
        std::vector<std::pair<std::uint64_t, TradeRecord>> rows;
        m_records->load(rows, txn);
        txn.commit();

        TradeRecordDBListResult result;
        result.status = TradeRecordDBStatus::SUCCESS;
        for (const auto& row : rows) {
            const auto timestamp = trade_record_db_detail::selected_timestamp_ms(row.second);
            if (timestamp >= start_ms && timestamp <= stop_ms) {
                result.records.push_back(row.second);
            }
        }
        std::sort(result.records.begin(), result.records.end(), [](const TradeRecord& lhs, const TradeRecord& rhs) {
            const auto lhs_timestamp = trade_record_db_detail::selected_timestamp_ms(lhs);
            const auto rhs_timestamp = trade_record_db_detail::selected_timestamp_ms(rhs);
            if (lhs_timestamp != rhs_timestamp) return lhs_timestamp < rhs_timestamp;
            return lhs.record_id < rhs.record_id;
        });
        return result;
    }

    inline TradeRecordDBStatus TradeRecordDB::erase_no_lock(std::uint64_t record_id) {
        if (!is_open_no_lock()) return TradeRecordDBStatus::NOT_OPEN;
        if (m_read_only) return TradeRecordDBStatus::READ_ONLY;
        if (record_id == 0) return TradeRecordDBStatus::INVALID_ARGUMENT;

        auto txn = m_connection->transaction(mdbxc::TransactionMode::WRITABLE);
        const auto record = m_records->find(record_id, txn);
        if (!record) {
            txn.commit();
            return TradeRecordDBStatus::NOT_FOUND;
        }
        if (record->request_unique_id > 0) {
            m_uid_index->erase(record->request_unique_id, txn);
        }
        m_records->erase(record_id, txn);
        update_last_update_no_lock(txn.handle());
        txn.commit();
        return TradeRecordDBStatus::SUCCESS;
    }

    inline TradeRecordDBStatus TradeRecordDB::clear_no_lock() {
        if (!is_open_no_lock()) return TradeRecordDBStatus::NOT_OPEN;
        if (m_read_only) return TradeRecordDBStatus::READ_ONLY;

        auto txn = m_connection->transaction(mdbxc::TransactionMode::WRITABLE);
        m_records->clear(txn);
        m_uid_index->clear(txn);
        m_meta->clear(txn);
        init_meta_no_lock(txn.handle());
        update_last_update_no_lock(txn.handle());
        txn.commit();
        return TradeRecordDBStatus::SUCCESS;
    }

    inline std::uint64_t TradeRecordDB::find_broker_identity_key_no_lock(
        const TradeRecord& record,
        MDBX_txn* txn) const {
        std::vector<std::pair<std::uint64_t, TradeRecord>> rows;
        m_records->load(rows, txn);
        for (const auto& row : rows) {
            if (record.same_broker_identity(row.second)) {
                return row.first;
            }
        }
        return 0;
    }

    inline bool TradeRecordDB::worker_running() const {
        std::lock_guard<std::mutex> lock(m_queue_mutex);
        return m_worker_running;
    }

    inline TradeRecordDBStatus TradeRecordDB::enqueue_command(std::function<void()> command) {
        {
            std::lock_guard<std::mutex> lock(m_queue_mutex);
            if (!m_accepting) return TradeRecordDBStatus::QUEUE_CLOSED;
            m_work_queue.push_back(std::move(command));
        }
        m_work_cv.notify_one();
        return TradeRecordDBStatus::SUCCESS;
    }

    inline void TradeRecordDB::enqueue_callback(std::function<void()> callback) {
        std::lock_guard<std::mutex> lock(m_callback_mutex);
        m_callback_queue.push_back(std::move(callback));
    }

    inline bool TradeRecordDB::pop_command(std::function<void()>& command) {
        std::lock_guard<std::mutex> lock(m_queue_mutex);
        if (m_work_queue.empty()) return false;
        command = std::move(m_work_queue.front());
        m_work_queue.pop_front();
        ++m_active_work;
        return true;
    }

    inline void TradeRecordDB::finish_command() {
        std::lock_guard<std::mutex> lock(m_queue_mutex);
        if (m_active_work > 0) --m_active_work;
        if (m_work_queue.empty() && m_active_work == 0) {
            m_idle_cv.notify_all();
        }
    }

    inline void TradeRecordDB::execute_command(std::function<void()> command) {
        try {
            if (command) command();
        } catch (const std::exception& ex) {
            LOGIT_PRINT_ERROR("TradeRecordDB queued command error: ", ex);
        } catch (...) {
            LOGIT_PRINT_ERROR("TradeRecordDB queued command unknown error");
        }
        finish_command();
    }

    inline void TradeRecordDB::drain_work_on_caller() {
        std::function<void()> command;
        while (pop_command(command)) {
            execute_command(std::move(command));
        }
    }

    inline void TradeRecordDB::deliver_callbacks() {
        for (;;) {
            std::function<void()> callback;
            {
                std::lock_guard<std::mutex> lock(m_callback_mutex);
                if (m_callback_queue.empty()) break;
                callback = std::move(m_callback_queue.front());
                m_callback_queue.pop_front();
            }
            try {
                if (callback) callback();
            } catch (const std::exception& ex) {
                LOGIT_PRINT_ERROR("TradeRecordDB callback error: ", ex);
            } catch (...) {
                LOGIT_PRINT_ERROR("TradeRecordDB callback unknown error");
            }
        }
    }

    inline void TradeRecordDB::worker_loop() {
        for (;;) {
            std::function<void()> command;
            {
                std::unique_lock<std::mutex> lock(m_queue_mutex);
                m_work_cv.wait(lock, [this]() {
                    return m_worker_stop || !m_work_queue.empty();
                });
                if (m_worker_stop && m_work_queue.empty()) {
                    break;
                }
                command = std::move(m_work_queue.front());
                m_work_queue.pop_front();
                ++m_active_work;
            }
            execute_command(std::move(command));
        }

        std::lock_guard<std::mutex> lock(m_queue_mutex);
        m_worker_running = false;
        if (m_work_queue.empty() && m_active_work == 0) {
            m_idle_cv.notify_all();
        }
    }

} // namespace optionx::storage

#endif // _OPTIONX_STORAGE_TRADE_RECORD_DB_IPP_INCLUDED
