#pragma once
#ifndef OPTIONX_HEADER_BRIDGES_METATRADER_FILE_DETAIL_META_TRADER_FILE_PROTOCOL_HPP_INCLUDED
#define OPTIONX_HEADER_BRIDGES_METATRADER_FILE_DETAIL_META_TRADER_FILE_PROTOCOL_HPP_INCLUDED

/// \file MetaTraderFileProtocol.hpp
/// \brief NDJSON file-transport helpers for MetaTrader common-files integrations.

#include "../MetaTraderFileBridgeConfig.hpp"
#include "data/trading.hpp"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__unix__) || defined(__APPLE__)
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace optionx::bridges::metatrader_file::detail {

    /// \struct FileTransportLayout
    /// \brief Resolved files for one OptionX MetaTrader file-transport client.
    struct FileTransportLayout {
        std::filesystem::path root; ///< Client root directory.

        std::filesystem::path commands_log() const { return root / "commands.ndjson"; }
        std::filesystem::path events_log() const { return root / "events.ndjson"; }
        std::filesystem::path state_snapshot() const { return root / "state.json"; }
        std::filesystem::path commands_checkpoint() const { return root / "commands.checkpoint.json"; }
        std::filesystem::path events_checkpoint() const { return root / "events.checkpoint.json"; }
    };

    /// \struct NdjsonRecord
    /// \brief One complete NDJSON line plus transport offsets.
    struct NdjsonRecord {
        nlohmann::json document; ///< Parsed line document.
        std::uint64_t start_offset = 0; ///< Byte offset where this line starts.
        std::uint64_t next_offset = 0; ///< Byte offset immediately after this line feed.
        std::uint64_t file_seq = 0; ///< Monotonic per-file sequence when present.
    };

    /// \struct NdjsonReadResult
    /// \brief Result of reading complete NDJSON records.
    struct NdjsonReadResult {
        std::vector<NdjsonRecord> records; ///< Complete parsed records.
        std::uint64_t next_offset = 0; ///< Offset suitable for the next incremental read.
        bool source_truncated = false; ///< True when the requested offset was beyond EOF.
        bool incomplete_tail = false; ///< True when the file ended with a partial line.
    };

    /// \brief Builds resolved files from a bridge config.
    inline FileTransportLayout make_layout(const MetaTraderFileBridgeConfig& config) {
        return FileTransportLayout{config.client_root()};
    }

    /// \brief Creates the client root directory.
    inline void ensure_runtime_directories(const FileTransportLayout& layout) {
        std::error_code ec;
        std::filesystem::create_directories(layout.root, ec);
        if (ec) {
            throw std::runtime_error(
                "Failed to create MetaTrader file transport directory: " +
                layout.root.u8string() +
                ": " +
                ec.message());
        }
    }

    /// \brief Returns milliseconds since Unix epoch.
    inline std::int64_t unix_time_ms() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    /// \brief Atomically replaces a regular file with another file.
    inline bool replace_file_atomic(
            const std::filesystem::path& source,
            const std::filesystem::path& destination,
            std::error_code& ec) noexcept {
        ec.clear();
#if defined(_WIN32)
        if (::MoveFileExW(
                source.c_str(),
                destination.c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0) {
            return true;
        }
        ec = std::error_code(static_cast<int>(::GetLastError()), std::system_category());
        return false;
#else
        std::filesystem::rename(source, destination, ec);
        return !ec;
#endif
    }

    /// \brief Removes a file while ignoring cleanup errors.
    inline void remove_file_quietly(const std::filesystem::path& file) noexcept {
        std::error_code remove_ec;
        std::filesystem::remove(file, remove_ec);
    }

    /// \brief Returns the current process ID for temporary filename uniqueness.
    inline std::uint64_t current_process_id() noexcept {
#if defined(_WIN32)
        return static_cast<std::uint64_t>(::GetCurrentProcessId());
#elif defined(__unix__) || defined(__APPLE__)
        return static_cast<std::uint64_t>(::getpid());
#else
        return 0;
#endif
    }

    /// \brief Returns true when an exclusive-create operation failed because the file exists.
    inline bool is_file_exists_error(const std::error_code& ec) noexcept {
#if defined(_WIN32)
        return ec.category() == std::system_category() &&
               (ec.value() == ERROR_FILE_EXISTS || ec.value() == ERROR_ALREADY_EXISTS);
#else
        return ec == std::errc::file_exists;
#endif
    }

    /// \brief Builds a same-directory temp path that is unique across retries and processes.
    inline std::filesystem::path make_temp_file_path(
            const std::filesystem::path& parent,
            const std::filesystem::path& filename) {
        static std::atomic<std::uint64_t> temp_counter{0};
        const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto seq = temp_counter.fetch_add(1, std::memory_order_relaxed);
        return parent / (
            filename.u8string() +
            ".tmp." +
            std::to_string(current_process_id()) +
            "." +
            std::to_string(tick) +
            "." +
            std::to_string(seq));
    }

    /// \brief Creates and writes a temp file only if it does not already exist.
    inline bool write_text_file_exclusive(
            const std::filesystem::path& file,
            const std::string& text,
            std::error_code& ec) noexcept {
        ec.clear();
#if defined(_WIN32)
        HANDLE handle = ::CreateFileW(
            file.c_str(),
            GENERIC_WRITE,
            0,
            nullptr,
            CREATE_NEW,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
            nullptr);
        if (handle == INVALID_HANDLE_VALUE) {
            ec = std::error_code(static_cast<int>(::GetLastError()), std::system_category());
            return false;
        }

        std::size_t offset = 0;
        while (offset < text.size()) {
            const auto remaining = text.size() - offset;
            const auto chunk = static_cast<DWORD>(std::min<std::size_t>(
                remaining,
                static_cast<std::size_t>(std::numeric_limits<DWORD>::max())));
            DWORD written = 0;
            if (::WriteFile(handle, text.data() + offset, chunk, &written, nullptr) == 0 ||
                written == 0) {
                ec = std::error_code(static_cast<int>(::GetLastError()), std::system_category());
                ::CloseHandle(handle);
                return false;
            }
            offset += written;
        }

        if (::FlushFileBuffers(handle) == 0) {
            ec = std::error_code(static_cast<int>(::GetLastError()), std::system_category());
            ::CloseHandle(handle);
            return false;
        }
        if (::CloseHandle(handle) == 0) {
            ec = std::error_code(static_cast<int>(::GetLastError()), std::system_category());
            return false;
        }
        return true;
#elif defined(__unix__) || defined(__APPLE__)
        const int fd = ::open(file.c_str(), O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
        if (fd < 0) {
            ec = std::error_code(errno, std::generic_category());
            return false;
        }

        std::size_t offset = 0;
        while (offset < text.size()) {
            const auto written = ::write(fd, text.data() + offset, text.size() - offset);
            if (written < 0) {
                if (errno == EINTR) {
                    continue;
                }
                ec = std::error_code(errno, std::generic_category());
                ::close(fd);
                return false;
            }
            if (written == 0) {
                ec = std::make_error_code(std::errc::io_error);
                ::close(fd);
                return false;
            }
            offset += static_cast<std::size_t>(written);
        }

        if (::fsync(fd) != 0) {
            ec = std::error_code(errno, std::generic_category());
            ::close(fd);
            return false;
        }
        if (::close(fd) != 0) {
            ec = std::error_code(errno, std::generic_category());
            return false;
        }
        return true;
#else
        if (std::filesystem::exists(file, ec)) {
            ec = std::make_error_code(std::errc::file_exists);
            return false;
        }
        std::ofstream out(file, std::ios::binary);
        if (!out) {
            ec = std::make_error_code(std::errc::io_error);
            return false;
        }
        out << text;
        out.flush();
        if (!out) {
            ec = std::make_error_code(std::errc::io_error);
            return false;
        }
        return true;
#endif
    }

    /// \brief Writes text via same-directory temp file and atomic replacement.
    inline void write_text_file_atomic(
            const std::filesystem::path& file,
            const std::string& text) {
        const auto parent = file.parent_path();
        if (!parent.empty()) {
            std::error_code ec;
            std::filesystem::create_directories(parent, ec);
            if (ec) {
                throw std::runtime_error("Failed to create file transport directory: " + ec.message());
            }
        }

        std::filesystem::path temp_path;
        std::error_code ec;
        bool temp_created = false;
        for (int attempt = 0; attempt < 64; ++attempt) {
            temp_path = make_temp_file_path(parent, file.filename());
            if (write_text_file_exclusive(temp_path, text, ec)) {
                temp_created = true;
                break;
            }
            if (!is_file_exists_error(ec)) {
                throw std::runtime_error("Failed to create temp file: " + temp_path.u8string() + ": " + ec.message());
            }
        }
        if (!temp_created) {
            throw std::runtime_error("Failed to create unique MetaTrader file transport temp file.");
        }

        replace_file_atomic(temp_path, file, ec);
        if (ec) {
            remove_file_quietly(temp_path);
            throw std::runtime_error("Failed to replace file transport file: " + ec.message());
        }
    }

    /// \brief Atomically clears a log file owned by the caller.
    inline void clear_file_atomic(const std::filesystem::path& file) {
        write_text_file_atomic(file, std::string());
    }

    /// \brief Writes a JSON snapshot through atomic replacement.
    inline void write_json_file_atomic(
            const std::filesystem::path& file,
            const nlohmann::json& document,
            const int indent = 2) {
        write_text_file_atomic(file, document.dump(indent));
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
            throw std::runtime_error("MetaTrader file transport JSON exceeds configured byte limit.");
        }
        if (max_bytes == std::numeric_limits<std::size_t>::max() ||
            max_bytes + 1 > static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max())) {
            throw std::runtime_error("MetaTrader file transport JSON byte limit is too large for bounded read.");
        }

        std::ifstream in(file, std::ios::binary);
        if (!in) {
            throw std::runtime_error("Failed to open JSON file: " + file.u8string());
        }

        std::string data(max_bytes + 1, '\0');
        in.read(data.data(), static_cast<std::streamsize>(data.size()));
        const auto count = static_cast<std::size_t>(in.gcount());
        if (in.bad()) {
            throw std::runtime_error("Failed to read JSON file: " + file.u8string());
        }
        if (count > max_bytes || !in.eof()) {
            throw std::runtime_error("MetaTrader file transport JSON exceeds configured byte limit.");
        }
        data.resize(count);
        return nlohmann::json::parse(data);
    }

    /// \brief Returns a compact JSON-RPC/notification document with a file sequence.
    inline nlohmann::json with_file_seq(
            nlohmann::json document,
            const std::uint64_t file_seq) {
        if (file_seq == 0) {
            throw std::invalid_argument("MetaTrader file transport file_seq must be positive.");
        }
        document["file_seq"] = file_seq;
        return document;
    }

    /// \brief Appends one compact JSON line and a trailing LF.
    /// \details One NDJSON file must have exactly one writer. A record is
    /// visible to readers only after its trailing `\n`.
    inline void append_json_line(
            const std::filesystem::path& file,
            const nlohmann::json& document,
            const std::size_t max_line_bytes) {
        const auto line = document.dump(-1);
        if (line.size() > max_line_bytes) {
            throw std::runtime_error("MetaTrader file transport NDJSON line exceeds configured byte limit.");
        }

        const auto parent = file.parent_path();
        if (!parent.empty()) {
            std::error_code ec;
            std::filesystem::create_directories(parent, ec);
            if (ec) {
                throw std::runtime_error("Failed to create file transport directory: " + ec.message());
            }
        }

        std::ofstream out(file, std::ios::binary | std::ios::app);
        if (!out) {
            throw std::runtime_error("Failed to open NDJSON log: " + file.u8string());
        }
        out << line << '\n';
        out.flush();
        if (!out) {
            throw std::runtime_error("Failed to append NDJSON log: " + file.u8string());
        }
    }

    /// \brief Reads complete NDJSON records starting at a byte offset.
    /// \details The last line is ignored until it has a trailing `\n`.
    inline NdjsonReadResult read_ndjson_from_offset(
            const std::filesystem::path& file,
            const std::uint64_t offset,
            const std::size_t max_line_bytes,
            const std::size_t max_records = 0) {
        NdjsonReadResult result;

        std::error_code ec;
        if (!std::filesystem::exists(file, ec)) {
            result.source_truncated = offset != 0;
            return result;
        }

        const auto file_size = std::filesystem::file_size(file, ec);
        if (ec) {
            throw std::runtime_error("Failed to read NDJSON file size: " + ec.message());
        }

        std::uint64_t start_offset = offset;
        if (start_offset > file_size) {
            start_offset = 0;
            result.source_truncated = true;
        }
        result.next_offset = start_offset;

        const auto remaining = file_size - start_offset;
        if (remaining == 0) {
            return result;
        }
        if (remaining > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
            throw std::runtime_error("NDJSON tail is too large to read on this platform.");
        }

        std::ifstream in(file, std::ios::binary);
        if (!in) {
            throw std::runtime_error("Failed to open NDJSON log: " + file.u8string());
        }
        in.seekg(static_cast<std::streamoff>(start_offset), std::ios::beg);
        std::string tail(static_cast<std::size_t>(remaining), '\0');
        in.read(&tail[0], static_cast<std::streamsize>(tail.size()));
        if (!in && !in.eof()) {
            throw std::runtime_error("Failed to read NDJSON log: " + file.u8string());
        }

        std::size_t pos = 0;
        while (pos < tail.size()) {
            const auto newline = tail.find('\n', pos);
            if (newline == std::string::npos) {
                result.incomplete_tail = pos < tail.size();
                const auto partial_size = tail.size() - pos;
                if (partial_size > max_line_bytes) {
                    throw std::runtime_error("Incomplete NDJSON tail exceeds configured byte limit.");
                }
                break;
            }

            auto line = tail.substr(pos, newline - pos);
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            const auto record_start = start_offset + static_cast<std::uint64_t>(pos);
            const auto record_next = start_offset + static_cast<std::uint64_t>(newline + 1);
            result.next_offset = record_next;
            pos = newline + 1;

            if (line.empty()) {
                continue;
            }
            if (line.size() > max_line_bytes) {
                throw std::runtime_error("NDJSON line exceeds configured byte limit.");
            }

            NdjsonRecord record;
            record.document = nlohmann::json::parse(line);
            record.start_offset = record_start;
            record.next_offset = record_next;
            if (record.document.contains("file_seq")) {
                record.file_seq = record.document.at("file_seq").get<std::uint64_t>();
            }
            result.records.push_back(std::move(record));

            if (max_records != 0 && result.records.size() >= max_records) {
                break;
            }
        }

        return result;
    }

    /// \brief Reads complete records with `file_seq` greater than `last_file_seq`.
    /// \details This is robust to owner-side log cleanup when writers keep
    /// `file_seq` monotonic across cleanup cycles.
    inline std::vector<NdjsonRecord> read_ndjson_since_file_seq(
            const std::filesystem::path& file,
            const std::uint64_t last_file_seq,
            const std::size_t max_line_bytes) {
        auto all = read_ndjson_from_offset(file, 0, max_line_bytes);
        std::vector<NdjsonRecord> filtered;
        for (auto& record : all.records) {
            if (record.file_seq == 0) {
                throw std::runtime_error("NDJSON record is missing required file_seq.");
            }
            if (record.file_seq > last_file_seq) {
                filtered.push_back(std::move(record));
            }
        }
        return filtered;
    }

    /// \brief Builds a checkpoint snapshot for a log reader.
    inline nlohmann::json make_log_checkpoint(
            const std::uint64_t offset,
            const std::uint64_t last_file_seq) {
        return nlohmann::json{
            {"offset", offset},
            {"last_file_seq", last_file_seq}
        };
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

    /// \brief Builds a JSON-RPC request document with a required file sequence.
    inline nlohmann::json make_file_jsonrpc_request(
            const std::uint64_t file_seq,
            nlohmann::json id,
            std::string method,
            nlohmann::json params = nlohmann::json::object()) {
        return with_file_seq(
            make_jsonrpc_request(std::move(id), std::move(method), std::move(params)),
            file_seq);
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

    /// \brief Builds a JSON-RPC notification document with a required file sequence.
    inline nlohmann::json make_file_jsonrpc_notification(
            const std::uint64_t file_seq,
            std::string method,
            nlohmann::json params = nlohmann::json::object()) {
        return with_file_seq(
            make_jsonrpc_notification(std::move(method), std::move(params)),
            file_seq);
    }

    /// \brief Formats a decimal value as a locale-independent fixed-scale string.
    inline std::string decimal_to_string(
            const double value,
            const int precision = 12) {
        if (precision < 0 || precision > 18) {
            throw std::invalid_argument("File transport decimal precision is out of range.");
        }
        if (!std::isfinite(value)) {
            throw std::invalid_argument("File transport decimal value must be finite.");
        }
        const double normalized = (value == 0.0) ? 0.0 : value;
        std::ostringstream out;
        out.imbue(std::locale::classic());
        out << std::fixed << std::setprecision(precision) << normalized;
        return out.str();
    }

    /// \brief Maps internal trade lifecycle states to Bridge Protocol states.
    inline const char* protocol_trade_state(const TradeState state) noexcept {
        switch (state) {
        case TradeState::WAITING_OPEN:
            return "queued";
        case TradeState::OPEN_SUCCESS:
        case TradeState::IN_PROGRESS:
        case TradeState::WAITING_CLOSE:
            return "opened";
        case TradeState::WIN:
        case TradeState::LOSS:
        case TradeState::STANDOFF:
        case TradeState::REFUND:
            return "closed";
        case TradeState::OPEN_ERROR:
        case TradeState::CHECK_ERROR:
            return "failed";
        case TradeState::CANCELED_TRADE:
            return "cancelled";
        case TradeState::UNKNOWN:
        default:
            return "unknown";
        }
    }

    /// \brief Maps internal trade lifecycle states to Bridge Protocol outcomes.
    inline const char* protocol_trade_outcome(const TradeState state) noexcept {
        switch (state) {
        case TradeState::WIN:
            return "win";
        case TradeState::LOSS:
            return "loss";
        case TradeState::STANDOFF:
            return "draw";
        case TradeState::REFUND:
            return "refund";
        default:
            return "unknown";
        }
    }

    /// \brief Maps internal trade error codes to stable lower-snake protocol codes.
    inline const char* protocol_trade_error_code(const TradeErrorCode code) noexcept {
        switch (code) {
        case TradeErrorCode::SUCCESS:
            return "success";
        case TradeErrorCode::INVALID_SYMBOL:
            return "invalid_symbol";
        case TradeErrorCode::INVALID_OPTION:
            return "invalid_option";
        case TradeErrorCode::INVALID_ORDER:
            return "invalid_order";
        case TradeErrorCode::INVALID_ACCOUNT:
            return "invalid_account";
        case TradeErrorCode::INVALID_CURRENCY:
            return "invalid_currency";
        case TradeErrorCode::AMOUNT_TOO_LOW:
            return "amount_too_low";
        case TradeErrorCode::AMOUNT_TOO_HIGH:
            return "amount_too_high";
        case TradeErrorCode::REFUND_TOO_LOW:
            return "refund_too_low";
        case TradeErrorCode::REFUND_TOO_HIGH:
            return "refund_too_high";
        case TradeErrorCode::PAYOUT_TOO_LOW:
            return "payout_too_low";
        case TradeErrorCode::INVALID_DURATION:
            return "invalid_duration";
        case TradeErrorCode::INVALID_EXPIRY_TIME:
            return "invalid_expiry_time";
        case TradeErrorCode::LIMIT_OPEN_TRADES:
            return "limit_open_trades";
        case TradeErrorCode::INVALID_REQUEST:
            return "invalid_request";
        case TradeErrorCode::LONG_QUEUE_WAIT:
            return "long_queue_wait";
        case TradeErrorCode::LONG_RESPONSE_WAIT:
            return "long_response_wait";
        case TradeErrorCode::NO_CONNECTION:
            return "no_connection";
        case TradeErrorCode::CLIENT_FORCED_CLOSE:
            return "client_forced_close";
        case TradeErrorCode::PARSING_ERROR:
            return "parsing_error";
        case TradeErrorCode::CANCELED_TRADE:
            return "cancelled_trade";
        case TradeErrorCode::INSUFFICIENT_BALANCE:
            return "insufficient_balance";
        default:
            return "unknown_error";
        }
    }

    /// \brief Returns a JSON string ID when the numeric ID is present.
    template<typename Integer>
    inline nlohmann::json numeric_id_to_json_string(const Integer id) {
        return id == 0 ? nlohmann::json(nullptr) : nlohmann::json(std::to_string(id));
    }

    /// \brief Builds a money object with a decimal-string amount.
    inline nlohmann::json make_money_value(
            const double value,
            const CurrencyType currency,
            const int precision = 2) {
        nlohmann::json money = {{"value", decimal_to_string(value, precision)}};
        if (currency != CurrencyType::UNKNOWN) {
            money["currency"] = to_str(currency);
        }
        return money;
    }

    /// \brief Builds a protocol trade snapshot from internal result/request DTOs.
    /// \details This adapter is intentionally separate from TradeResult JSON
    /// serialization so the wire protocol does not expose internal lifecycle
    /// enum names or numeric JSON identifiers.
    inline nlohmann::json make_protocol_trade_snapshot(
            const TradeResult& result,
            const TradeRequest* request = nullptr) {
        const auto trade_id =
            result.trade_id != 0 ? result.trade_id : (request ? request->trade_id : 0);
        const auto account_id = request ? request->account_id : 0;
        const auto signal_id = request ? request->signal_id : 0;
        const auto bridge_id = request ? request->bridge_id : 0;
        const auto currency =
            result.currency != CurrencyType::UNKNOWN
                ? result.currency
                : (request ? request->currency : CurrencyType::UNKNOWN);

        nlohmann::json snapshot = {
            {"trade_id", numeric_id_to_json_string(trade_id)},
            {"broker_option_id", numeric_id_to_json_string(result.option_id)},
            {"broker_option_hash", result.option_hash.empty()
                ? nlohmann::json(nullptr)
                : nlohmann::json(result.option_hash)},
            {"state", protocol_trade_state(result.trade_state)},
            {"outcome", protocol_trade_outcome(result.trade_state)},
            {"final", is_terminal_trade_state(result.trade_state)}
        };

        if (request) {
            if (account_id != 0) snapshot["account_id"] = std::to_string(account_id);
            if (!request->symbol.empty()) snapshot["symbol"] = request->symbol;
            if (request->option_type != OptionType::UNKNOWN) snapshot["option_type"] = to_str(request->option_type);
            if (request->order_type != OrderType::UNKNOWN) snapshot["order_type"] = to_str(request->order_type);
            if (request->account_type != AccountType::UNKNOWN) snapshot["account_type"] = to_str(request->account_type);
            if (request->duration != 0) snapshot["duration_ms"] = static_cast<std::uint64_t>(request->duration) * 1000ULL;
            if (request->expiry_time > 0) snapshot["expires_at_ms"] = request->expiry_time * 1000LL;

            nlohmann::json origin_signal = nlohmann::json::object();
            if (signal_id != 0) origin_signal["signal_id"] = std::to_string(signal_id);
            if (bridge_id != 0) origin_signal["bridge_id"] = std::to_string(bridge_id);
            if (!request->unique_hash.empty()) origin_signal["unique_hash"] = request->unique_hash;
            if (!request->signal_name.empty()) origin_signal["signal_name"] = request->signal_name;
            if (!origin_signal.empty()) snapshot["origin_signal"] = std::move(origin_signal);
        } else if (result.account_type != AccountType::UNKNOWN) {
            snapshot["account_type"] = to_str(result.account_type);
        }

        if (result.platform_type != PlatformType::UNKNOWN) {
            snapshot["platform_type"] = to_str(result.platform_type);
        }
        if (result.amount > 0.0) {
            snapshot["amount"] = make_money_value(result.amount, currency, 2);
        }
        if (result.payout > 0.0) {
            snapshot["payout"] = decimal_to_string(result.payout, 6);
        }
        snapshot["profit"] =
            is_result_state(result.trade_state) || result.profit != 0.0
                ? make_money_value(result.profit, currency, 2)
                : nlohmann::json(nullptr);

        if (result.has_balance()) snapshot["balance"] = make_money_value(result.balance, currency, 2);
        if (result.has_open_balance()) snapshot["open_balance"] = make_money_value(result.open_balance, currency, 2);
        if (result.has_close_balance()) snapshot["close_balance"] = make_money_value(result.close_balance, currency, 2);

        if (result.open_price != 0.0) snapshot["open_price"] = decimal_to_string(result.open_price, 12);
        if (result.close_price != 0.0) snapshot["close_price"] = decimal_to_string(result.close_price, 12);
        if (result.delay != 0) snapshot["delay_ms"] = result.delay;
        if (result.ping != 0) snapshot["ping_ms"] = result.ping;
        if (result.place_date > 0) snapshot["place_time_ms"] = result.place_date;
        if (result.send_date > 0) snapshot["send_time_ms"] = result.send_date;
        if (result.open_date > 0) snapshot["open_time_ms"] = result.open_date;
        if (result.close_date > 0) snapshot["close_time_ms"] = result.close_date;

        if (result.trade_state == TradeState::CANCELED_TRADE &&
            result.error_code == TradeErrorCode::SUCCESS) {
            if (result.error_desc.empty()) {
                snapshot["failure"] = nullptr;
            } else {
                snapshot["failure"] = {
                    {"code", "cancelled"},
                    {"message", result.error_desc}
                };
            }
        } else if (result.error_code != TradeErrorCode::SUCCESS ||
            is_error_trade_state(result.trade_state)) {
            const auto failure_code =
                result.error_code == TradeErrorCode::SUCCESS
                    ? "trade_failed"
                    : protocol_trade_error_code(result.error_code);
            snapshot["failure"] = {
                {"code", failure_code},
                {"message", result.error_desc.empty()
                    ? (result.error_code == TradeErrorCode::SUCCESS
                        ? std::string("Trade failed.")
                        : std::string(to_str(result.error_code)))
                    : result.error_desc}
            };
        } else {
            snapshot["failure"] = nullptr;
        }

        return snapshot;
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
                {"value", decimal_to_string(balance, 2)},
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

    /// \brief Builds a `trade.updated` notification carrying a protocol trade snapshot.
    inline nlohmann::json make_trade_updated_notification(
            std::string event_id,
            std::string source,
            std::string stream_id,
            const std::uint64_t seq,
            const std::int64_t occurred_at_ms,
            const std::int64_t emitted_at_ms,
            const TradeResult& result,
            const std::uint64_t revision = 1) {
        nlohmann::json subject = {{"trade_id", numeric_id_to_json_string(result.trade_id)}};
        nlohmann::json payload = {
            {"trade", make_protocol_trade_snapshot(result)}
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

    /// \brief Builds a `trade.updated` notification from request and result snapshots.
    inline nlohmann::json make_trade_updated_notification(
            std::string event_id,
            std::string source,
            std::string stream_id,
            const std::uint64_t seq,
            const std::int64_t occurred_at_ms,
            const std::int64_t emitted_at_ms,
            const TradeRequest& request,
            const TradeResult& result,
            const std::uint64_t revision = 1) {
        const auto trade_id = result.trade_id != 0 ? result.trade_id : request.trade_id;
        nlohmann::json subject = {{"trade_id", numeric_id_to_json_string(trade_id)}};
        if (request.signal_id != 0) subject["signal_id"] = std::to_string(request.signal_id);
        nlohmann::json payload = {
            {"trade", make_protocol_trade_snapshot(result, &request)}
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

    /// \brief Builds a `state.json` snapshot.
    inline nlohmann::json make_state_snapshot(
            const std::uint64_t version,
            const std::int64_t updated_at_ms,
            std::string connection,
            nlohmann::json accounts = nlohmann::json::array(),
            nlohmann::json open_trades = nlohmann::json::array()) {
        return nlohmann::json{
            {"version", version},
            {"updated_at_ms", updated_at_ms},
            {"connection", std::move(connection)},
            {"accounts", std::move(accounts)},
            {"open_trades", std::move(open_trades)}
        };
    }

} // namespace optionx::bridges::metatrader_file::detail

#endif // OPTIONX_HEADER_BRIDGES_METATRADER_FILE_DETAIL_META_TRADER_FILE_PROTOCOL_HPP_INCLUDED
