#pragma once
#ifndef OPTIONX_HEADER_BRIDGES_METATRADER_FILE_DETAIL_META_TRADER_FILE_PROTOCOL_HPP_INCLUDED
#define OPTIONX_HEADER_BRIDGES_METATRADER_FILE_DETAIL_META_TRADER_FILE_PROTOCOL_HPP_INCLUDED

/// \file MetaTraderFileProtocol.hpp
/// \brief File-drop transport helpers for MetaTrader common-files integrations.

#include "../MetaTraderFileBridgeConfig.hpp"
#include "data/trading.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace optionx::bridges::metatrader_file::detail {

    /// \struct FileTransportLayout
    /// \brief Resolved directories for one OptionX file-transport client.
    struct FileTransportLayout {
        std::filesystem::path root; ///< Client root directory.

        std::filesystem::path requests_ready() const { return root / "requests" / "ready"; }
        std::filesystem::path requests_processing() const { return root / "requests" / "processing"; }
        std::filesystem::path requests_archive() const { return root / "requests" / "archive"; }
        std::filesystem::path requests_errors() const { return root / "requests" / "errors"; }
        std::filesystem::path responses_ready() const { return root / "responses" / "ready"; }
        std::filesystem::path responses_processing() const { return root / "responses" / "processing"; }
        std::filesystem::path events_ready() const { return root / "events" / "ready"; }
        std::filesystem::path events_processing() const { return root / "events" / "processing"; }
        std::filesystem::path archive() const { return root / "archive"; }
        std::filesystem::path errors() const { return root / "errors"; }
        std::filesystem::path queue_state() const { return root / "events" / "queue-state.json"; }
    };

    /// \struct EventFileNameParts
    /// \brief Parsed ordered event-delivery filename parts.
    struct EventFileNameParts {
        std::string delivery_queue_id; ///< Active delivery queue identifier.
        std::uint64_t delivery_seq = 0; ///< Transport-level delivery sequence.
        std::string file_uuid; ///< Transport-level file identifier.
    };

    /// \brief Builds resolved directories from a bridge config.
    inline FileTransportLayout make_layout(const MetaTraderFileBridgeConfig& config) {
        return FileTransportLayout{config.client_root()};
    }

    /// \brief Returns all directories that should exist before polling/publishing.
    inline std::vector<std::filesystem::path> runtime_directories(
            const FileTransportLayout& layout) {
        return {
            layout.requests_ready(),
            layout.requests_processing(),
            layout.requests_archive(),
            layout.requests_errors(),
            layout.responses_ready(),
            layout.responses_processing(),
            layout.events_ready(),
            layout.events_processing(),
            layout.archive(),
            layout.errors()
        };
    }

    /// \brief Creates all runtime directories for a file-transport client.
    inline void ensure_runtime_directories(const FileTransportLayout& layout) {
        for (const auto& dir : runtime_directories(layout)) {
            std::error_code ec;
            std::filesystem::create_directories(dir, ec);
            if (ec) {
                throw std::runtime_error(
                    "Failed to create file transport directory: " +
                    dir.u8string() +
                    ": " +
                    ec.message());
            }
        }
    }

    /// \brief Returns true when a filename can be safely created by helpers.
    inline bool is_safe_transport_filename(const std::string& filename) noexcept {
        if (filename.empty() || filename == "." || filename == "..") return false;
        for (const unsigned char ch : filename) {
            const bool ok =
                (ch >= 'A' && ch <= 'Z') ||
                (ch >= 'a' && ch <= 'z') ||
                (ch >= '0' && ch <= '9') ||
                ch == '-' ||
                ch == '.' ||
                ch == '_';
            if (!ok) return false;
        }
        return true;
    }

    /// \brief Returns a zero-padded decimal delivery sequence.
    inline std::string format_delivery_seq(const std::uint64_t seq) {
        std::ostringstream out;
        out << std::setw(20) << std::setfill('0') << seq;
        return out.str();
    }

    /// \brief Builds the unordered request/response filename pattern.
    inline std::string make_message_filename(
            const std::int64_t unix_ms,
            const std::string& file_uuid) {
        if (unix_ms < 0) {
            throw std::invalid_argument("File transport unix_ms must not be negative.");
        }
        if (!is_safe_file_transport_id(file_uuid)) {
            throw std::invalid_argument("File transport file_uuid must match [A-Za-z0-9.-]+.");
        }
        return std::to_string(unix_ms) + "_" + file_uuid + ".json";
    }

    /// \brief Builds the ordered event filename pattern.
    inline std::string make_event_filename(
            const std::string& delivery_queue_id,
            const std::uint64_t delivery_seq,
            const std::string& file_uuid) {
        if (!is_safe_file_transport_id(delivery_queue_id)) {
            throw std::invalid_argument("File transport delivery_queue_id must match [A-Za-z0-9.-]+.");
        }
        if (!is_safe_file_transport_id(file_uuid)) {
            throw std::invalid_argument("File transport file_uuid must match [A-Za-z0-9.-]+.");
        }
        return delivery_queue_id + "_" + format_delivery_seq(delivery_seq) + "_" + file_uuid + ".json";
    }

    /// \brief Parses an ordered event-delivery filename.
    inline std::optional<EventFileNameParts> parse_event_filename(
            const std::string& filename) {
        const std::string suffix = ".json";
        if (filename.size() <= suffix.size() ||
            filename.substr(filename.size() - suffix.size()) != suffix) {
            return std::nullopt;
        }

        const auto first_sep = filename.find('_');
        const auto second_sep = filename.find('_', first_sep == std::string::npos ? 0 : first_sep + 1);
        if (first_sep == std::string::npos ||
            second_sep == std::string::npos ||
            filename.find('_', second_sep + 1) != std::string::npos) {
            return std::nullopt;
        }

        EventFileNameParts parts;
        parts.delivery_queue_id = filename.substr(0, first_sep);
        const auto seq_text = filename.substr(first_sep + 1, second_sep - first_sep - 1);
        parts.file_uuid = filename.substr(
            second_sep + 1,
            filename.size() - second_sep - 1 - suffix.size());

        if (!is_safe_file_transport_id(parts.delivery_queue_id) ||
            !is_safe_file_transport_id(parts.file_uuid) ||
            seq_text.size() != 20) {
            return std::nullopt;
        }
        for (const char ch : seq_text) {
            if (ch < '0' || ch > '9') return std::nullopt;
        }

        try {
            parts.delivery_seq = static_cast<std::uint64_t>(std::stoull(seq_text));
        } catch (...) {
            return std::nullopt;
        }
        return parts;
    }

    /// \brief Returns milliseconds since Unix epoch.
    inline std::int64_t unix_time_ms() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    /// \brief Writes a JSON document to `ready_dir/final_filename` via same-directory temp rename.
    inline std::filesystem::path publish_json_atomic(
            const std::filesystem::path& ready_dir,
            const std::string& final_filename,
            const nlohmann::json& document,
            const int indent = -1) {
        if (!is_safe_transport_filename(final_filename)) {
            throw std::invalid_argument("File transport final filename is not path-safe.");
        }

        std::error_code ec;
        std::filesystem::create_directories(ready_dir, ec);
        if (ec) {
            throw std::runtime_error("Failed to create ready directory: " + ec.message());
        }

        const auto final_path = ready_dir / final_filename;
        if (std::filesystem::exists(final_path, ec)) {
            throw std::runtime_error("File transport final file already exists: " + final_path.u8string());
        }

        const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto temp_path = ready_dir / (final_filename + ".tmp." + std::to_string(tick));
        {
            std::ofstream out(temp_path, std::ios::binary | std::ios::trunc);
            if (!out) {
                throw std::runtime_error("Failed to open temp file: " + temp_path.u8string());
            }
            out << document.dump(indent);
            out.flush();
            if (!out) {
                throw std::runtime_error("Failed to write temp file: " + temp_path.u8string());
            }
        }

        if (std::filesystem::exists(final_path, ec)) {
            std::filesystem::remove(temp_path, ec);
            throw std::runtime_error("File transport final file already exists: " + final_path.u8string());
        }

        std::filesystem::rename(temp_path, final_path, ec);
        if (ec) {
            std::error_code remove_ec;
            std::filesystem::remove(temp_path, remove_ec);
            throw std::runtime_error("Failed to publish file transport JSON: " + ec.message());
        }
        return final_path;
    }

    /// \brief Claims a ready file by moving it into a processing directory.
    inline bool claim_ready_file(
            const std::filesystem::path& ready_file,
            const std::filesystem::path& processing_dir,
            std::filesystem::path& claimed_path) {
        const auto filename = ready_file.filename().u8string();
        if (!is_safe_transport_filename(filename)) {
            return false;
        }

        std::error_code ec;
        if (!std::filesystem::exists(ready_file, ec)) {
            return false;
        }
        std::filesystem::create_directories(processing_dir, ec);
        if (ec) return false;

        claimed_path = processing_dir / filename;
        if (std::filesystem::exists(claimed_path, ec)) {
            return false;
        }
        std::filesystem::rename(ready_file, claimed_path, ec);
        return !ec;
    }

    /// \brief Reads and parses a JSON file with a maximum byte limit.
    inline nlohmann::json read_json_file(
            const std::filesystem::path& file,
            const std::size_t max_bytes) {
        std::error_code ec;
        const auto size = std::filesystem::file_size(file, ec);
        if (ec) {
            throw std::runtime_error("Failed to read JSON file size: " + ec.message());
        }
        if (size > max_bytes) {
            throw std::runtime_error("File transport JSON exceeds configured byte limit.");
        }

        std::ifstream in(file, std::ios::binary);
        if (!in) {
            throw std::runtime_error("Failed to open JSON file: " + file.u8string());
        }
        std::ostringstream buffer;
        buffer << in.rdbuf();
        return nlohmann::json::parse(buffer.str());
    }

    /// \brief Builds a JSON-RPC request document.
    inline nlohmann::json make_jsonrpc_request(
            nlohmann::json id,
            std::string method,
            nlohmann::json params = nlohmann::json::object()) {
        return nlohmann::json{
            {"jsonrpc", "2.0"},
            {"id", std::move(id)},
            {"method", std::move(method)},
            {"params", std::move(params)}
        };
    }

    /// \brief Builds a JSON-RPC success response.
    inline nlohmann::json make_jsonrpc_result(
            nlohmann::json id,
            nlohmann::json result) {
        return nlohmann::json{
            {"jsonrpc", "2.0"},
            {"id", std::move(id)},
            {"result", std::move(result)}
        };
    }

    /// \brief Builds a JSON-RPC error response.
    inline nlohmann::json make_jsonrpc_error(
            nlohmann::json id,
            const int code,
            std::string message,
            nlohmann::json data = nlohmann::json()) {
        nlohmann::json error = {
            {"code", code},
            {"message", std::move(message)}
        };
        if (!data.is_null()) {
            error["data"] = std::move(data);
        }
        return nlohmann::json{
            {"jsonrpc", "2.0"},
            {"id", std::move(id)},
            {"error", std::move(error)}
        };
    }

    /// \brief Builds a JSON-RPC notification document.
    inline nlohmann::json make_jsonrpc_notification(
            std::string method,
            nlohmann::json params = nlohmann::json::object()) {
        return nlohmann::json{
            {"jsonrpc", "2.0"},
            {"method", std::move(method)},
            {"params", std::move(params)}
        };
    }

    /// \brief Formats a decimal value as a compact string for wire payloads.
    inline std::string decimal_to_string(
            const double value,
            const int precision = 12) {
        std::ostringstream out;
        out << std::fixed << std::setprecision(precision) << value;
        std::string text = out.str();
        while (text.size() > 1 && text.back() == '0') {
            text.pop_back();
        }
        if (!text.empty() && text.back() == '.') {
            text.pop_back();
        }
        return text;
    }

    /// \brief Builds the standard event params envelope.
    inline nlohmann::json make_event_params(
            std::string event_id,
            std::string source,
            std::string stream_id,
            const std::uint64_t seq,
            const std::int64_t occurred_at_ms,
            const std::int64_t emitted_at_ms,
            nlohmann::json subject,
            const std::uint64_t revision,
            nlohmann::json payload) {
        return nlohmann::json{
            {"event_id", std::move(event_id)},
            {"source", std::move(source)},
            {"stream_id", std::move(stream_id)},
            {"seq", seq},
            {"occurred_at_ms", occurred_at_ms},
            {"emitted_at_ms", emitted_at_ms},
            {"subject", std::move(subject)},
            {"revision", revision},
            {"payload", std::move(payload)}
        };
    }

    /// \brief Builds a `balance.updated` notification.
    inline nlohmann::json make_balance_updated_notification(
            std::string event_id,
            std::string source,
            std::string stream_id,
            const std::uint64_t seq,
            const std::int64_t occurred_at_ms,
            const std::int64_t emitted_at_ms,
            std::string account_id,
            const double balance,
            const CurrencyType currency,
            const std::uint64_t revision = 1) {
        nlohmann::json subject = {{"account_id", account_id}};
        nlohmann::json payload = {
            {"account_id", account_id},
            {"balance", {
                {"value", decimal_to_string(balance)},
                {"currency", to_str(currency)}
            }}
        };
        return make_jsonrpc_notification(
            "balance.updated",
            make_event_params(
                std::move(event_id),
                std::move(source),
                std::move(stream_id),
                seq,
                occurred_at_ms,
                emitted_at_ms,
                std::move(subject),
                revision,
                std::move(payload)));
    }

    /// \brief Builds a `trade.updated` notification carrying a TradeResult snapshot.
    inline nlohmann::json make_trade_updated_notification(
            std::string event_id,
            std::string source,
            std::string stream_id,
            const std::uint64_t seq,
            const std::int64_t occurred_at_ms,
            const std::int64_t emitted_at_ms,
            const TradeResult& result,
            const std::uint64_t revision = 1) {
        nlohmann::json subject = {{"trade_id", std::to_string(result.trade_id)}};
        nlohmann::json payload = {
            {"trade_result", result}
        };
        return make_jsonrpc_notification(
            "trade.updated",
            make_event_params(
                std::move(event_id),
                std::move(source),
                std::move(stream_id),
                seq,
                occurred_at_ms,
                emitted_at_ms,
                std::move(subject),
                revision,
                std::move(payload)));
    }

} // namespace optionx::bridges::metatrader_file::detail

#endif // OPTIONX_HEADER_BRIDGES_METATRADER_FILE_DETAIL_META_TRADER_FILE_PROTOCOL_HPP_INCLUDED
