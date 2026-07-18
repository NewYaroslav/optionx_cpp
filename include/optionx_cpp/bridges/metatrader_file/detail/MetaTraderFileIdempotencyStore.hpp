#pragma once
#ifndef OPTIONX_HEADER_BRIDGES_METATRADER_FILE_DETAIL_META_TRADER_FILE_IDEMPOTENCY_STORE_HPP_INCLUDED
#define OPTIONX_HEADER_BRIDGES_METATRADER_FILE_DETAIL_META_TRADER_FILE_IDEMPOTENCY_STORE_HPP_INCLUDED

/// \file MetaTraderFileIdempotencyStore.hpp
/// \brief Defines durable idempotency-state helpers for the MetaTrader file bridge.

namespace optionx::bridges::metatrader_file::detail {

    /// \struct IdempotencyRecord
    /// \brief Active operation registry entry retained for duplicate detection.
    struct IdempotencyRecord {
        std::string payload_fingerprint; ///< Canonical request payload fingerprint.
        nlohmann::json result; ///< Result to return for duplicate requests.
        std::uint64_t created_at_ms = 0; ///< Creation time in Unix milliseconds.
        std::uint64_t updated_at_ms = 0; ///< Last update time in Unix milliseconds.
    };

    /// \struct IdempotencyTombstone
    /// \brief Compact retained entry for evicted completed operations.
    struct IdempotencyTombstone {
        std::string payload_fingerprint; ///< Original request payload fingerprint.
        nlohmann::json result; ///< Original operation result retained for retry.
        std::uint64_t evicted_at_ms = 0; ///< Eviction time in Unix milliseconds.
    };

    /// \struct IdempotencyState
    /// \brief Complete durable idempotency registry snapshot.
    struct IdempotencyState {
        std::map<std::string, IdempotencyRecord> records; ///< Active records by method/key.
        std::map<std::string, IdempotencyTombstone> tombstones; ///< Evicted completed records.
        std::map<std::string, std::string> request_index; ///< JSON-RPC request ID to operation key.
        std::uint64_t processed_through_file_seq = 0; ///< Durable high-water command sequence.
    };

    /// \brief Reads a required non-negative integer JSON field.
    inline std::uint64_t required_idempotency_uint64_value(
            const nlohmann::json& object,
            const char* key,
            const std::filesystem::path& file,
            const bool allow_zero = true) {
        const auto it = object.find(key);
        if (it == object.end()) {
            throw std::runtime_error(
                "MetaTrader file bridge idempotency state is missing numeric field '" +
                std::string(key) + "': " + file.u8string());
        }
        std::uint64_t result = 0;
        if (it->is_number_unsigned()) {
            result = it->get<std::uint64_t>();
        } else if (it->is_number_integer()) {
            const auto value = it->get<std::int64_t>();
            if (value < 0) {
                throw std::runtime_error(
                    "MetaTrader file bridge idempotency state field '" +
                    std::string(key) + "' must be non-negative: " + file.u8string());
            }
            result = static_cast<std::uint64_t>(value);
        } else {
            throw std::runtime_error(
                "MetaTrader file bridge idempotency state field '" +
                std::string(key) + "' must be an integer: " + file.u8string());
        }
        if (!allow_zero && result == 0) {
            throw std::runtime_error(
                "MetaTrader file bridge idempotency state field '" +
                std::string(key) + "' must be positive: " + file.u8string());
        }
        return result;
    }

    /// \brief Reads an idempotency registry snapshot from JSON.
    inline IdempotencyState read_idempotency_state(
            const std::filesystem::path& file,
            const std::size_t max_state_bytes) {
        IdempotencyState state;

        std::error_code ec;
        const auto exists = std::filesystem::exists(file, ec);
        if (ec) {
            throw std::runtime_error("Failed to inspect idempotency state: " + ec.message());
        }
        if (!exists) {
            return state;
        }

        const auto parsed = read_json_file(file, max_state_bytes);
        if (!parsed.is_object()) {
            throw std::runtime_error(
                "MetaTrader file bridge idempotency state must be an object: " +
                file.u8string());
        }
        state.processed_through_file_seq =
            required_idempotency_uint64_value(
                parsed,
                "processed_through_file_seq",
                file);

        const auto it = parsed.find("records");
        if (it == parsed.end()) {
            return state;
        }
        if (!it->is_object()) {
            throw std::runtime_error(
                "MetaTrader file bridge idempotency records must be an object: " +
                file.u8string());
        }

        for (auto record_it = it->begin(); record_it != it->end(); ++record_it) {
            const auto& value = record_it.value();
            if (!value.is_object() ||
                !value.contains("payload_fingerprint") ||
                !value.at("payload_fingerprint").is_string() ||
                !value.contains("result")) {
                throw std::runtime_error(
                    "MetaTrader file bridge idempotency record is malformed: " +
                    file.u8string());
            }

            IdempotencyRecord record;
            record.payload_fingerprint = value.at("payload_fingerprint").get<std::string>();
            record.result = value.at("result");
            record.created_at_ms =
                required_idempotency_uint64_value(
                    value,
                    "created_at_ms",
                    file,
                    false);
            record.updated_at_ms =
                required_idempotency_uint64_value(
                    value,
                    "updated_at_ms",
                    file,
                    false);
            state.records.emplace(record_it.key(), std::move(record));
        }

        const auto tombstones_it = parsed.find("tombstones");
        if (tombstones_it != parsed.end()) {
            if (!tombstones_it->is_object()) {
                throw std::runtime_error(
                    "MetaTrader file bridge idempotency tombstones must be an object: " +
                    file.u8string());
            }
            for (auto tombstone_it = tombstones_it->begin();
                 tombstone_it != tombstones_it->end();
                 ++tombstone_it) {
                const auto& value = tombstone_it.value();
                if (!value.is_object() ||
                    !value.contains("payload_fingerprint") ||
                    !value.at("payload_fingerprint").is_string() ||
                    !value.contains("result")) {
                    throw std::runtime_error(
                        "MetaTrader file bridge idempotency tombstone is malformed: " +
                        file.u8string());
                }

                IdempotencyTombstone tombstone;
                tombstone.payload_fingerprint =
                    value.at("payload_fingerprint").get<std::string>();
                tombstone.result = value.at("result");
                tombstone.evicted_at_ms =
                    required_idempotency_uint64_value(
                        value,
                        "evicted_at_ms",
                        file,
                        false);
                state.tombstones.emplace(tombstone_it.key(), std::move(tombstone));
            }
        }

        const auto index_it = parsed.find("request_index");
        if (index_it != parsed.end()) {
            if (!index_it->is_object()) {
                throw std::runtime_error(
                    "MetaTrader file bridge idempotency request_index must be an object: " +
                    file.u8string());
            }
            for (auto request_it = index_it->begin();
                 request_it != index_it->end();
                 ++request_it) {
                if (!request_it.value().is_string()) {
                    throw std::runtime_error(
                        "MetaTrader file bridge idempotency request_index entry is malformed: " +
                        file.u8string());
                }
                state.request_index.emplace(
                    request_it.key(),
                    request_it.value().get<std::string>());
            }
        }
        return state;
    }

    /// \brief Builds an idempotency registry JSON document from in-memory maps.
    inline nlohmann::json make_idempotency_state_document(
            const std::map<std::string, IdempotencyRecord>& records_map,
            const std::map<std::string, IdempotencyTombstone>& tombstones_map,
            const std::map<std::string, std::string>& request_index_map,
            const std::uint64_t processed_through_file_seq) {
        nlohmann::json records = nlohmann::json::object();
        for (const auto& item : records_map) {
            records[item.first] = {
                {"payload_fingerprint", item.second.payload_fingerprint},
                {"result", item.second.result},
                {"created_at_ms", item.second.created_at_ms},
                {"updated_at_ms", item.second.updated_at_ms}
            };
        }
        nlohmann::json tombstones = nlohmann::json::object();
        for (const auto& item : tombstones_map) {
            tombstones[item.first] = {
                {"payload_fingerprint", item.second.payload_fingerprint},
                {"result", item.second.result},
                {"evicted_at_ms", item.second.evicted_at_ms}
            };
        }
        nlohmann::json request_index = nlohmann::json::object();
        for (const auto& item : request_index_map) {
            if (records_map.find(item.second) != records_map.end() ||
                tombstones_map.find(item.second) != tombstones_map.end()) {
                request_index[item.first] = item.second;
            }
        }
        return nlohmann::json{
            {"processed_through_file_seq", processed_through_file_seq},
            {"records", std::move(records)},
            {"tombstones", std::move(tombstones)},
            {"request_index", std::move(request_index)}
        };
    }

} // namespace optionx::bridges::metatrader_file::detail

#endif // OPTIONX_HEADER_BRIDGES_METATRADER_FILE_DETAIL_META_TRADER_FILE_IDEMPOTENCY_STORE_HPP_INCLUDED
