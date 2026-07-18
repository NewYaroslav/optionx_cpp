#pragma once
#ifndef OPTIONX_HEADER_BRIDGES_METATRADER_FILE_META_TRADER_FILE_BRIDGE_HPP_INCLUDED
#define OPTIONX_HEADER_BRIDGES_METATRADER_FILE_META_TRADER_FILE_BRIDGE_HPP_INCLUDED

/// \file MetaTraderFileBridge.hpp
/// \brief Defines the MetaTrader Common\Files NDJSON bridge.

namespace optionx::bridges::metatrader_file {

    /// \class MetaTraderFileBridge
    /// \brief Polls MetaTrader file-transport commands and publishes bridge events.
    ///
    /// The bridge owns `events.ndjson` and the command reader checkpoint. The
    /// MQL side owns `commands.ndjson`. Incoming `signal.submit` and
    /// `trade.open` records are converted to `TradeSignal` snapshots and
    /// delivered through `on_trade_signal()`.
    class MetaTraderFileBridge final : public BaseBridge {
    private:
        struct RuntimeState {
            std::mutex mutex;
            std::shared_ptr<BaseAccountInfoData> account_info;
            bridge_status_callback_t status_callback;
            BaseBridge::trade_signal_callback_t trade_signal_callback;
            BaseBridge::signal_report_callback_t signal_report_callback;
            BaseBridge::signal_id_allocator_t signal_id_allocator;
        };

        struct PendingSignalDispatch {
            detail::NdjsonRecord record;
            nlohmann::json id;
            nlohmann::json params;
            std::unique_ptr<TradeSignal> signal;
            std::string idempotency_key;
            std::string idempotency_storage_key;
            std::string request_storage_key;
            std::string payload_fingerprint;
        };

    public:
        /// \brief Constructs a bridge with empty runtime state.
        MetaTraderFileBridge()
            : m_state(std::make_shared<RuntimeState>()) {}

        /// \brief Stops the polling task before destruction.
        ~MetaTraderFileBridge() override {
            shutdown();
        }

        /// \brief Configures the bridge with MetaTrader file settings.
        /// \param config Configuration object. Must be `MetaTraderFileBridgeConfig`.
        /// \return `true` when configuration is valid and accepted.
        bool configure(std::unique_ptr<IBridgeConfig> config) override {
            if (!config) return false;

            const auto* typed =
                dynamic_cast<const MetaTraderFileBridgeConfig*>(config.get());
            if (!typed) {
                config->dispatch_callbacks(false, "Invalid MetaTrader file bridge config type.");
                return false;
            }

            auto next_config = std::make_shared<MetaTraderFileBridgeConfig>(*typed);
            const auto validation = next_config->validate();
            config->dispatch_callbacks(validation.first, validation.second);
            if (!validation.first) {
                return false;
            }

            std::unique_lock<std::mutex> config_lock(m_config_mutex, std::defer_lock);
            std::unique_lock<std::mutex> io_lock(m_io_mutex, std::defer_lock);
            std::lock(config_lock, io_lock);
            // Runtime paths, offsets and durable sequence state are derived
            // once. Reconfiguring after startup would mix old filesystem state
            // with a new layout, so it is rejected instead of partially reset.
            if (m_runtime_initialized || m_running || m_processing) {
                io_lock.unlock();
                config_lock.unlock();
                config->dispatch_callbacks(
                    false,
                    "MetaTrader file bridge cannot be reconfigured after runtime start.");
                return false;
            }
            m_config = std::move(next_config);
            return true;
        }

        /// \brief Returns the status update callback slot.
        bridge_status_callback_t& on_status_update() override {
            return m_state->status_callback;
        }

        /// \brief Returns the trade signal callback slot.
        trade_signal_callback_t& on_trade_signal() override {
            return m_state->trade_signal_callback;
        }

        /// \brief Returns the signal diagnostic report callback slot.
        signal_report_callback_t& on_signal_report() override {
            return m_state->signal_report_callback;
        }

        /// \brief Returns the signal ID allocator slot.
        signal_id_allocator_t& on_signal_id() override {
            return m_state->signal_id_allocator;
        }

        /// \brief Updates account snapshot state and emits balance/state files.
        /// \param info Account update received from the trading platform.
        void update_account_info(const AccountInfoUpdate& info) override {
            if (!info.account_info) return;

            {
                std::lock_guard<std::mutex> lock(m_state->mutex);
                m_state->account_info = info.account_info;
            }

            try {
                auto config = get_config_or_throw();
                std::lock_guard<std::mutex> io_lock(m_io_mutex);
                ensure_runtime_started_locked(*config);
                if (config->enable_events && detail::should_emit_balance_update(info.status)) {
                    append_event_locked(
                        detail::make_balance_updated_notification(
                            next_event_id_locked("balance"),
                            detail::source_uri(*config),
                            m_event_stream_id,
                            next_event_stream_seq_locked(),
                            detail::unix_time_ms(),
                            detail::unix_time_ms(),
                            detail::account_id_string(*info.account_info),
                            detail::safe_account_balance(*info.account_info),
                            detail::safe_account_currency(*info.account_info)));
                }
                if (config->enable_state_snapshot) {
                    write_state_snapshot_locked(*config);
                }
            } catch (const std::exception& ex) {
                notify_status(BridgeStatus::CONNECTION_ERROR, {}, ex.what());
            }
        }

        /// \brief Emits a trade result update to `events.ndjson`.
        /// \param request Original trade request.
        /// \param result Current trade result snapshot.
        void update_trade_result(
                const TradeRequest& request,
                const TradeResult& result) override {
            try {
                auto config = get_config_or_throw();
                std::lock_guard<std::mutex> io_lock(m_io_mutex);
                ensure_runtime_started_locked(*config);
                if (!config->enable_events) {
                    return;
                }
                const auto now = detail::unix_time_ms();
                append_event_locked(
                    detail::make_trade_updated_notification(
                        next_event_id_locked("trade"),
                        detail::source_uri(*config),
                        m_event_stream_id,
                        next_event_stream_seq_locked(),
                        result.close_date > 0 ? result.close_date : now,
                        now,
                        request,
                        result));
            } catch (const std::exception& ex) {
                notify_status(BridgeStatus::CONNECTION_ERROR, {}, ex.what());
            }
        }

        /// \brief Starts periodic polling of `commands.ndjson`.
        void run() override {
            auto config = get_config_or_throw();
            if (!get_signal_id_allocator()) {
                notify_status(
                    BridgeStatus::SERVER_START_FAILED,
                    {},
                    "MetaTrader file bridge requires a signal ID allocator.");
                return;
            }
            if (!get_trade_signal_callback()) {
                notify_status(
                    BridgeStatus::SERVER_START_FAILED,
                    {},
                    "MetaTrader file bridge requires a trade signal callback.");
                return;
            }

            {
                std::lock_guard<std::mutex> io_lock(m_io_mutex);
                if (m_running) {
                    return;
                }
                try {
                    ensure_runtime_started_locked(*config);
                } catch (const std::exception& ex) {
                    notify_status(BridgeStatus::SERVER_START_FAILED, {}, ex.what());
                    return;
                }
                m_running = true;
            }

            if (!m_task_manager.add_periodic_task(
                    "metatrader-file-bridge-poll",
                    config->poll_interval_ms,
                    [this](std::shared_ptr<utils::Task> task) {
                        if (task->is_shutdown()) return;
                        try {
                            process();
                        } catch (const std::exception& ex) {
                            notify_status(BridgeStatus::CONNECTION_ERROR, {}, ex.what());
                        } catch (...) {
                            notify_status(
                                BridgeStatus::CONNECTION_ERROR,
                                {},
                                "MetaTrader file bridge polling failed with an unknown exception.");
                        }
                    })) {
                {
                    std::lock_guard<std::mutex> io_lock(m_io_mutex);
                    m_running = false;
                }
                notify_status(
                    BridgeStatus::SERVER_START_FAILED,
                    {},
                    "Failed to schedule MetaTrader file bridge polling task.");
                return;
            }

            m_task_manager.run();
            notify_status(BridgeStatus::SERVER_STARTED, m_layout.root.u8string());
        }

        /// \brief Processes one bounded polling window.
        /// \details This method is public for deterministic tests and manual
        /// owner-loop integrations. `run()` calls it periodically.
        void process() {
            {
                std::lock_guard<std::mutex> io_lock(m_io_mutex);
                // File polling is intentionally single-flight. External
                // callbacks run outside this lock, but another poll must not
                // claim the same command window while the current one is in
                // its idempotency handoff window.
                if (m_processing) {
                    return;
                }
                m_processing = true;
            }

            try {
                process_impl();
            } catch (...) {
                std::lock_guard<std::mutex> io_lock(m_io_mutex);
                m_processing = false;
                throw;
            }

            {
                std::lock_guard<std::mutex> io_lock(m_io_mutex);
                m_processing = false;
            }
        }

        /// \brief Stops polling and reports bridge shutdown.
        void shutdown() override {
            bool was_running = false;
            {
                std::lock_guard<std::mutex> io_lock(m_io_mutex);
                was_running = m_running;
                m_running = false;
            }

            m_task_manager.shutdown();
            if (was_running) {
                notify_status(BridgeStatus::SERVER_STOPPED);
            }
        }

        /// \brief Returns the current resolved client root.
        std::filesystem::path client_root() const {
            std::lock_guard<std::mutex> io_lock(m_io_mutex);
            return m_layout.root;
        }

    private:
        static constexpr int jsonrpc_invalid_request = -32600;
        static constexpr int jsonrpc_method_not_found = -32601;
        static constexpr int jsonrpc_invalid_params = -32602;
        static constexpr int jsonrpc_internal_error = -32603;

        mutable std::mutex m_config_mutex;
        mutable std::mutex m_io_mutex;
        std::shared_ptr<RuntimeState> m_state;
        std::shared_ptr<MetaTraderFileBridgeConfig> m_config;
        utils::TaskManager m_task_manager;

        detail::FileTransportLayout m_layout;
        bool m_runtime_initialized = false;
        bool m_running = false;
        bool m_processing = false;
        std::uint64_t m_last_command_file_seq = 0;
        std::uint64_t m_command_scan_offset = 0;
        bool m_command_first_file_seq_known = false;
        std::uint64_t m_command_first_file_seq = 0;
        std::uint64_t m_next_event_file_seq = 1;
        std::uint64_t m_event_stream_seq = 0;
        std::uint64_t m_state_version = 0;
        std::string m_event_stream_id;
        std::map<std::string, detail::IdempotencyRecord> m_idempotency_records;
        std::map<std::string, detail::IdempotencyTombstone> m_idempotency_tombstones;
        std::map<std::string, std::string> m_request_id_index;
        std::uint64_t m_idempotency_processed_through_file_seq = 0;

        std::shared_ptr<MetaTraderFileBridgeConfig> get_config_or_throw() const {
            std::lock_guard<std::mutex> lock(m_config_mutex);
            if (!m_config) {
                throw std::invalid_argument("MetaTrader file bridge is not configured.");
            }
            return m_config;
        }

        signal_id_allocator_t get_signal_id_allocator() const {
            std::lock_guard<std::mutex> lock(m_state->mutex);
            return m_state->signal_id_allocator;
        }

        trade_signal_callback_t get_trade_signal_callback() const {
            std::lock_guard<std::mutex> lock(m_state->mutex);
            return m_state->trade_signal_callback;
        }

        std::shared_ptr<BaseAccountInfoData> get_account_info_snapshot() const {
            std::lock_guard<std::mutex> lock(m_state->mutex);
            return m_state->account_info;
        }

        void process_impl() {
            auto config = get_config_or_throw();
            auto allocator = get_signal_id_allocator();
            auto signal_callback = get_trade_signal_callback();

            detail::NdjsonSequenceReadResult window;

            {
                std::lock_guard<std::mutex> io_lock(m_io_mutex);
                ensure_runtime_started_locked(*config);
                window = read_commands_window_locked(*config);
            }

            for (const auto& malformed : window.malformed_records) {
                notify_signal_report(
                    detail::make_signal_report(
                        *config,
                        BridgeSignalReportStatus::INVALID,
                        "malformed_ndjson_record",
                        malformed.message,
                        nlohmann::json(),
                        nlohmann::json(),
                        {},
                        {},
                        {},
                        nlohmann::json{
                            {"start_offset", malformed.start_offset},
                            {"next_offset", malformed.next_offset}
                        }));
            }

            if (window.records.empty()) {
                std::lock_guard<std::mutex> io_lock(m_io_mutex);
                // Malformed lines have no domain file_seq, so after their
                // diagnostic reports are emitted the byte cursor is the only
                // progress marker available for this window.
                commit_command_scan_offset_locked(window.next_offset);
                return;
            }

            for (const auto& record : window.records) {
                std::vector<BridgeSignalReport> reports;
                std::vector<PendingSignalDispatch> pending_signals;
                {
                    std::lock_guard<std::mutex> io_lock(m_io_mutex);
                    handle_command_record_locked(
                        *config,
                        record,
                        static_cast<bool>(allocator),
                        static_cast<bool>(signal_callback),
                        reports,
                        pending_signals);
                }

                for (const auto& report : reports) {
                    notify_signal_report(report);
                }

                for (auto& pending : pending_signals) {
                    process_pending_signal_dispatch(
                        *config,
                        pending,
                        allocator,
                        signal_callback);
                }

                {
                    std::lock_guard<std::mutex> io_lock(m_io_mutex);
                    if (record.file_seq > m_last_command_file_seq) {
                        m_last_command_file_seq = record.file_seq;
                        write_commands_checkpoint_locked(*config);
                    }
                    // The cursor is a read-ahead optimization, not the source
                    // of truth. It advances only after the command has either
                    // produced a durable result or advanced the file_seq
                    // checkpoint, so an exception cannot skip a trade command.
                    commit_command_scan_offset_locked(record.next_offset);
                }
            }

            {
                std::lock_guard<std::mutex> io_lock(m_io_mutex);
                commit_command_scan_offset_locked(window.next_offset);
            }
        }

        void ensure_runtime_started_locked(const MetaTraderFileBridgeConfig& config) {
            if (m_runtime_initialized) {
                return;
            }

            m_layout = detail::make_layout(config);
            detail::ensure_runtime_directories(m_layout);
            m_last_command_file_seq =
                read_last_file_seq_checkpoint(m_layout.commands_checkpoint(), config.max_line_bytes);
            const auto last_event_reader_seq =
                read_last_file_seq_checkpoint(m_layout.events_checkpoint(), config.max_line_bytes);
            m_next_event_file_seq = detail::next_file_seq_after_checkpoint(
                m_layout.events_log(),
                last_event_reader_seq,
                config.max_line_bytes);
            m_command_scan_offset = 0;
            m_command_first_file_seq_known = false;
            m_command_first_file_seq = 0;
            m_event_stream_seq = 0;
            m_state_version = 0;
            m_event_stream_id =
                detail::stream_id(config) + "-" +
                std::to_string(detail::unix_time_ms()) + "-" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
            const auto idempotency_state =
                detail::read_idempotency_state(
                    m_layout.idempotency_state(),
                    config.max_idempotency_state_bytes);
            m_idempotency_records = idempotency_state.records;
            m_idempotency_tombstones = idempotency_state.tombstones;
            m_request_id_index = idempotency_state.request_index;
            m_idempotency_processed_through_file_seq =
                idempotency_state.processed_through_file_seq;
            m_last_command_file_seq = std::max(
                m_last_command_file_seq,
                m_idempotency_processed_through_file_seq);
            m_runtime_initialized = true;
        }

        detail::NdjsonSequenceReadResult read_commands_window_locked(
                const MetaTraderFileBridgeConfig& config) {
            std::error_code ec;
            const auto exists = std::filesystem::exists(m_layout.commands_log(), ec);
            if (ec) {
                throw std::runtime_error("Failed to inspect commands log: " + ec.message());
            }
            if (!exists) {
                m_command_scan_offset = 0;
                m_command_first_file_seq_known = false;
                m_command_first_file_seq = 0;
                return {};
            }

            const auto file_size = std::filesystem::file_size(m_layout.commands_log(), ec);
            if (ec) {
                throw std::runtime_error("Failed to read commands log size: " + ec.message());
            }
            if (file_size > config.max_command_log_bytes) {
                throw std::runtime_error("MetaTrader file bridge commands.ndjson exceeds configured byte limit.");
            }
            if (m_command_scan_offset > file_size) {
                m_command_scan_offset = 0;
                m_command_first_file_seq_known = false;
            }

            std::size_t prefix_scanned_records = 0;
            bool prefix_reset_scan_offset = false;
            if (m_command_scan_offset != 0) {
                // MQL owns commands.ndjson and may clear/regrow it. A stale
                // byte offset is safe only while the first valid file_seq still
                // matches the identity observed by previous polls. The prefix
                // scan skips malformed complete lines so a bad first line does
                // not force a full history rescan on every poll.
                const auto prefix = command_log_prefix_state_locked(config);
                prefix_scanned_records = prefix.scanned_records;
                if (prefix.first_file_seq) {
                    if (m_command_first_file_seq_known &&
                        *prefix.first_file_seq != m_command_first_file_seq) {
                        m_command_scan_offset = 0;
                        prefix_reset_scan_offset = true;
                    }
                    m_command_first_file_seq_known = true;
                    m_command_first_file_seq = *prefix.first_file_seq;
                    if (*prefix.first_file_seq > m_last_command_file_seq) {
                        m_command_scan_offset = 0;
                        prefix_reset_scan_offset = true;
                    }
                } else if (prefix.saw_complete_line &&
                           m_command_first_file_seq_known) {
                    m_command_scan_offset = 0;
                    prefix_reset_scan_offset = true;
                    m_command_first_file_seq_known = false;
                    m_command_first_file_seq = 0;
                }
            }

            auto scan_budget = config.max_scanned_records_per_poll;
            if (prefix_scanned_records != 0 &&
                !prefix_reset_scan_offset &&
                scan_budget > 1) {
                const auto consumed = std::min<std::size_t>(
                    prefix_scanned_records,
                    scan_budget - 1);
                scan_budget -= consumed;
            }
            auto window = detail::read_ndjson_sequence_window(
                m_layout.commands_log(),
                m_command_scan_offset,
                m_last_command_file_seq,
                config.max_line_bytes,
                scan_budget,
                config.max_returned_records_per_poll);
            if (!m_command_first_file_seq_known && !window.records.empty()) {
                m_command_first_file_seq_known = true;
                m_command_first_file_seq = window.records.front().file_seq;
            }
            return window;
        }

        void commit_command_scan_offset_locked(const std::uint64_t next_offset) {
            if (next_offset > m_command_scan_offset) {
                m_command_scan_offset = next_offset;
            }
        }

        struct CommandLogPrefixState {
            bool saw_complete_line = false;
            std::size_t scanned_records = 0;
            std::unique_ptr<std::uint64_t> first_file_seq;
        };

        CommandLogPrefixState command_log_prefix_state_locked(
                const MetaTraderFileBridgeConfig& config) const {
            auto batch = detail::read_ndjson_from_offset(
                m_layout.commands_log(),
                0,
                config.max_line_bytes,
                1);
            if (batch.records.empty() &&
                !batch.malformed_records.empty() &&
                config.max_scanned_records_per_poll > 1) {
                batch = detail::read_ndjson_from_offset(
                    m_layout.commands_log(),
                    0,
                    config.max_line_bytes,
                    config.max_scanned_records_per_poll);
            }
            CommandLogPrefixState state;
            state.saw_complete_line =
                !batch.records.empty() || !batch.malformed_records.empty();
            state.scanned_records = batch.scanned_records;
            if (!batch.records.empty()) {
                state.first_file_seq =
                    std::make_unique<std::uint64_t>(batch.records.front().file_seq);
            }
            return state;
        }

        static std::uint64_t read_last_file_seq_checkpoint(
                const std::filesystem::path& checkpoint,
                const std::size_t max_line_bytes) {
            std::error_code ec;
            const auto exists = std::filesystem::exists(checkpoint, ec);
            if (ec || !exists) {
                return 0;
            }

            const auto parsed = detail::read_json_file(checkpoint, max_line_bytes);
            if (!parsed.is_object() || !parsed.contains("last_file_seq")) {
                throw std::runtime_error(
                    "MetaTrader file bridge checkpoint is missing last_file_seq: " +
                    checkpoint.u8string());
            }
            const auto& value = parsed.at("last_file_seq");
            if (value.is_number_unsigned()) {
                return value.get<std::uint64_t>();
            }
            if (value.is_number_integer()) {
                const auto signed_value = value.get<std::int64_t>();
                if (signed_value >= 0) {
                    return static_cast<std::uint64_t>(signed_value);
                }
            }
            throw std::runtime_error(
                "MetaTrader file bridge checkpoint last_file_seq must be a non-negative integer: " +
                checkpoint.u8string());
        }

        nlohmann::json idempotency_records_document_locked() const {
            return detail::make_idempotency_state_document(
                m_idempotency_records,
                m_idempotency_tombstones,
                m_request_id_index,
                m_idempotency_processed_through_file_seq);
        }

        bool prune_expired_idempotency_tombstones_locked(
                const MetaTraderFileBridgeConfig& config,
                const std::uint64_t now_ms =
                    static_cast<std::uint64_t>(detail::unix_time_ms())) {
            bool changed = false;
            // Tombstones preserve retry/conflict semantics for evicted
            // completed operations only within the configured idempotency
            // horizon. In-doubt active records are never removed here.
            for (auto it = m_idempotency_tombstones.begin();
                 it != m_idempotency_tombstones.end();) {
                const auto evicted_at = it->second.evicted_at_ms;
                const auto age_ms = now_ms >= evicted_at ? now_ms - evicted_at : 0;
                if (evicted_at == 0 || age_ms >= config.idempotency_retention_ms) {
                    it = m_idempotency_tombstones.erase(it);
                    changed = true;
                } else {
                    ++it;
                }
            }
            return remove_stale_request_index_locked() || changed;
        }

        bool remove_stale_request_index_locked() {
            bool changed = false;
            for (auto it = m_request_id_index.begin();
                 it != m_request_id_index.end();) {
                if (m_idempotency_records.find(it->second) == m_idempotency_records.end() &&
                    m_idempotency_tombstones.find(it->second) == m_idempotency_tombstones.end()) {
                    it = m_request_id_index.erase(it);
                    changed = true;
                } else {
                    ++it;
                }
            }
            return changed;
        }

        void erase_oldest_idempotency_record_locked() {
            if (m_idempotency_records.empty()) {
                return;
            }

            auto oldest = m_idempotency_records.end();
            for (auto it = m_idempotency_records.begin();
                 it != m_idempotency_records.end();
                 ++it) {
                if (detail::string_value(it->second.result, "status") == "in_doubt") {
                    continue;
                }
                if (oldest == m_idempotency_records.end()) {
                    oldest = it;
                    continue;
                }
                const auto lhs = it->second.updated_at_ms != 0
                    ? it->second.updated_at_ms
                    : it->second.created_at_ms;
                const auto rhs = oldest->second.updated_at_ms != 0
                    ? oldest->second.updated_at_ms
                    : oldest->second.created_at_ms;
                if (lhs < rhs) {
                    oldest = it;
                }
            }
            if (oldest == m_idempotency_records.end()) {
                // Dropping an in-doubt record could create a duplicate trade
                // after restart. Once the registry is full of unresolved
                // handoffs, the bridge must fail closed.
                throw std::runtime_error(
                    "MetaTrader file bridge idempotency state is full of in_doubt operations.");
            }
            m_idempotency_tombstones[oldest->first] = detail::IdempotencyTombstone{
                oldest->second.payload_fingerprint,
                oldest->second.result,
                static_cast<std::uint64_t>(detail::unix_time_ms())
            };
            m_idempotency_records.erase(oldest);
        }

        void prune_idempotency_records_locked(const MetaTraderFileBridgeConfig& config) {
            while (m_idempotency_records.size() > config.max_idempotency_records) {
                erase_oldest_idempotency_record_locked();
            }
        }

        void write_idempotency_records_locked(const MetaTraderFileBridgeConfig& config) {
            prune_expired_idempotency_tombstones_locked(config);
            prune_idempotency_records_locked(config);
            auto document = idempotency_records_document_locked();
            auto text = document.dump(2);
            while (text.size() > config.max_idempotency_state_bytes &&
                   m_idempotency_records.size() > 1) {
                erase_oldest_idempotency_record_locked();
                prune_expired_idempotency_tombstones_locked(config);
                document = idempotency_records_document_locked();
                text = document.dump(2);
            }
            if (text.size() > config.max_idempotency_state_bytes) {
                throw std::runtime_error(
                    "MetaTrader file bridge idempotency state exceeds configured byte limit.");
            }
            detail::write_text_file_atomic(m_layout.idempotency_state(), text);
        }

        static std::string idempotency_storage_key(
                const std::string& method,
                const std::string& idempotency_key) {
            if (idempotency_key.empty()) {
                return {};
            }
            return method + "\n" + idempotency_key;
        }

        static std::string request_storage_key(
                const std::string& method,
                const nlohmann::json& id) {
            if (id.is_null()) {
                return {};
            }
            return method + "\n" + id.dump(-1);
        }

        static std::string payload_fingerprint(const nlohmann::json& params) {
            auto canonical = params;
            auto context_it = canonical.find("context");
            if (context_it != canonical.end() && context_it->is_object()) {
                // Admission metadata may legitimately change between retries.
                // The idempotency fingerprint must describe the business
                // operation, not the transport attempt envelope.
                context_it->erase("valid_until_ms");
                context_it->erase("client_created_at_ms");
            }
            return canonical.dump(-1);
        }

        static nlohmann::json make_in_doubt_result(
                const MetaTraderFileBridgeConfig& config,
                const detail::NdjsonRecord& record,
                const SignalId signal_id) {
            return nlohmann::json{
                {"status", "in_doubt"},
                {"final", false},
                {"operation_id", "file:" + std::to_string(config.bridge_id) + ":" + std::to_string(record.file_seq)},
                {"signal_ref", {
                    {"signal_id", std::to_string(signal_id)}
                }},
                {"reason", {
                    {"code", "operation_handoff_in_doubt"},
                    {"message", "The command was durably marked before handoff; bridge restart must not dispatch it again automatically."}
                }}
            };
        }

        std::uint64_t next_event_stream_seq_locked() {
            if (m_event_stream_seq == std::numeric_limits<std::uint64_t>::max()) {
                throw std::overflow_error("MetaTrader file bridge event seq overflow.");
            }
            return ++m_event_stream_seq;
        }

        std::string next_event_id_locked(const char* prefix) {
            return std::string("evt-") +
                   prefix +
                   "-" +
                   std::to_string(m_next_event_file_seq) +
                   "-" +
                   std::to_string(m_event_stream_seq + 1);
        }

        void append_event_locked(nlohmann::json document) {
            detail::append_json_line(
                m_layout.events_log(),
                detail::with_file_seq(std::move(document), m_next_event_file_seq++),
                get_config_or_throw()->max_line_bytes);
        }

        void write_commands_checkpoint_locked(const MetaTraderFileBridgeConfig& config) {
            if (m_last_command_file_seq > m_idempotency_processed_through_file_seq) {
                // The high-water mark protects against replay when
                // commands.checkpoint.json is lost. Keep memory and disk in
                // sync: if the idempotency state write fails, roll back the
                // in-memory high-water update too.
                const auto records_backup = m_idempotency_records;
                const auto tombstones_backup = m_idempotency_tombstones;
                const auto request_index_backup = m_request_id_index;
                const auto processed_backup = m_idempotency_processed_through_file_seq;
                m_idempotency_processed_through_file_seq = m_last_command_file_seq;
                try {
                    write_idempotency_records_locked(config);
                } catch (...) {
                    m_idempotency_records = records_backup;
                    m_idempotency_tombstones = tombstones_backup;
                    m_request_id_index = request_index_backup;
                    m_idempotency_processed_through_file_seq = processed_backup;
                    throw;
                }
            }
            detail::write_json_file_atomic(
                m_layout.commands_checkpoint(),
                detail::make_log_checkpoint(m_last_command_file_seq));
        }

        void write_state_snapshot_locked(const MetaTraderFileBridgeConfig& config) {
            auto account = get_account_info_snapshot();
            nlohmann::json accounts = nlohmann::json::array();
            if (account) {
                accounts.push_back(detail::account_snapshot_json(*account));
            }

            detail::write_json_file_atomic(
                m_layout.state_snapshot(),
                detail::make_state_snapshot(
                    ++m_state_version,
                    detail::unix_time_ms(),
                    account ? detail::connection_string(*account) : std::string("unknown"),
                    std::move(accounts),
                    nlohmann::json::array()));
            (void)config;
        }

        void notify_status(
                BridgeStatus status,
                std::string connection_id = {},
                std::string message = {}) const {
            bridge_status_callback_t callback;
            {
                std::lock_guard<std::mutex> lock(m_state->mutex);
                callback = m_state->status_callback;
            }
            if (callback) {
                callback(BridgeStatusUpdate(
                    status,
                    std::move(connection_id),
                    std::move(message)));
            }
        }

        void notify_signal_report(const BridgeSignalReport& report) const {
            signal_report_callback_t callback;
            {
                std::lock_guard<std::mutex> lock(m_state->mutex);
                callback = m_state->signal_report_callback;
            }
            if (callback) {
                callback(report);
            }
        }

        void handle_command_record_locked(
                const MetaTraderFileBridgeConfig& config,
                const detail::NdjsonRecord& record,
                const bool has_signal_allocator,
                const bool has_trade_signal_callback,
                std::vector<BridgeSignalReport>& reports,
                std::vector<PendingSignalDispatch>& pending_signals) {
            const auto& document = record.document;
            const auto id = document.contains("id") ? document.at("id") : nlohmann::json(nullptr);
            try {
                if (!document.is_object() ||
                    document.value("jsonrpc", std::string()) != "2.0" ||
                    !document.contains("method") ||
                    !document.at("method").is_string()) {
                    append_rpc_error_locked(
                        id,
                        jsonrpc_invalid_request,
                        "Invalid JSON-RPC request.");
                    reports.push_back(detail::make_signal_report(
                        config,
                        BridgeSignalReportStatus::INVALID,
                        "invalid_jsonrpc_request",
                        "MetaTrader file command is not a valid JSON-RPC request.",
                        document));
                    return;
                }

                const auto method = document.at("method").get<std::string>();
                const auto params =
                    document.contains("params") ? document.at("params") : nlohmann::json::object();

                if (method == "protocol.hello") {
                    append_rpc_result_locked(
                        id,
                        nlohmann::json{
                            {"status", "ok"},
                            {"protocol_versions", nlohmann::json::array({"1.0"})},
                            {"supported_methods", nlohmann::json::array({
                                "protocol.hello",
                                "account.balance.get",
                                "signal.submit",
                                "trade.open"
                            })},
                            {"transport", "metatrader_file"}
                        });
                    return;
                }

                if (method == "account.balance.get") {
                    handle_balance_get_locked(id);
                    return;
                }

                if (method == "signal.submit" || method == "trade.open") {
                    handle_signal_command_locked(
                        config,
                        record,
                        id,
                        method,
                        params,
                        has_signal_allocator,
                        has_trade_signal_callback,
                        reports,
                        pending_signals);
                    return;
                }

                append_rpc_error_locked(
                    id,
                    jsonrpc_method_not_found,
                    "Unsupported MetaTrader file bridge method.",
                    nlohmann::json{{"method", method}});
                reports.push_back(detail::make_signal_report(
                    config,
                    BridgeSignalReportStatus::IGNORED,
                    "unsupported_method",
                    "MetaTrader file command method is unsupported.",
                    document,
                    params,
                    detail::json_id_to_string(id)));
            } catch (const std::exception& ex) {
                append_rpc_error_locked(
                    id,
                    jsonrpc_internal_error,
                    ex.what());
                reports.push_back(detail::make_signal_report(
                    config,
                    BridgeSignalReportStatus::INTAKE_ERROR,
                    "command_processing_error",
                    ex.what(),
                    document));
            }
        }

        void handle_balance_get_locked(const nlohmann::json& id) {
            auto account = get_account_info_snapshot();
            if (!account) {
                append_rpc_result_locked(
                    id,
                    nlohmann::json{
                        {"status", "unavailable"},
                        {"final", true},
                        {"reason", {
                            {"code", "account_snapshot_unavailable"},
                            {"message", "No account snapshot is available."}
                        }}
                    });
                return;
            }

            append_rpc_result_locked(
                id,
                nlohmann::json{
                    {"status", "completed"},
                    {"final", true},
                    {"account", detail::account_snapshot_json(*account)}
                });
        }

        void handle_signal_command_locked(
                const MetaTraderFileBridgeConfig& config,
                const detail::NdjsonRecord& record,
                const nlohmann::json& id,
                const std::string& method,
                const nlohmann::json& params,
                const bool has_signal_allocator,
                const bool has_trade_signal_callback,
                std::vector<BridgeSignalReport>& reports,
                std::vector<PendingSignalDispatch>& pending_signals) {
            prune_expired_idempotency_tombstones_locked(config);
            const auto rpc_request_key = request_storage_key(method, id);

            const auto idempotency_key = detail::context_idempotency_key(params);
            if (idempotency_key.empty()) {
                append_rpc_error_locked(
                    id,
                    jsonrpc_invalid_params,
                    "MetaTrader file bridge trade-affecting commands require context.idempotency_key.");
                reports.push_back(detail::make_signal_report(
                    config,
                    BridgeSignalReportStatus::INVALID,
                    "missing_idempotency_key",
                    "MetaTrader file bridge trade-affecting commands require context.idempotency_key.",
                    record.document,
                    params,
                    detail::json_id_to_string(id)));
                return;
            }

            const auto storage_key = idempotency_storage_key(method, idempotency_key);
            const auto fingerprint = payload_fingerprint(params);
            if (!rpc_request_key.empty()) {
                const auto request_it = m_request_id_index.find(rpc_request_key);
                if (request_it != m_request_id_index.end()) {
                    if (request_it->second != storage_key) {
                        const nlohmann::json conflict = {
                            {"status", "rejected"},
                            {"final", true},
                            {"reason", {
                                {"code", "idempotency_conflict"},
                                {"message", "The same JSON-RPC id was used with a different operation."}
                            }}
                        };
                        append_rpc_result_locked(id, conflict);
                        reports.push_back(detail::make_signal_report(
                            config,
                            BridgeSignalReportStatus::REJECTED,
                            "idempotency_conflict",
                            "MetaTrader file command JSON-RPC id conflicts with an earlier operation.",
                            record.document,
                            params,
                            detail::json_id_to_string(id),
                            idempotency_key));
                        return;
                    }

                    const auto record_it = m_idempotency_records.find(request_it->second);
                    const auto tombstone_it =
                        m_idempotency_tombstones.find(request_it->second);
                    if (record_it == m_idempotency_records.end() &&
                        tombstone_it == m_idempotency_tombstones.end()) {
                        m_request_id_index.erase(request_it);
                    }
                }
            }

            std::unique_ptr<TradeSignal> signal;
            try {
                signal = detail::parse_signal_params(params, method == "trade.open");
            } catch (const std::exception& ex) {
                append_rpc_error_locked(
                    id,
                    jsonrpc_invalid_params,
                    ex.what());
                reports.push_back(detail::make_signal_report(
                    config,
                    BridgeSignalReportStatus::INVALID,
                    "invalid_params",
                    ex.what(),
                    record.document,
                    params,
                    detail::json_id_to_string(id)));
                return;
            }

            const auto existing = m_idempotency_records.find(storage_key);
            if (existing != m_idempotency_records.end()) {
                if (existing->second.payload_fingerprint == fingerprint) {
                    if (!rpc_request_key.empty()) {
                        m_request_id_index[rpc_request_key] = storage_key;
                    }
                    append_rpc_result_locked(id, existing->second.result);
                    return;
                }

                const nlohmann::json conflict = {
                    {"status", "rejected"},
                    {"final", true},
                    {"reason", {
                        {"code", "idempotency_conflict"},
                        {"message", "The same idempotency_key was used with a different payload."}
                    }}
                };
                append_rpc_result_locked(id, conflict);
                reports.push_back(detail::make_signal_report(
                    config,
                    BridgeSignalReportStatus::REJECTED,
                    "idempotency_conflict",
                    "MetaTrader file command idempotency key conflicts with an earlier payload.",
                    record.document,
                    params,
                    detail::json_id_to_string(id),
                    idempotency_key,
                    detail::clone_candidate_signal(signal)));
                return;
            }

            const auto tombstone = m_idempotency_tombstones.find(storage_key);
            if (tombstone != m_idempotency_tombstones.end()) {
                if (tombstone->second.payload_fingerprint == fingerprint) {
                    if (!rpc_request_key.empty()) {
                        m_request_id_index[rpc_request_key] = storage_key;
                    }
                    append_rpc_result_locked(id, tombstone->second.result);
                    return;
                }

                const nlohmann::json conflict = {
                    {"status", "rejected"},
                    {"final", true},
                    {"reason", {
                        {"code", "idempotency_conflict"},
                        {"message", "The same idempotency_key was used with a different payload."}
                    }}
                };
                append_rpc_result_locked(id, conflict);
                reports.push_back(detail::make_signal_report(
                    config,
                    BridgeSignalReportStatus::REJECTED,
                    "idempotency_conflict",
                    "MetaTrader file command idempotency tombstone conflicts with a new payload.",
                    record.document,
                    params,
                    detail::json_id_to_string(id),
                    idempotency_key,
                    detail::clone_candidate_signal(signal)));
                return;
            }

            signal->bridge_id = config.bridge_id;
            const auto& context = detail::object_member_or_empty(params, "context");
            if (!context.contains("valid_until_ms")) {
                append_rpc_error_locked(
                    id,
                    jsonrpc_invalid_params,
                    "MetaTrader file bridge trade-affecting commands require context.valid_until_ms.");
                reports.push_back(detail::make_signal_report(
                    config,
                    BridgeSignalReportStatus::INVALID,
                    "missing_valid_until_ms",
                    "MetaTrader file bridge trade-affecting commands require context.valid_until_ms.",
                    record.document,
                    params,
                    detail::json_id_to_string(id),
                    idempotency_key,
                    detail::clone_candidate_signal(signal)));
                return;
            }

            std::int64_t valid_until_ms = 0;
            try {
                valid_until_ms = detail::context_valid_until_ms(params);
            } catch (const std::exception& ex) {
                append_rpc_error_locked(
                    id,
                    jsonrpc_invalid_params,
                    ex.what());
                reports.push_back(detail::make_signal_report(
                    config,
                    BridgeSignalReportStatus::INVALID,
                    "invalid_valid_until_ms",
                    ex.what(),
                    record.document,
                    params,
                    detail::json_id_to_string(id),
                    idempotency_key,
                    detail::clone_candidate_signal(signal)));
                return;
            }
            if (valid_until_ms <= 0) {
                append_rpc_error_locked(
                    id,
                    jsonrpc_invalid_params,
                    "MetaTrader file bridge context.valid_until_ms must be a positive Unix millisecond timestamp.");
                reports.push_back(detail::make_signal_report(
                    config,
                    BridgeSignalReportStatus::INVALID,
                    "invalid_valid_until_ms",
                    "MetaTrader file bridge context.valid_until_ms must be a positive Unix millisecond timestamp.",
                    record.document,
                    params,
                    detail::json_id_to_string(id),
                    idempotency_key,
                    detail::clone_candidate_signal(signal)));
                return;
            }
            if (valid_until_ms > 0 && detail::unix_time_ms() > valid_until_ms) {
                const nlohmann::json result = {
                    {"status", "rejected"},
                    {"final", true},
                    {"reason", {
                        {"code", "stale_request"},
                        {"message", "Command valid_until_ms is in the past."}
                    }}
                };
                append_rpc_result_locked(id, result);
                store_idempotency_result_locked(
                    config,
                    storage_key,
                    rpc_request_key,
                    fingerprint,
                    result);
                reports.push_back(detail::make_signal_report(
                    config,
                    BridgeSignalReportStatus::REJECTED,
                    "stale_request",
                    "MetaTrader file command expired before processing.",
                    record.document,
                    params,
                    detail::json_id_to_string(id),
                    signal->unique_hash,
                    detail::clone_candidate_signal(signal)));
                return;
            }

            if (!has_signal_allocator) {
                append_rpc_error_locked(
                    id,
                    jsonrpc_internal_error,
                    "MetaTrader file bridge signal ID allocator is not configured.");
                reports.push_back(detail::make_signal_report(
                    config,
                    BridgeSignalReportStatus::INTAKE_ERROR,
                    "missing_signal_id_allocator",
                    "MetaTrader file bridge signal ID allocator is not configured.",
                    record.document,
                    params,
                    detail::json_id_to_string(id),
                    signal->unique_hash,
                    detail::clone_candidate_signal(signal)));
                return;
            }

            if (!has_trade_signal_callback) {
                const nlohmann::json result = {
                    {"status", "rejected"},
                    {"final", true},
                    {"reason", {
                        {"code", "signal_handler_unavailable"},
                        {"message", "MetaTrader file bridge trade signal callback is not configured."}
                    }}
                };
                append_rpc_result_locked(id, result);
                store_idempotency_result_locked(
                    config,
                    storage_key,
                    rpc_request_key,
                    fingerprint,
                    result);
                reports.push_back(detail::make_signal_report(
                    config,
                    BridgeSignalReportStatus::REJECTED,
                    "signal_handler_unavailable",
                    "MetaTrader file bridge trade signal callback is not configured.",
                    record.document,
                    params,
                    detail::json_id_to_string(id),
                    signal->unique_hash,
                    detail::clone_candidate_signal(signal)));
                return;
            }

            pending_signals.push_back(PendingSignalDispatch{
                record,
                id,
                params,
                std::move(signal),
                idempotency_key,
                storage_key,
                rpc_request_key,
                fingerprint
            });
        }

        void process_pending_signal_dispatch(
                const MetaTraderFileBridgeConfig& config,
                PendingSignalDispatch& pending,
                const signal_id_allocator_t& allocator,
                const trade_signal_callback_t& callback) {
            try {
                pending.signal->signal_id = allocator();
            } catch (const std::exception& ex) {
                append_pending_signal_error(
                    config,
                    pending,
                    jsonrpc_internal_error,
                    "MetaTrader file bridge signal ID allocator failed.",
                    "signal_id_allocation_exception",
                    ex.what());
                return;
            }

            if (pending.signal->signal_id == 0) {
                append_pending_signal_error(
                    config,
                    pending,
                    jsonrpc_internal_error,
                    "MetaTrader file bridge could not allocate signal ID.",
                    "signal_id_allocation_failed",
                    "MetaTrader file bridge could not allocate signal ID.");
                return;
            }

            auto candidate = detail::clone_candidate_signal(pending.signal);
            {
                std::lock_guard<std::mutex> io_lock(m_io_mutex);
                // Persist the handoff marker before invoking user code. If the
                // process dies during or after the callback, restart returns
                // the in_doubt result instead of dispatching the trade again.
                store_idempotency_result_locked(
                    config,
                    pending.idempotency_storage_key,
                    pending.request_storage_key,
                    pending.payload_fingerprint,
                    make_in_doubt_result(config, pending.record, candidate->signal_id));
            }

            try {
                callback(std::move(pending.signal));
            } catch (const std::exception& ex) {
                append_pending_signal_error(
                    config,
                    pending,
                    jsonrpc_internal_error,
                    ex.what(),
                    "signal_callback_exception",
                    ex.what(),
                    candidate);
                return;
            } catch (...) {
                append_pending_signal_error(
                    config,
                    pending,
                    jsonrpc_internal_error,
                    "MetaTrader file bridge signal callback failed with an unknown exception.",
                    "signal_callback_exception",
                    "MetaTrader file bridge signal callback failed with an unknown exception.",
                    candidate);
                return;
            }

            const nlohmann::json result = {
                {"status", "accepted"},
                {"final", false},
                {"operation_id", "file:" + std::to_string(config.bridge_id) + ":" + std::to_string(pending.record.file_seq)},
                {"signal_ref", {
                    {"signal_id", std::to_string(candidate->signal_id)}
                }}
            };

            {
                std::lock_guard<std::mutex> io_lock(m_io_mutex);
                store_idempotency_result_locked(
                    config,
                    pending.idempotency_storage_key,
                    pending.request_storage_key,
                    pending.payload_fingerprint,
                    result);
                append_rpc_result_locked(pending.id, result);
            }
        }

        void append_pending_signal_error(
                const MetaTraderFileBridgeConfig& config,
                const PendingSignalDispatch& pending,
                const int error_code,
                const std::string& rpc_message,
                std::string report_code,
                std::string report_message,
                std::shared_ptr<const TradeSignal> candidate = {}) {
            {
                std::lock_guard<std::mutex> io_lock(m_io_mutex);
                append_rpc_error_locked(
                    pending.id,
                    error_code,
                    rpc_message);
            }

            if (!candidate) {
                candidate = detail::clone_candidate_signal(pending.signal);
            }
            notify_signal_report(detail::make_signal_report(
                config,
                BridgeSignalReportStatus::INTAKE_ERROR,
                std::move(report_code),
                std::move(report_message),
                pending.record.document,
                pending.params,
                detail::json_id_to_string(pending.id),
                pending.signal ? pending.signal->unique_hash : std::string(),
                std::move(candidate)));
        }

        void store_idempotency_result_locked(
                const MetaTraderFileBridgeConfig& config,
                const std::string& storage_key,
                const std::string& request_key,
                const std::string& fingerprint,
                const nlohmann::json& result) {
            if (storage_key.empty()) {
                return;
            }
            // State mutations are copy-on-write from the caller's perspective.
            // A failed atomic file write must not leave transient records in
            // memory, otherwise a later poll could observe state that never
            // survived a restart.
            const auto records_backup = m_idempotency_records;
            const auto tombstones_backup = m_idempotency_tombstones;
            const auto request_index_backup = m_request_id_index;
            const auto processed_backup = m_idempotency_processed_through_file_seq;
            if (!request_key.empty()) {
                m_request_id_index[request_key] = storage_key;
            }
            const auto now = static_cast<std::uint64_t>(detail::unix_time_ms());
            auto existing = m_idempotency_records.find(storage_key);
            if (existing == m_idempotency_records.end()) {
                m_idempotency_records[storage_key] = detail::IdempotencyRecord{
                    fingerprint,
                    result,
                    now,
                    now
                };
            } else {
                existing->second.payload_fingerprint = fingerprint;
                existing->second.result = result;
                if (existing->second.created_at_ms == 0) {
                    existing->second.created_at_ms = now;
                }
                existing->second.updated_at_ms = now;
            }
            try {
                write_idempotency_records_locked(config);
            } catch (...) {
                m_idempotency_records = records_backup;
                m_idempotency_tombstones = tombstones_backup;
                m_request_id_index = request_index_backup;
                m_idempotency_processed_through_file_seq = processed_backup;
                throw;
            }
        }

        void append_rpc_result_locked(nlohmann::json id, nlohmann::json result) {
            append_event_locked(detail::make_jsonrpc_result(std::move(id), std::move(result)));
        }

        void append_rpc_error_locked(
                nlohmann::json id,
                const int code,
                std::string message,
                nlohmann::json data = nlohmann::json()) {
            append_event_locked(
                detail::make_jsonrpc_error(
                    std::move(id),
                    code,
                    std::move(message),
                    std::move(data)));
        }

    };

} // namespace optionx::bridges::metatrader_file

#endif // OPTIONX_HEADER_BRIDGES_METATRADER_FILE_META_TRADER_FILE_BRIDGE_HPP_INCLUDED
