#pragma once
#ifndef OPTIONX_HEADER_BRIDGES_METATRADER_FILE_DETAIL_META_TRADER_FILE_PROTOCOL_HPP_INCLUDED
#define OPTIONX_HEADER_BRIDGES_METATRADER_FILE_DETAIL_META_TRADER_FILE_PROTOCOL_HPP_INCLUDED

/// \file MetaTraderFileProtocol.hpp
/// \brief File-drop transport helpers for MetaTrader common-files integrations.

#include "../MetaTraderFileBridgeConfig.hpp"
#include "data/trading.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <locale>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#if defined(__linux__)
#include <fcntl.h>
#include <sys/syscall.h>
#ifndef RENAME_NOREPLACE
#define RENAME_NOREPLACE (1U << 0U)
#endif
#endif
#endif

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
    inline bool is_safe_transport_filename(const std::string& filename) {
        if (filename.empty() || filename == "." || filename == "..") return false;
        const auto dot = filename.find('.');
        if (is_windows_reserved_device_name(filename.substr(0, dot))) return false;
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
        std::array<char, 20> digits{};
        std::array<char, 20> raw{};
        const auto result = std::to_chars(raw.data(), raw.data() + raw.size(), seq);
        if (result.ec != std::errc()) {
            throw std::runtime_error("Failed to format file transport delivery sequence.");
        }
        const auto count = static_cast<std::size_t>(result.ptr - raw.data());
        if (count > digits.size()) {
            throw std::runtime_error("File transport delivery sequence exceeds 20 digits.");
        }
        digits.fill('0');
        std::copy(raw.data(), raw.data() + count, digits.data() + digits.size() - count);
        return std::string(digits.data(), digits.size());
    }

    /// \brief Builds the unordered request/response filename pattern.
    inline std::string make_message_filename(
            const std::int64_t unix_ms,
            const std::string& file_uuid) {
        if (unix_ms < 0) {
            throw std::invalid_argument("File transport unix_ms must not be negative.");
        }
        if (!is_safe_file_transport_id(file_uuid)) {
            throw std::invalid_argument(
                "File transport file_uuid must be a safe [A-Za-z0-9.-]+ identifier.");
        }
        return std::to_string(unix_ms) + "_" + file_uuid + ".json";
    }

    /// \brief Builds the ordered event filename pattern.
    inline std::string make_event_filename(
            const std::string& delivery_queue_id,
            const std::uint64_t delivery_seq,
            const std::string& file_uuid) {
        if (!is_safe_file_transport_id(delivery_queue_id)) {
            throw std::invalid_argument(
                "File transport delivery_queue_id must be a safe [A-Za-z0-9.-]+ identifier.");
        }
        if (!is_safe_file_transport_id(file_uuid)) {
            throw std::invalid_argument(
                "File transport file_uuid must be a safe [A-Za-z0-9.-]+ identifier.");
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

    /// \brief Atomically moves a regular file without replacing an existing destination.
    inline bool rename_file_no_replace(
            const std::filesystem::path& source,
            const std::filesystem::path& destination,
            std::error_code& ec) noexcept {
        ec.clear();
#if defined(_WIN32)
        if (::MoveFileExW(
                source.c_str(),
                destination.c_str(),
                MOVEFILE_WRITE_THROUGH) != 0) {
            return true;
        }
        const auto code = ::GetLastError();
        if (code == ERROR_FILE_EXISTS || code == ERROR_ALREADY_EXISTS) {
            ec = std::make_error_code(std::errc::file_exists);
        } else {
            ec = std::error_code(static_cast<int>(code), std::system_category());
        }
        return false;
#elif defined(__linux__)
#if defined(SYS_renameat2)
        if (::syscall(
                SYS_renameat2,
                AT_FDCWD,
                source.c_str(),
                AT_FDCWD,
                destination.c_str(),
                RENAME_NOREPLACE) == 0) {
            return true;
        }
        const int rename_errno = errno;
        if (rename_errno != ENOSYS && rename_errno != EINVAL) {
            ec = std::error_code(rename_errno, std::generic_category());
            return false;
        }
#endif
        if (::link(source.c_str(), destination.c_str()) != 0) {
            ec = std::error_code(errno, std::generic_category());
            return false;
        }
        if (::unlink(source.c_str()) != 0) {
            ec = std::error_code(errno, std::generic_category());
            return false;
        }
        return true;
#elif defined(__unix__) || defined(__APPLE__)
        if (::link(source.c_str(), destination.c_str()) != 0) {
            ec = std::error_code(errno, std::generic_category());
            return false;
        }
        if (::unlink(source.c_str()) != 0) {
            ec = std::error_code(errno, std::generic_category());
            return false;
        }
        return true;
#else
        if (std::filesystem::exists(destination, ec)) {
            ec = std::make_error_code(std::errc::file_exists);
            return false;
        }
        std::filesystem::rename(source, destination, ec);
        return !ec;
#endif
    }

    /// \brief Removes a file while ignoring cleanup errors.
    inline void remove_file_quietly(const std::filesystem::path& file) noexcept {
        std::error_code remove_ec;
        std::filesystem::remove(file, remove_ec);
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

        rename_file_no_replace(temp_path, final_path, ec);
        if (ec) {
            remove_file_quietly(temp_path);
            if (ec == std::make_error_code(std::errc::file_exists)) {
                throw std::runtime_error(
                    "File transport final file already exists: " +
                    final_path.u8string());
            }
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
        rename_file_no_replace(ready_file, claimed_path, ec);
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

        if (result.error_code != TradeErrorCode::SUCCESS ||
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

} // namespace optionx::bridges::metatrader_file::detail

#endif // OPTIONX_HEADER_BRIDGES_METATRADER_FILE_DETAIL_META_TRADER_FILE_PROTOCOL_HPP_INCLUDED
