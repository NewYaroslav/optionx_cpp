#pragma once
#ifndef OPTIONX_HEADER_BRIDGES_METATRADER_FILE_META_TRADER_FILE_COMMAND_WRITER_HPP_INCLUDED
#define OPTIONX_HEADER_BRIDGES_METATRADER_FILE_META_TRADER_FILE_COMMAND_WRITER_HPP_INCLUDED

/// \file MetaTraderFileCommandWriter.hpp
/// \brief Defines the C++ client-side command writer for the MetaTrader file bridge.

namespace optionx::bridges::metatrader_file {

    /// \struct MetaTraderFileTradeCommand
    /// \brief Input data for `signal.submit` and `trade.open` command generation.
    struct MetaTraderFileTradeCommand {
        std::string symbol = "EURUSD"; ///< Trading symbol as seen by the receiver.
        std::string order_type = "BUY"; ///< Protocol order side, for example `BUY` or `SELL`.
        std::string option_type = "SPRINT"; ///< Protocol option type.
        std::string amount_value = "1.00"; ///< Decimal amount string preserving caller scale.
        std::string currency = "USD"; ///< Amount currency.
        std::uint64_t duration_ms = 60000; ///< Expiry duration for binary-option style trades.
        std::string signal_name; ///< Optional strategy/signal name.
        std::string unique_hash; ///< Optional domain dedupe key; defaults to `idempotency_key`.
        std::string account_id; ///< Optional account routing target.
        std::string id; ///< Optional JSON-RPC id; defaults to `idempotency_key`.
        std::string idempotency_key; ///< Required caller-persisted retry key for trade commands.
        std::int64_t valid_until_ms = 0; ///< Absolute Unix deadline; generated from `valid_for_ms` when zero.
        std::int64_t valid_for_ms = 60000; ///< Relative command lifetime used when `valid_until_ms` is zero.
    };

    /// \struct MetaTraderFileWrittenCommand
    /// \brief Metadata returned after a command is appended to `commands.ndjson`.
    struct MetaTraderFileWrittenCommand {
        std::uint64_t file_seq = 0; ///< Writer sequence assigned to the appended line.
        std::string id; ///< JSON-RPC request id written to the command.
        std::string idempotency_key; ///< Effective idempotency key, if the command has one.
        std::filesystem::path commands_log; ///< Writer-owned command log path.
        nlohmann::json document; ///< Full command document that was appended.
    };

    /// \class MetaTraderFileCommandWriter
    /// \brief Writes canonical client commands for `MetaTraderFileBridge`.
    /// \details This is the C++ counterpart to the MQL `COptionXFileBridge`
    /// header: a lightweight client-side writer, not a bridge runtime. It owns
    /// `commands.ndjson`, assigns monotonic `file_seq`, appends one compact
    /// JSON-RPC request per line, and may clear the command log only after the
    /// bridge checkpoint confirms that all visible commands were consumed.
    /// Public methods serialize object state with an internal mutex. Mutations
    /// of `commands.ndjson` additionally take a sibling lock file so several
    /// writer objects/processes can share the same client root without assigning
    /// duplicate `file_seq` values or clearing each other's records.
    class MetaTraderFileCommandWriter {
    public:
        /// \brief Creates a command writer from a validated bridge file configuration.
        explicit MetaTraderFileCommandWriter(MetaTraderFileBridgeConfig config)
            : m_config(std::move(config)),
              m_layout(detail::make_layout(m_config)) {}

        /// \brief Ensures directories exist and initializes the next `file_seq`.
        void initialize() {
            std::lock_guard<std::mutex> lock(m_mutex);
            detail::ScopedLogFileLock log_lock(m_layout.commands_log());
            initialize_locked();
        }

        /// \brief Returns the resolved file layout.
        const detail::FileTransportLayout& layout() const noexcept {
            return m_layout;
        }

        /// \brief Returns the next writer sequence, initializing the writer if needed.
        std::uint64_t next_file_seq() {
            std::lock_guard<std::mutex> lock(m_mutex);
            detail::ScopedLogFileLock log_lock(m_layout.commands_log());
            ensure_initialized_locked();
            refresh_next_file_seq_locked();
            return m_next_file_seq;
        }

        /// \brief Appends a `signal.submit` command.
        /// \throws std::invalid_argument when `command.idempotency_key` is empty.
        MetaTraderFileWrittenCommand signal_submit(MetaTraderFileTradeCommand command) {
            std::lock_guard<std::mutex> lock(m_mutex);
            prepare_trade_command(command);
            return append_request_locked(
                command.id,
                "signal.submit",
                make_trade_params(command, "signal"));
        }

        /// \brief Appends a `trade.open` command.
        /// \throws std::invalid_argument when `command.idempotency_key` is empty.
        MetaTraderFileWrittenCommand trade_open(MetaTraderFileTradeCommand command) {
            std::lock_guard<std::mutex> lock(m_mutex);
            prepare_trade_command(command);
            return append_request_locked(
                command.id,
                "trade.open",
                make_trade_params(command, "trade"));
        }

        /// \brief Appends an `account.balance.get` query command.
        MetaTraderFileWrittenCommand account_balance_get(
                std::string account_id = {},
                std::string id = {}) {
            std::lock_guard<std::mutex> lock(m_mutex);
            ensure_initialized_locked();
            if (id.empty()) {
                id = make_compact_operation_key("mql");
            }

            nlohmann::json params = nlohmann::json::object();
            if (!account_id.empty()) {
                params["account_id"] = std::move(account_id);
            }
            return append_request_locked(std::move(id), "account.balance.get", std::move(params));
        }

        /// \brief Clears `commands.ndjson` only when the bridge checkpoint caught up.
        /// \return True when the log was cleared; false when cleanup is not safe yet.
        bool clear_commands_if_checkpoint_caught_up() {
            std::lock_guard<std::mutex> lock(m_mutex);
            detail::ScopedLogFileLock log_lock(m_layout.commands_log());
            ensure_initialized_locked();
            return clear_commands_if_checkpoint_caught_up_locked();
        }

    private:
        void initialize_locked() {
            detail::ensure_runtime_directories(m_layout);
            refresh_next_file_seq_locked();
            m_initialized = true;
        }

        void refresh_next_file_seq_locked() {
            const auto checkpoint = read_reader_checkpoint();
            const auto recovered = detail::next_file_seq_after_checkpoint(
                m_layout.commands_log(),
                checkpoint,
                m_config.max_line_bytes);
            if (!m_initialized || recovered > m_next_file_seq) {
                m_next_file_seq = recovered;
            }
        }

        void ensure_initialized_locked() {
            if (!m_initialized) {
                initialize_locked();
            }
        }

        std::uint64_t read_reader_checkpoint() const {
            std::error_code ec;
            const auto exists = std::filesystem::exists(m_layout.commands_checkpoint(), ec);
            if (ec) {
                throw std::runtime_error("Failed to inspect MetaTrader command checkpoint: " + ec.message());
            }
            if (!exists) {
                return 0;
            }
            const auto checkpoint = detail::read_json_file(
                m_layout.commands_checkpoint(),
                m_config.max_line_bytes);
            if (!checkpoint.is_object() || !checkpoint.contains("last_file_seq")) {
                throw std::runtime_error("MetaTrader command checkpoint is missing last_file_seq.");
            }
            const auto& value = checkpoint.at("last_file_seq");
            if (value.is_number_unsigned()) {
                return value.get<std::uint64_t>();
            }
            if (value.is_number_integer()) {
                const auto signed_value = value.get<std::int64_t>();
                if (signed_value >= 0) {
                    return static_cast<std::uint64_t>(signed_value);
                }
            }
            throw std::runtime_error("MetaTrader command checkpoint last_file_seq must be a non-negative integer.");
        }

        bool clear_commands_if_checkpoint_caught_up_locked() {
            const auto checkpoint = read_reader_checkpoint();
            const auto visible_max = detail::max_file_seq_in_ndjson(
                m_layout.commands_log(),
                m_config.max_line_bytes);
            if (visible_max == 0 || checkpoint < visible_max) {
                return false;
            }

            detail::clear_file_atomic(m_layout.commands_log());
            if (checkpoint == std::numeric_limits<std::uint64_t>::max()) {
                throw std::overflow_error("MetaTrader file command writer file_seq overflow.");
            }
            m_next_file_seq = checkpoint + 1;
            return true;
        }

        void ensure_command_log_capacity_locked(const std::size_t appended_bytes) {
            if (appended_bytes > m_config.max_command_log_bytes) {
                throw std::runtime_error(
                    "MetaTrader file command would exceed configured command log byte limit.");
            }

            detail::repair_incomplete_ndjson_tail(m_layout.commands_log());
            auto current_size = detail::file_size_or_zero(m_layout.commands_log());
            if (current_size <= m_config.max_command_log_bytes &&
                appended_bytes <= m_config.max_command_log_bytes - current_size) {
                return;
            }

            if (clear_commands_if_checkpoint_caught_up_locked()) {
                current_size = detail::file_size_or_zero(m_layout.commands_log());
                if (current_size <= m_config.max_command_log_bytes &&
                    appended_bytes <= m_config.max_command_log_bytes - current_size) {
                    return;
                }
            }

            throw std::runtime_error(
                "MetaTrader command log would exceed configured byte limit; "
                "wait for bridge checkpoint or clean up commands.ndjson.");
        }

        static void prepare_trade_command(MetaTraderFileTradeCommand& command) {
            if (command.idempotency_key.empty()) {
                throw std::invalid_argument(
                    "MetaTrader file trade commands require a caller-provided "
                    "idempotency_key that can be reused on retry.");
            }
            if (command.id.empty()) {
                command.id = command.idempotency_key;
            }
            if (command.unique_hash.empty()) {
                command.unique_hash = command.idempotency_key;
            }
            if (command.valid_until_ms <= 0) {
                const auto lifetime = command.valid_for_ms > 0 ? command.valid_for_ms : 60000;
                command.valid_until_ms = detail::unix_time_ms() + lifetime;
            }
        }

        static nlohmann::json make_trade_params(
                const MetaTraderFileTradeCommand& command,
                const char* trade_key) {
            nlohmann::json params = {
                {"context", {
                    {"idempotency_key", command.idempotency_key},
                    {"valid_until_ms", command.valid_until_ms}
                }},
                {"identity", {
                    {"unique_hash", command.unique_hash}
                }},
                {trade_key, {
                    {"symbol", command.symbol},
                    {"order_type", command.order_type},
                    {"option_type", command.option_type},
                    {"amount", {
                        {"value", command.amount_value},
                        {"currency", command.currency}
                    }},
                    {"expiry", {
                        {"kind", "duration"},
                        {"duration_ms", command.duration_ms}
                    }}
                }}
            };

            if (!command.signal_name.empty()) {
                params["identity"]["signal_name"] = command.signal_name;
            }
            if (!command.account_id.empty()) {
                params["routing"] = {
                    {"selector", {
                        {"kind", "account"},
                        {"account_id", command.account_id}
                    }}
                };
            }
            return params;
        }

        MetaTraderFileWrittenCommand append_request_locked(
                nlohmann::json id,
                std::string method,
                nlohmann::json params) {
            detail::ScopedLogFileLock log_lock(m_layout.commands_log());
            ensure_initialized_locked();
            refresh_next_file_seq_locked();
            const auto file_seq = m_next_file_seq;
            const auto document = detail::make_file_jsonrpc_request(
                file_seq,
                std::move(id),
                std::move(method),
                std::move(params));
            const auto line = document.dump(-1);
            ensure_command_log_capacity_locked(line.size() + 1);
            detail::append_serialized_json_line(
                m_layout.commands_log(),
                line,
                m_config.max_line_bytes);

            if (m_next_file_seq == std::numeric_limits<std::uint64_t>::max()) {
                throw std::overflow_error("MetaTrader file command writer file_seq overflow.");
            }
            ++m_next_file_seq;

            MetaTraderFileWrittenCommand written;
            written.file_seq = file_seq;
            written.commands_log = m_layout.commands_log();
            written.document = document;
            written.id = detail::json_id_to_string(document.at("id"));
            const auto& context = document.at("params").value("context", nlohmann::json::object());
            if (context.is_object() && context.contains("idempotency_key")) {
                written.idempotency_key = context.at("idempotency_key").get<std::string>();
            }
            return written;
        }

        MetaTraderFileBridgeConfig m_config;
        detail::FileTransportLayout m_layout;
        std::uint64_t m_next_file_seq = 1;
        bool m_initialized = false;
        mutable std::mutex m_mutex;
    };

} // namespace optionx::bridges::metatrader_file

#endif // OPTIONX_HEADER_BRIDGES_METATRADER_FILE_META_TRADER_FILE_COMMAND_WRITER_HPP_INCLUDED
