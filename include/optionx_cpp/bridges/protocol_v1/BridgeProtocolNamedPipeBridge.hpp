#pragma once
#ifndef OPTIONX_HEADER_BRIDGES_PROTOCOL_V1_BRIDGE_PROTOCOL_NAMED_PIPE_BRIDGE_HPP_INCLUDED
#define OPTIONX_HEADER_BRIDGES_PROTOCOL_V1_BRIDGE_PROTOCOL_NAMED_PIPE_BRIDGE_HPP_INCLUDED

/// \file BridgeProtocolNamedPipeBridge.hpp
/// \brief Defines the Bridge Protocol v1 named-pipe bridge.

namespace optionx::bridges::protocol_v1 {

    /// \class BridgeProtocolNamedPipeBridge
    /// \brief Serves JSON-RPC Bridge Protocol v1 commands over a local named pipe.
    class BridgeProtocolNamedPipeBridge final : public BaseBridge {
    private:
        struct StoredOperation {
            std::string fingerprint;
            nlohmann::json result;
            std::int64_t completed_at_ms = 0;
            std::size_t byte_size = 0;
            bool dispatching = false;
        };

        enum class RuntimePhase {
            Stopped,
            Starting,
            Running,
            Stopping
        };

        struct RuntimeState {
            std::mutex mutex;
            std::condition_variable lifecycle_cv;
            std::shared_ptr<BridgeProtocolNamedPipeConfig> config;
            bridge_status_callback_t status_callback;
            BaseBridge::trade_signal_callback_t trade_signal_callback;
            BaseBridge::signal_report_callback_t signal_report_callback;
            BaseBridge::signal_id_allocator_t signal_id_allocator;
            std::shared_ptr<BaseAccountInfoData> account_info;
            std::unordered_map<std::string, StoredOperation> operations;
            std::unordered_map<std::string, std::uint64_t> event_revisions;
            std::deque<std::string> operation_order;
            std::string stream_id;
            std::size_t operation_cache_bytes = 0;
            std::size_t active_transport_callbacks = 0;
            std::uint64_t event_seq = 0;
            RuntimePhase phase = RuntimePhase::Stopped;
            bool stop_notified = false;
            bool pending_callback_shutdown = false;
            bool transport_callback_admission_closed = false;
#           if defined(_WIN32)
            std::shared_ptr<SimpleNamedPipe::NamedPipeServer> server;
            std::set<int> client_ids;
#           endif
        };

        inline static thread_local std::vector<const RuntimeState*> s_callback_stack;

        class CallbackScope final {
        public:
            explicit CallbackScope(std::shared_ptr<RuntimeState> state)
                : m_state(std::move(state)),
                  m_identity(m_state.get()) {
                if (m_identity) {
                    s_callback_stack.push_back(m_identity);
                }
            }

            ~CallbackScope() {
                if (!m_identity) {
                    return;
                }
                if (!s_callback_stack.empty() &&
                    s_callback_stack.back() == m_identity) {
                    s_callback_stack.pop_back();
                    return;
                }
                const auto it = std::find(
                    s_callback_stack.rbegin(),
                    s_callback_stack.rend(),
                    m_identity);
                if (it != s_callback_stack.rend()) {
                    s_callback_stack.erase(std::next(it).base());
                }
            }

        private:
            std::shared_ptr<RuntimeState> m_state;
            const RuntimeState* m_identity = nullptr;
        };

        class TransportCallbackScope final {
        public:
            explicit TransportCallbackScope(std::shared_ptr<RuntimeState> state)
                : m_state(std::move(state)),
                  m_identity(m_state.get()) {
                if (!m_identity) {
                    return;
                }
                {
                    std::lock_guard<std::mutex> lock(m_state->mutex);
                    if (m_state->transport_callback_admission_closed ||
                        m_state->phase != RuntimePhase::Running) {
                        return;
                    }
                    ++m_state->active_transport_callbacks;
                    m_admitted = true;
                }
                s_callback_stack.push_back(m_identity);
            }

            ~TransportCallbackScope() {
                if (!m_admitted) {
                    return;
                }
                if (!s_callback_stack.empty() &&
                    s_callback_stack.back() == m_identity) {
                    s_callback_stack.pop_back();
                } else {
                    const auto it = std::find(
                        s_callback_stack.rbegin(),
                        s_callback_stack.rend(),
                        m_identity);
                    if (it != s_callback_stack.rend()) {
                        s_callback_stack.erase(std::next(it).base());
                    }
                }

                bool should_drain = false;
                {
                    std::lock_guard<std::mutex> lock(m_state->mutex);
                    if (m_state->active_transport_callbacks > 0) {
                        --m_state->active_transport_callbacks;
                    }
                    should_drain =
                        m_state->active_transport_callbacks == 0 &&
                        m_state->pending_callback_shutdown;
                }
                if (should_drain) {
                    BridgeProtocolNamedPipeBridge::drain_pending_callback_shutdown(m_state);
                }
            }

            bool admitted() const noexcept {
                return m_admitted;
            }

        private:
            std::shared_ptr<RuntimeState> m_state;
            const RuntimeState* m_identity = nullptr;
            bool m_admitted = false;
        };

    public:
        /// \brief Constructs an unconfigured named-pipe protocol bridge.
        BridgeProtocolNamedPipeBridge()
            : m_state(std::make_shared<RuntimeState>()) {}

        /// \brief Stops the named-pipe transport before destruction.
        ~BridgeProtocolNamedPipeBridge() override {
            shutdown();
        }

#ifdef OPTIONX_ENABLE_BRIDGE_PROTOCOL_TEST_HOOKS
#       if defined(_WIN32)
        /// \brief Stops the underlying server without going through bridge shutdown.
        void simulate_unexpected_server_stop_for_test() {
            std::shared_ptr<SimpleNamedPipe::NamedPipeServer> server;
            {
                std::lock_guard<std::mutex> lock(m_state->mutex);
                server = m_state->server;
            }
            if (server) {
                std::thread([server = std::move(server)]() mutable {
                    server->stop();
                }).detach();
            }
        }
#       endif
#endif

        /// \brief Configures the bridge with Bridge Protocol v1 named-pipe settings.
        bool configure(std::unique_ptr<IBridgeConfig> config) override {
            if (!config) return false;

            const auto* typed =
                dynamic_cast<const BridgeProtocolNamedPipeConfig*>(config.get());
            if (!typed) {
                config->dispatch_callbacks(
                    false,
                    "Invalid Bridge Protocol v1 named-pipe config type.");
                return false;
            }

            auto next_config = std::make_shared<BridgeProtocolNamedPipeConfig>(*typed);
            const auto validation = next_config->validate();
            config->dispatch_callbacks(validation.first, validation.second);
            if (!validation.first) {
                return false;
            }

            std::unique_lock<std::mutex> lock(m_state->mutex);
            while (m_state->phase == RuntimePhase::Stopping) {
                m_state->lifecycle_cv.wait(lock);
            }
            if (m_state->phase != RuntimePhase::Stopped) {
                config->dispatch_callbacks(
                    false,
                    "Bridge Protocol v1 named-pipe bridge cannot be reconfigured while running.");
                return false;
            }
            m_state->config = std::move(next_config);
            m_state->stream_id = make_stream_id();
            return true;
        }

        /// \brief Returns the status callback slot.
        bridge_status_callback_t& on_status_update() override {
            return m_state->status_callback;
        }

        /// \brief Returns the trade signal callback slot.
        trade_signal_callback_t& on_trade_signal() override {
            return m_state->trade_signal_callback;
        }

        /// \brief Returns the signal diagnostic callback slot.
        signal_report_callback_t& on_signal_report() override {
            return m_state->signal_report_callback;
        }

        /// \brief Returns the signal ID allocator slot.
        signal_id_allocator_t& on_signal_id() override {
            return m_state->signal_id_allocator;
        }

        /// \brief Updates the account snapshot used by `account.balance.get`.
        void update_account_info(const AccountInfoUpdate& info) override {
            if (!info.account_info) return;

            std::shared_ptr<BridgeProtocolNamedPipeConfig> config;
            {
                std::lock_guard<std::mutex> lock(m_state->mutex);
                m_state->account_info = info.account_info;
                config = m_state->config;
            }
            if (!config) {
                return;
            }

            broadcast_notification(make_balance_updated_notification(*config, *info.account_info));
        }

        /// \brief Broadcasts `trade.updated` notifications to connected named-pipe clients.
        void update_trade_result(
                const TradeRequest& request,
                const TradeResult& result) override {
            auto config = get_config();
            if (!config) {
                return;
            }
            const auto now = metatrader_file::detail::unix_time_ms();
            const auto coord = next_event_coordinate(trade_revision_key(request, result));
            auto notification = metatrader_file::detail::make_trade_updated_notification(
                make_event_id("trade", coord.stream_id, coord.seq),
                source_uri(*config),
                coord.stream_id,
                coord.seq,
                result.close_date > 0 ? result.close_date : now,
                now,
                request,
                result,
                coord.revision);
            broadcast_notification(std::move(notification));
        }

        /// \brief Starts the named-pipe transport.
        void run() override {
            auto config = get_config_or_throw();
#           if defined(_WIN32)
            {
                std::unique_lock<std::mutex> lock(m_state->mutex);
                while (m_state->phase == RuntimePhase::Starting ||
                       m_state->phase == RuntimePhase::Stopping) {
                    m_state->lifecycle_cv.wait(lock);
                }
                if (m_state->phase == RuntimePhase::Running) {
                    return;
                }
            }

            SimpleNamedPipe::ServerConfig server_config(
                config->named_pipe,
                config->buffer_size,
                config->pipe_timeout_ms);
            server_config.write_limits.max_message_size = max_pipe_frame_bytes(*config);
            auto server = std::make_shared<SimpleNamedPipe::NamedPipeServer>(server_config);
            configure_server_callbacks(server);

            {
                std::lock_guard<std::mutex> lock(m_state->mutex);
                if (m_state->phase != RuntimePhase::Stopped) {
                    return;
                }
                m_state->phase = RuntimePhase::Starting;
                m_state->server = server;
                m_state->client_ids.clear();
                m_state->operation_order.clear();
                m_state->operations.clear();
                m_state->operation_cache_bytes = 0;
                m_state->event_revisions.clear();
                m_state->event_seq = 0;
                m_state->stream_id = make_stream_id();
                m_state->stop_notified = false;
                m_state->pending_callback_shutdown = false;
                m_state->transport_callback_admission_closed = false;
                m_state->lifecycle_cv.notify_all();
            }

            try {
                server->start(true);
            } catch (const std::exception& ex) {
                clear_runtime_after_start_failure(server.get());
                notify_status(BridgeStatus::SERVER_START_FAILED, {}, ex.what());
            }
#           else
            (void)config;
            notify_status(
                BridgeStatus::SERVER_START_FAILED,
                {},
                "Bridge Protocol v1 named-pipe transport is available only on Windows.");
#           endif
        }

        /// \brief Stops the named-pipe transport and drains active callbacks.
        void shutdown() override {
#           if defined(_WIN32)
            bool should_drain = false;
            std::shared_ptr<SimpleNamedPipe::NamedPipeServer> server;
            {
                std::unique_lock<std::mutex> lock(m_state->mutex);
                if (m_state->phase == RuntimePhase::Stopping) {
                    if (is_inside_callback()) {
                        return;
                    }
                    while (m_state->phase == RuntimePhase::Stopping) {
                        m_state->lifecycle_cv.wait(lock);
                    }
                    return;
                }
                if (m_state->phase == RuntimePhase::Stopped) {
                    return;
                }

                m_state->phase = RuntimePhase::Stopping;
                m_state->transport_callback_admission_closed = true;
                if (m_state->config) {
                    fail_dispatching_operations_locked(
                        m_state,
                        *m_state->config,
                        server_stopped_result());
                }
                if (is_inside_callback()) {
                    m_state->pending_callback_shutdown = true;
                    should_drain = m_state->active_transport_callbacks == 0;
                } else {
                    server = collect_server_locked(m_state);
                }
            }

            if (should_drain) {
                drain_pending_callback_shutdown(m_state);
                return;
            }
            if (server) {
                finalize_shutdown(m_state, std::move(server));
            }
#           else
            notify_status(BridgeStatus::SERVER_STOPPED);
#           endif
        }

    private:
        std::shared_ptr<RuntimeState> m_state;

        std::shared_ptr<BridgeProtocolNamedPipeConfig> get_config() const {
            std::lock_guard<std::mutex> lock(m_state->mutex);
            return m_state->config;
        }

        std::shared_ptr<BridgeProtocolNamedPipeConfig> get_config_or_throw() const {
            auto config = get_config();
            if (!config) {
                throw std::invalid_argument("Bridge Protocol v1 named-pipe bridge is not configured.");
            }
            return config;
        }

        bool is_inside_callback() const noexcept {
            return std::find(
                s_callback_stack.begin(),
                s_callback_stack.end(),
                m_state.get()) != s_callback_stack.end();
        }

        static std::string source_uri(const BridgeProtocolNamedPipeConfig& config) {
            return "optionx://bridge/protocol_v1/" +
                   config.installation_id +
                   "/" +
                   config.server_instance_id +
                   "/" +
                   std::to_string(config.bridge_id);
        }

        static std::string make_stream_id() {
            std::ostringstream out;
            out << "bridge-protocol-v1-pipe-"
                << std::chrono::steady_clock::now().time_since_epoch().count()
                << '-'
                << std::this_thread::get_id();
            return out.str();
        }

        static std::string make_event_id(
                const std::string& prefix,
                const std::string& stream_id,
                const std::uint64_t seq) {
            return "evt-" + stream_id + "-" + prefix + "-" + std::to_string(seq);
        }

        struct EventCoordinate {
            std::string stream_id;
            std::uint64_t seq = 0;
            std::uint64_t revision = 1;
        };

        EventCoordinate next_event_coordinate(const std::string& subject_key) {
            std::lock_guard<std::mutex> lock(m_state->mutex);
            EventCoordinate coordinate;
            coordinate.stream_id = m_state->stream_id;
            coordinate.seq = ++m_state->event_seq;
            if (!subject_key.empty()) {
                coordinate.revision = ++m_state->event_revisions[subject_key];
            }
            return coordinate;
        }

        static std::string trade_revision_key(
                const TradeRequest& request,
                const TradeResult& result) {
            const auto trade_id = result.trade_id != 0 ? result.trade_id : request.trade_id;
            if (trade_id != 0) {
                return "trade:" + std::to_string(trade_id);
            }
            if (request.signal_id != 0) {
                return "signal:" + std::to_string(request.signal_id);
            }
            if (!request.unique_hash.empty()) {
                return "unique_hash:" + request.unique_hash;
            }
            return "trade:unknown";
        }

        nlohmann::json make_balance_updated_notification(
                const BridgeProtocolNamedPipeConfig& config,
                const BaseAccountInfoData& account) {
            const auto now = metatrader_file::detail::unix_time_ms();
            const auto account_id = metatrader_file::detail::account_id_string(account);
            const auto coord = next_event_coordinate("account:" + account_id);
            return metatrader_file::detail::make_balance_updated_notification(
                make_event_id("balance", coord.stream_id, coord.seq),
                source_uri(config),
                coord.stream_id,
                coord.seq,
                now,
                now,
                account_id,
                metatrader_file::detail::safe_account_balance(account),
                metatrader_file::detail::safe_account_currency(account),
                coord.revision);
        }

        void notify_status(
                BridgeStatus status,
                std::string connection_id = {},
                std::string message = {}) const {
            notify_status_from_state(
                m_state,
                status,
                std::move(connection_id),
                std::move(message));
        }

        static void notify_status_from_state(
                const std::shared_ptr<RuntimeState>& state,
                BridgeStatus status,
                std::string connection_id,
                std::string message = {}) {
            bridge_status_callback_t callback;
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                callback = state->status_callback;
            }
            if (callback) {
                try {
                    CallbackScope scope(state);
                    callback(BridgeStatusUpdate{
                        status,
                        std::move(connection_id),
                        std::move(message)
                    });
                } catch (...) {
                }
            }
        }

        std::string current_stream_id() const {
            std::lock_guard<std::mutex> lock(m_state->mutex);
            return m_state->stream_id;
        }

        static std::string frame_message(nlohmann::json message) {
            return message.dump(-1) + "\n";
        }

        static std::size_t max_pipe_frame_bytes(
                const BridgeProtocolNamedPipeConfig& config) {
            if (config.request_body_limit == (std::numeric_limits<std::size_t>::max)()) {
                return config.request_body_limit;
            }
            return config.request_body_limit + 1;
        }

        static bool is_valid_jsonrpc_id(const nlohmann::json& id) {
            return id.is_string() ||
                   id.is_null() ||
                   id.is_number_integer() ||
                   id.is_number_unsigned();
        }

        static nlohmann::json request_id_or_null(const nlohmann::json& request) {
            if (!request.is_object() || !request.contains("id")) {
                return nullptr;
            }
            const auto& id = request.at("id");
            return is_valid_jsonrpc_id(id) ? id : nlohmann::json(nullptr);
        }

        static bool hello_accepts_v1(const nlohmann::json& params) {
            if (!params.is_object() || !params.contains("requested_protocol_versions")) {
                return true;
            }
            const auto& versions = params.at("requested_protocol_versions");
            if (!versions.is_array()) {
                return false;
            }
            for (const auto& version : versions) {
                if (version.is_string() && version.get<std::string>() == "1") {
                    return true;
                }
                if (version.is_number_integer() && version.get<std::int64_t>() == 1) {
                    return true;
                }
                if (version.is_number_unsigned() && version.get<std::uint64_t>() == 1) {
                    return true;
                }
            }
            return false;
        }

        nlohmann::json handle_message_body(
                const BridgeProtocolNamedPipeConfig& config,
                const std::string& body) {
            if (body.size() > config.request_body_limit) {
                return detail::jsonrpc_error(
                    nullptr,
                    detail::jsonrpc_invalid_request,
                    "Message is too large.",
                    nlohmann::json{{"code", "request_body_too_large"}});
            }

            try {
                return handle_jsonrpc_command(config, nlohmann::json::parse(body));
            } catch (const nlohmann::json::parse_error& ex) {
                return detail::jsonrpc_error(
                    nullptr,
                    detail::jsonrpc_parse_error,
                    ex.what());
            } catch (const std::exception& ex) {
                return detail::jsonrpc_error(
                    nullptr,
                    detail::jsonrpc_internal_error,
                    ex.what());
            }
        }

        nlohmann::json handle_jsonrpc_command(
                const BridgeProtocolNamedPipeConfig& config,
                const nlohmann::json& request) {
            const auto id = request_id_or_null(request);
            if (!request.is_object() ||
                !request.contains("jsonrpc") ||
                !request.at("jsonrpc").is_string() ||
                request.at("jsonrpc").get<std::string>() != "2.0" ||
                !request.contains("id") ||
                !is_valid_jsonrpc_id(request.at("id")) ||
                !request.contains("method") ||
                !request.at("method").is_string()) {
                return detail::jsonrpc_error(
                    id,
                    detail::jsonrpc_invalid_request,
                    "Invalid JSON-RPC request.");
            }
            if (!id.is_null() && id.dump(-1).size() > config.max_jsonrpc_id_bytes) {
                return detail::jsonrpc_error(
                    id,
                    detail::jsonrpc_invalid_request,
                    "JSON-RPC id exceeds configured size limit.",
                    nlohmann::json{{"code", "jsonrpc_id_too_large"}});
            }

            const auto method = request.at("method").get<std::string>();
            const auto params =
                request.contains("params") ? request.at("params") : nlohmann::json::object();
            if (!params.is_object()) {
                return detail::jsonrpc_error(
                    id,
                    detail::jsonrpc_invalid_params,
                    "JSON-RPC params must be an object.");
            }

            if (method == "protocol.hello") {
                if (!hello_accepts_v1(params)) {
                    return detail::jsonrpc_error(
                        id,
                        detail::jsonrpc_unsupported_protocol_version,
                        "Unsupported protocol version.",
                        nlohmann::json{{"code", "unsupported_protocol_version"}});
                }
                return detail::jsonrpc_result(
                    id,
                    nlohmann::json{
                        {"selected_protocol_version", "1"},
                        {"installation_id", config.installation_id},
                        {"server_instance_id", config.server_instance_id},
                        {"session_id", current_stream_id()}
                    });
            }
            if (method == "protocol.capabilities.get") {
                auto result = detail::capabilities_snapshot();
                result["server_instance_id"] = config.server_instance_id;
                result["features"]["http"] = false;
                result["features"]["websocket"] = false;
                result["features"]["named_pipe"] = true;
                result["limits"]["max_message_bytes"] = config.request_body_limit;
                result["limits"]["max_jsonrpc_id_bytes"] = config.max_jsonrpc_id_bytes;
                result["limits"]["max_idempotency_key_bytes"] =
                    config.max_idempotency_key_bytes;
                result["limits"]["max_operation_fingerprint_bytes"] =
                    config.max_operation_fingerprint_bytes;
                result["limits"]["max_operation_cache_bytes"] =
                    config.max_operation_cache_bytes;
                result["limits"]["operation_cache_retention_ms"] =
                    config.operation_cache_retention_ms;
                return detail::jsonrpc_result(id, std::move(result));
            }
            if (method == "account.balance.get") {
                return handle_account_balance_get(id);
            }
            if (method == "signal.submit" || method == "trade.open") {
                return handle_trade_affecting_command(
                    config,
                    id,
                    method,
                    params,
                    method == "trade.open");
            }

            return detail::jsonrpc_error(
                id,
                detail::jsonrpc_method_not_found,
                "Method not found.",
                nlohmann::json{{"method", method}});
        }

        nlohmann::json handle_account_balance_get(const nlohmann::json& id) {
            std::shared_ptr<BaseAccountInfoData> account;
            {
                std::lock_guard<std::mutex> lock(m_state->mutex);
                account = m_state->account_info;
            }
            if (!account) {
                return detail::jsonrpc_result(
                    id,
                    nlohmann::json{
                        {"status", "unavailable"},
                        {"final", true},
                        {"reason", {
                            {"code", "account_snapshot_unavailable"},
                            {"message", "No account snapshot is available."}
                        }}
                    });
            }
            return detail::jsonrpc_result(
                id,
                nlohmann::json{
                    {"status", "completed"},
                    {"final", true},
                    {"account", metatrader_file::detail::account_snapshot_json(*account)}
                });
        }

        static bool is_known_key(
                const std::unordered_set<std::string>& keys,
                const std::string& key) {
            return keys.find(key) != keys.end();
        }

        static std::pair<bool, std::string> reject_unknown_keys(
                const nlohmann::json& object,
                const std::unordered_set<std::string>& keys,
                const std::string& path) {
            if (!object.is_object()) {
                return {false, path + " must be an object."};
            }
            for (const auto& item : object.items()) {
                if (!is_known_key(keys, item.key())) {
                    return {
                        false,
                        path + " contains unsupported field: " + item.key() + "."
                    };
                }
            }
            return {true, {}};
        }

        static std::pair<bool, std::string> validate_known_trade_schema(
                const nlohmann::json& params,
                const bool direct_trade_open) {
            static const std::unordered_set<std::string> top_level_keys{
                "context",
                "routing",
                "identity",
                "sizing",
                "signal",
                "trade",
                "metadata",
                "extensions"
            };
            static const std::unordered_set<std::string> context_keys{
                "idempotency_key",
                "valid_until_ms",
                "client_created_at_ms",
                "file_seq",
                "transport"
            };
            static const std::unordered_set<std::string> identity_keys{
                "signal_name", "unique_hash", "unique_id", "user_data"
            };
            static const std::unordered_set<std::string> trade_keys{
                "symbol",
                "order_type",
                "direction",
                "action",
                "option_type",
                "amount",
                "currency",
                "expiry",
                "duration_ms",
                "duration",
                "duration_sec",
                "expires_at_ms",
                "expiry_time",
                "refund",
                "min_payout",
                "comment",
                "account_id",
                "account_type",
                "signal_name",
                "unique_hash",
                "unique_id",
                "user_data",
                "metadata",
                "extensions"
            };
            static const std::unordered_set<std::string> amount_keys{"value", "currency"};
            static const std::unordered_set<std::string> expiry_keys{
                "kind", "duration_ms", "expires_at_ms"
            };
            static const std::unordered_set<std::string> sizing_keys{
                "mode",
                "type",
                "amount",
                "currency",
                "step",
                "group_id",
                "group_hash",
                "group_name",
                "params",
                "metadata",
                "extensions"
            };

            if (!params.is_object()) {
                return {false, "Command params must be an object."};
            }
            const auto top = reject_unknown_keys(params, top_level_keys, "Command params");
            if (!top.first) {
                return top;
            }
            if (params.contains("context")) {
                const auto context =
                    reject_unknown_keys(params.at("context"), context_keys, "Command context");
                if (!context.first) {
                    return context;
                }
            }
            if (params.contains("identity")) {
                const auto identity =
                    reject_unknown_keys(params.at("identity"), identity_keys, "Command identity");
                if (!identity.first) {
                    return identity;
                }
            }
            if (params.contains("sizing")) {
                const auto sizing =
                    reject_unknown_keys(params.at("sizing"), sizing_keys, "Command sizing");
                if (!sizing.first) {
                    return sizing;
                }
            }

            const auto trade_key = direct_trade_open ? "trade" : "signal";
            if (!params.contains(trade_key) || !params.at(trade_key).is_object()) {
                return {
                    false,
                    std::string("Command ") + trade_key + " must be an object."
                };
            }
            const auto& trade = params.at(trade_key);
            const auto trade_schema =
                reject_unknown_keys(trade, trade_keys, "Command " + std::string(trade_key));
            if (!trade_schema.first) {
                return trade_schema;
            }
            if (trade.contains("amount") && trade.at("amount").is_object()) {
                const auto amount =
                    reject_unknown_keys(trade.at("amount"), amount_keys, "Command amount");
                if (!amount.first) {
                    return amount;
                }
            }
            if (trade.contains("expiry")) {
                const auto expiry =
                    reject_unknown_keys(trade.at("expiry"), expiry_keys, "Command expiry");
                if (!expiry.first) {
                    return expiry;
                }
            }
            return {true, {}};
        }

        nlohmann::json handle_trade_affecting_command(
                const BridgeProtocolNamedPipeConfig& config,
                const nlohmann::json& id,
                const std::string& method,
                const nlohmann::json& params,
                const bool direct_trade_open) {
            const auto schema_validation =
                validate_known_trade_schema(params, direct_trade_open);
            if (!schema_validation.first) {
                return detail::jsonrpc_error(
                    id,
                    detail::jsonrpc_invalid_params,
                    schema_validation.second);
            }

            const auto routing_validation =
                validate_supported_routing(params, direct_trade_open);
            if (!routing_validation.first) {
                return detail::jsonrpc_error(
                    id,
                    detail::jsonrpc_invalid_params,
                    routing_validation.second);
            }

            const auto idempotency_key =
                metatrader_file::detail::context_idempotency_key(params);
            if (idempotency_key.empty()) {
                return detail::jsonrpc_error(
                    id,
                    detail::jsonrpc_invalid_params,
                    "Trade-affecting commands require context.idempotency_key.");
            }
            if (idempotency_key.size() > config.max_idempotency_key_bytes) {
                return detail::jsonrpc_error(
                    id,
                    detail::jsonrpc_invalid_params,
                    "context.idempotency_key exceeds configured size limit.",
                    nlohmann::json{{"code", "idempotency_key_too_large"}});
            }

            const auto operation_key = method + "\n" + idempotency_key;
            const auto fingerprint =
                detail::canonical_trade_command_payload(
                    params,
                    direct_trade_open).dump(-1);
            if (fingerprint.size() > config.max_operation_fingerprint_bytes) {
                return detail::jsonrpc_error(
                    id,
                    detail::jsonrpc_invalid_params,
                    "Canonical trade command fingerprint exceeds configured size limit.",
                    nlohmann::json{{"code", "operation_fingerprint_too_large"}});
            }

            {
                std::unique_lock<std::mutex> lock(m_state->mutex);
                nlohmann::json cached_response;
                if (try_cached_operation_response_locked(
                        config,
                        id,
                        operation_key,
                        fingerprint,
                        idempotency_key,
                        cached_response)) {
                    return cached_response;
                }
            }

            std::unique_ptr<TradeSignal> signal;
            try {
                signal = metatrader_file::detail::parse_signal_params(params, direct_trade_open);
            } catch (const std::exception& ex) {
                return detail::jsonrpc_error(id, detail::jsonrpc_invalid_params, ex.what());
            }

            const auto& context = metatrader_file::detail::object_member_or_empty(params, "context");
            if (!context.contains("valid_until_ms")) {
                return detail::jsonrpc_error(
                    id,
                    detail::jsonrpc_invalid_params,
                    "Trade-affecting commands require context.valid_until_ms.");
            }
            std::int64_t valid_until_ms = 0;
            try {
                valid_until_ms = metatrader_file::detail::context_valid_until_ms(params);
            } catch (const std::exception& ex) {
                return detail::jsonrpc_error(id, detail::jsonrpc_invalid_params, ex.what());
            }
            if (valid_until_ms <= 0 ||
                metatrader_file::detail::unix_time_ms() > valid_until_ms) {
                const auto result = nlohmann::json{
                    {"status", "rejected"},
                    {"final", true},
                    {"reason", {
                        {"code", "stale_request"},
                        {"message", "Command valid_until_ms is in the past."}
                    }}
                };
                if (!remember_operation(config, operation_key, fingerprint, result)) {
                    return detail::jsonrpc_result(id, idempotency_cache_full_result());
                }
                return detail::jsonrpc_result(id, result);
            }

            signal_id_allocator_t allocator;
            trade_signal_callback_t callback;
            {
                std::lock_guard<std::mutex> lock(m_state->mutex);
                allocator = m_state->signal_id_allocator;
                callback = m_state->trade_signal_callback;
            }
            if (!allocator || !callback) {
                return detail::jsonrpc_error(
                    id,
                    detail::jsonrpc_internal_error,
                    "Trade signal callback and signal ID allocator must be configured.");
            }

            const auto signal_id = allocator();
            if (signal_id == 0) {
                return detail::jsonrpc_error(
                    id,
                    detail::jsonrpc_internal_error,
                    "Signal ID allocator returned zero.");
            }

            signal->bridge_id = config.bridge_id;
            signal->signal_id = signal_id;
            auto result = nlohmann::json{
                {"status", "accepted"},
                {"final", false},
                {"operation_id", "mem:" + std::to_string(config.bridge_id) + ":" + idempotency_key}
            };
            if (direct_trade_open) {
                result["trade_refs"] = nlohmann::json::array({
                    nlohmann::json{
                        {"status", "pending"},
                        {"signal_id", std::to_string(signal_id)}
                    }
                });
            } else {
                result["signal_ref"] = nlohmann::json{
                    {"signal_id", std::to_string(signal_id)},
                    {"unique_hash", signal->unique_hash},
                    {"signal_name", signal->signal_name}
                };
                result["trade_refs"] = nlohmann::json::array();
            }

            {
                std::unique_lock<std::mutex> lock(m_state->mutex);
                nlohmann::json cached_response;
                if (try_cached_operation_response_locked(
                        config,
                        id,
                        operation_key,
                        fingerprint,
                        idempotency_key,
                        cached_response)) {
                    return cached_response;
                }

                const auto byte_size = operation_byte_size(operation_key, fingerprint, result);
                prune_completed_operations_locked(m_state, config, byte_size, 1);
                if (m_state->operations.size() >= config.dedupe_cache_size) {
                    return detail::jsonrpc_result(id, idempotency_cache_full_result());
                }
                if (byte_size > config.max_operation_cache_bytes ||
                    m_state->operation_cache_bytes >
                        config.max_operation_cache_bytes - byte_size) {
                    return detail::jsonrpc_result(id, idempotency_cache_full_result());
                }

                m_state->operation_order.push_back(operation_key);
                StoredOperation operation;
                operation.fingerprint = fingerprint;
                operation.result = result;
                operation.byte_size = byte_size;
                operation.completed_at_ms = 0;
                operation.dispatching = true;
                m_state->operations.emplace(operation_key, std::move(operation));
                m_state->operation_cache_bytes += byte_size;
            }

            if (metatrader_file::detail::unix_time_ms() > valid_until_ms) {
                const auto stale = nlohmann::json{
                    {"status", "rejected"},
                    {"final", true},
                    {"reason", {
                        {"code", "stale_request"},
                        {"message", "Command valid_until_ms expired before dispatch."}
                    }}
                };
                complete_reserved_operation(config, operation_key, stale);
                return detail::jsonrpc_result(id, stale);
            }

            try {
                callback(std::move(signal));
            } catch (const std::exception& ex) {
                const auto failed = dispatch_failed_result(ex.what());
                complete_reserved_operation(config, operation_key, failed);
                return detail::jsonrpc_result(id, failed);
            } catch (...) {
                const auto failed = dispatch_failed_result("Trade signal callback failed.");
                complete_reserved_operation(config, operation_key, failed);
                return detail::jsonrpc_result(id, failed);
            }

            complete_reserved_operation(config, operation_key, result);
            return detail::jsonrpc_result(id, result);
        }

        static std::pair<bool, std::string> validate_supported_routing(
                const nlohmann::json& params,
                const bool direct_trade_open) {
            if (!params.is_object() || !params.contains("routing")) {
                return {true, {}};
            }
            const auto& routing = params.at("routing");
            if (!routing.is_object()) {
                return {false, "Command routing must be an object."};
            }
            if (routing.empty()) {
                return {true, {}};
            }
            if (routing.contains("policy")) {
                return {
                    false,
                    direct_trade_open
                        ? "trade.open routing.policy is not supported."
                        : "signal.submit routing.policy is not supported by this bridge yet."
                };
            }

            for (const auto& item : routing.items()) {
                if (item.key() != "selector" && item.key() != "platform_type") {
                    return {
                        false,
                        "Command routing contains unsupported field: " + item.key() + "."
                    };
                }
            }

            if (!routing.contains("selector")) {
                return {true, {}};
            }
            const auto& selector = routing.at("selector");
            if (!selector.is_object()) {
                return {false, "Command routing.selector must be an object."};
            }
            if (selector.empty()) {
                return {true, {}};
            }

            for (const auto& item : selector.items()) {
                if (item.key() != "kind" && item.key() != "account_id") {
                    return {
                        false,
                        "Command routing.selector contains unsupported field: " + item.key() + "."
                    };
                }
            }

            auto kind = metatrader_file::detail::string_value(selector, "kind", "default");
            kind = metatrader_file::detail::lower_ascii_copy(
                metatrader_file::detail::trim_ascii_copy(kind));
            if (kind.empty()) {
                kind = "default";
            }
            if (kind == "accounts" || kind == "all") {
                return {
                    false,
                    direct_trade_open
                        ? "trade.open supports only default or account routing."
                        : "signal.submit accounts/all routing is not supported by this bridge yet."
                };
            }
            if (kind != "default" && kind != "account") {
                return {false, "Command routing.selector.kind is unsupported."};
            }
            if (kind == "default" && selector.contains("account_id")) {
                return {false, "Command default routing must not include account_id."};
            }
            if (kind == "account") {
                if (!selector.contains("account_id")) {
                    return {false, "Command account routing requires account_id."};
                }
                try {
                    static_cast<void>(
                        metatrader_file::detail::int64_value(selector, "account_id", 0));
                } catch (...) {
                    return {
                        false,
                        "Command account routing requires a numeric account_id in this bridge."
                    };
                }
            }
            return {true, {}};
        }

        static nlohmann::json idempotency_conflict_result(std::string message) {
            return nlohmann::json{
                {"status", "rejected"},
                {"final", true},
                {"reason", {
                    {"code", "idempotency_conflict"},
                    {"message", std::move(message)}
                }}
            };
        }

        static nlohmann::json idempotency_cache_full_result() {
            return nlohmann::json{
                {"status", "rejected"},
                {"final", true},
                {"reason", {
                    {"code", "idempotency_cache_full"},
                    {"message", "Bridge Protocol v1 idempotency cache is full."}
                }}
            };
        }

        static nlohmann::json operation_in_progress_result(
                const BridgeProtocolNamedPipeConfig& config,
                const std::string& idempotency_key) {
            return nlohmann::json{
                {"status", "processing"},
                {"final", false},
                {"operation_id", "mem:" + std::to_string(config.bridge_id) + ":" + idempotency_key},
                {"reason", {
                    {"code", "operation_in_progress"},
                    {"message", "The idempotent operation is still being dispatched."}
                }}
            };
        }

        static nlohmann::json dispatch_failed_result(std::string message) {
            if (message.size() > 1024) {
                message.resize(1024);
            }
            return nlohmann::json{
                {"status", "rejected"},
                {"final", true},
                {"reason", {
                    {"code", "dispatch_failed"},
                    {"message", std::move(message)}
                }}
            };
        }

        static nlohmann::json server_stopped_result() {
            return nlohmann::json{
                {"status", "rejected"},
                {"final", true},
                {"reason", {
                    {"code", "server_stopped"},
                    {"message", "Bridge Protocol v1 named-pipe bridge stopped before dispatch completed."}
                }}
            };
        }

        static nlohmann::json server_stopping_response() {
            return detail::jsonrpc_error(
                nullptr,
                detail::jsonrpc_internal_error,
                "Bridge Protocol v1 named-pipe bridge is stopping.",
                nlohmann::json{{"code", "server_stopping"}});
        }

        static std::size_t operation_byte_size(
                const std::string& operation_key,
                const std::string& fingerprint,
                const nlohmann::json& result) {
            return operation_key.size() + fingerprint.size() + result.dump(-1).size();
        }

        static void touch_operation_locked(
                const std::shared_ptr<RuntimeState>& state,
                const std::string& operation_key) {
            const auto it = std::find(
                state->operation_order.begin(),
                state->operation_order.end(),
                operation_key);
            if (it == state->operation_order.end()) {
                state->operation_order.push_back(operation_key);
                return;
            }
            auto key = std::move(*it);
            state->operation_order.erase(it);
            state->operation_order.push_back(std::move(key));
        }

        bool try_cached_operation_response_locked(
                const BridgeProtocolNamedPipeConfig& config,
                const nlohmann::json& id,
                const std::string& operation_key,
                const std::string& fingerprint,
                const std::string& idempotency_key,
                nlohmann::json& response) {
            prune_completed_operations_locked(m_state, config);
            const auto existing = m_state->operations.find(operation_key);
            if (existing == m_state->operations.end()) {
                return false;
            }
            if (existing->second.fingerprint != fingerprint) {
                response = detail::jsonrpc_result(id, idempotency_conflict_result(
                    "The same idempotency_key was used with a different payload."));
                return true;
            }
            if (existing->second.dispatching) {
                response = detail::jsonrpc_result(
                    id,
                    operation_in_progress_result(config, idempotency_key));
                return true;
            }
            touch_operation_locked(m_state, operation_key);
            response = detail::jsonrpc_result(id, existing->second.result);
            return true;
        }

        bool remember_operation(
                const BridgeProtocolNamedPipeConfig& config,
                const std::string& operation_key,
                const std::string& fingerprint,
                const nlohmann::json& result) {
            std::lock_guard<std::mutex> lock(m_state->mutex);
            const auto byte_size = operation_byte_size(operation_key, fingerprint, result);
            auto existing = m_state->operations.find(operation_key);
            prune_completed_operations_locked(
                m_state,
                config,
                byte_size,
                existing == m_state->operations.end() ? 1 : 0);
            existing = m_state->operations.find(operation_key);
            const auto old_size = existing == m_state->operations.end()
                ? std::size_t{0}
                : existing->second.byte_size;
            if (byte_size > config.max_operation_cache_bytes ||
                m_state->operation_cache_bytes - old_size >
                    config.max_operation_cache_bytes - byte_size) {
                return false;
            }
            if (existing == m_state->operations.end()) {
                if (m_state->operations.size() >= config.dedupe_cache_size) {
                    return false;
                }
                m_state->operation_order.push_back(operation_key);
            }
            auto& operation = m_state->operations[operation_key];
            m_state->operation_cache_bytes =
                m_state->operation_cache_bytes - operation.byte_size + byte_size;
            operation.fingerprint = fingerprint;
            operation.result = result;
            operation.byte_size = byte_size;
            operation.completed_at_ms = metatrader_file::detail::unix_time_ms();
            operation.dispatching = false;
            return true;
        }

        void complete_reserved_operation(
                const BridgeProtocolNamedPipeConfig& config,
                const std::string& operation_key,
                const nlohmann::json& result) {
            std::lock_guard<std::mutex> lock(m_state->mutex);
            const auto existing = m_state->operations.find(operation_key);
            if (existing == m_state->operations.end()) {
                return;
            }
            const auto byte_size =
                operation_byte_size(operation_key, existing->second.fingerprint, result);
            m_state->operation_cache_bytes =
                m_state->operation_cache_bytes - existing->second.byte_size + byte_size;
            existing->second.result = result;
            existing->second.byte_size = byte_size;
            existing->second.completed_at_ms = metatrader_file::detail::unix_time_ms();
            existing->second.dispatching = false;
            touch_operation_locked(m_state, operation_key);
            if (byte_size > config.max_operation_cache_bytes) {
                m_state->operation_cache_bytes -= existing->second.byte_size;
                m_state->operations.erase(existing);
                const auto order_it = std::find(
                    m_state->operation_order.begin(),
                    m_state->operation_order.end(),
                    operation_key);
                if (order_it != m_state->operation_order.end()) {
                    m_state->operation_order.erase(order_it);
                }
                return;
            }
            prune_completed_operations_locked(m_state, config);
        }

        static void fail_dispatching_operations_locked(
                const std::shared_ptr<RuntimeState>& state,
                const BridgeProtocolNamedPipeConfig& config,
                const nlohmann::json& result) {
            for (auto& item : state->operations) {
                auto& operation = item.second;
                if (!operation.dispatching) {
                    continue;
                }
                const auto byte_size =
                    operation_byte_size(item.first, operation.fingerprint, result);
                state->operation_cache_bytes =
                    state->operation_cache_bytes - operation.byte_size + byte_size;
                operation.result = result;
                operation.byte_size = byte_size;
                operation.completed_at_ms = metatrader_file::detail::unix_time_ms();
                operation.dispatching = false;
            }
            prune_completed_operations_locked(state, config);
        }

        static bool evict_oldest_completed_operation_locked(
                const std::shared_ptr<RuntimeState>& state) {
            for (auto it = state->operation_order.begin();
                 it != state->operation_order.end();) {
                const auto& key = *it;
                auto existing = state->operations.find(key);
                if (existing == state->operations.end()) {
                    it = state->operation_order.erase(it);
                    continue;
                }
                if (existing->second.dispatching) {
                    ++it;
                    continue;
                }
                state->operation_cache_bytes -= existing->second.byte_size;
                state->operations.erase(existing);
                state->operation_order.erase(it);
                return true;
            }
            return false;
        }

        static void prune_completed_operations_locked(
                const std::shared_ptr<RuntimeState>& state,
                const BridgeProtocolNamedPipeConfig& config,
                const std::size_t required_bytes = 0,
                const std::size_t required_records = 0) {
            const auto now = metatrader_file::detail::unix_time_ms();
            for (auto it = state->operation_order.begin(); it != state->operation_order.end();) {
                auto existing = state->operations.find(*it);
                if (existing == state->operations.end()) {
                    it = state->operation_order.erase(it);
                    continue;
                }
                const auto& operation = existing->second;
                if (!operation.dispatching &&
                    operation.completed_at_ms > 0 &&
                    now - operation.completed_at_ms >= config.operation_cache_retention_ms) {
                    state->operation_cache_bytes -= operation.byte_size;
                    state->operations.erase(existing);
                    it = state->operation_order.erase(it);
                    continue;
                }
                ++it;
            }

            while (state->operations.size() + required_records >
                   config.dedupe_cache_size) {
                if (!evict_oldest_completed_operation_locked(state)) {
                    break;
                }
            }
            while (required_bytes <= config.max_operation_cache_bytes &&
                   state->operation_cache_bytes >
                       config.max_operation_cache_bytes - required_bytes) {
                if (!evict_oldest_completed_operation_locked(state)) {
                    break;
                }
            }
        }

#       if defined(_WIN32)
        void configure_server_callbacks(
                const std::shared_ptr<SimpleNamedPipe::NamedPipeServer>& server) {
            auto* const server_identity = server.get();
            server->on_connected = [state = m_state](int client_id) {
                TransportCallbackScope scope(state);
                if (!scope.admitted()) {
                    std::shared_ptr<SimpleNamedPipe::NamedPipeServer> current_server;
                    {
                        std::lock_guard<std::mutex> lock(state->mutex);
                        current_server = state->server;
                    }
                    if (current_server) {
                        current_server->close(client_id);
                    }
                    return;
                }

                {
                    std::lock_guard<std::mutex> lock(state->mutex);
                    state->client_ids.insert(client_id);
                }

                notify_status_from_state(
                    state,
                    BridgeStatus::CLIENT_CONNECTED,
                    connection_id(client_id));
            };

            server->on_start = [state = m_state, server_identity](
                    const SimpleNamedPipe::ServerConfig&) {
                {
                    std::lock_guard<std::mutex> lock(state->mutex);
                    if (state->server.get() != server_identity ||
                        state->phase != RuntimePhase::Starting) {
                        return;
                    }
                    state->phase = RuntimePhase::Running;
                    state->lifecycle_cv.notify_all();
                }
                notify_status_from_state(state, BridgeStatus::SERVER_STARTED, "pipe");
            };

            server->on_disconnected =
                [state = m_state](int client_id, const std::error_code& ec) {
                    CallbackScope scope(state);
                    {
                        std::lock_guard<std::mutex> lock(state->mutex);
                        state->client_ids.erase(client_id);
                    }
                    notify_status_from_state(
                        state,
                        BridgeStatus::CLIENT_DISCONNECTED,
                        connection_id(client_id),
                        ec.message());
                };

            server->on_message =
                [this](int client_id, const std::string& message) {
                    handle_message(client_id, message);
                };

            server->on_error = [state = m_state, server_identity](const std::error_code& ec) {
                bool start_failed = false;
                std::shared_ptr<SimpleNamedPipe::NamedPipeServer> server_to_finalize;
                {
                    std::lock_guard<std::mutex> lock(state->mutex);
                    if (state->server.get() != server_identity) {
                        return;
                    }
                    if (state->phase == RuntimePhase::Starting) {
                        start_failed = true;
                        state->phase = RuntimePhase::Stopping;
                        state->transport_callback_admission_closed = true;
                        server_to_finalize = state->server;
                        state->server.reset();
                        state->client_ids.clear();
                        state->pending_callback_shutdown = false;
                    }
                }
                notify_status_from_state(
                    state,
                    start_failed ? BridgeStatus::SERVER_START_FAILED
                                 : BridgeStatus::CONNECTION_ERROR,
                    {},
                    ec.message());
                if (server_to_finalize) {
                    std::thread([state, server = std::move(server_to_finalize)]() mutable {
                        finalize_shutdown(state, std::move(server));
                    }).detach();
                }
            };

            server->on_stop = [state = m_state, server_identity](
                    const SimpleNamedPipe::ServerConfig&) {
                std::shared_ptr<SimpleNamedPipe::NamedPipeServer> server_to_finalize;
                {
                    std::lock_guard<std::mutex> lock(state->mutex);
                    if (state->server.get() != server_identity) {
                        return;
                    }
                    state->phase = RuntimePhase::Stopping;
                    state->transport_callback_admission_closed = true;
                    server_to_finalize = state->server;
                    state->server.reset();
                    state->client_ids.clear();
                    state->pending_callback_shutdown = false;
                }
                if (server_to_finalize) {
                    std::thread([state, server = std::move(server_to_finalize)]() mutable {
                        finalize_shutdown(state, std::move(server));
                    }).detach();
                }
            };
        }

        static std::string connection_id(int client_id) {
            return std::to_string(client_id);
        }

        void handle_message(int client_id, const std::string& message) {
            TransportCallbackScope scope(m_state);
            std::shared_ptr<SimpleNamedPipe::NamedPipeServer> server;
            {
                std::lock_guard<std::mutex> lock(m_state->mutex);
                server = m_state->server;
            }
            if (!server) {
                return;
            }
            if (!scope.admitted()) {
                auto config = get_config();
                if (config) {
                    send_pipe_text(
                        m_state,
                        *config,
                        server,
                        client_id,
                        frame_message(server_stopping_response()));
                }
                return;
            }

            auto config = get_config_or_throw();
            const auto response = frame_message(handle_message_body(*config, message));
            send_pipe_text(m_state, *config, server, client_id, response);
        }

        void broadcast_notification(nlohmann::json notification) {
            std::shared_ptr<SimpleNamedPipe::NamedPipeServer> server;
            std::vector<int> clients;
            std::shared_ptr<BridgeProtocolNamedPipeConfig> config;
            {
                std::lock_guard<std::mutex> lock(m_state->mutex);
                server = m_state->server;
                config = m_state->config;
                clients.assign(m_state->client_ids.begin(), m_state->client_ids.end());
            }
            if (!server || !config) {
                return;
            }
            const auto text = frame_message(std::move(notification));
            for (const int client_id : clients) {
                send_pipe_text(m_state, *config, server, client_id, text);
            }
        }

        static void send_pipe_text(
                const std::shared_ptr<RuntimeState>& state,
                const BridgeProtocolNamedPipeConfig& config,
                const std::shared_ptr<SimpleNamedPipe::NamedPipeServer>& server,
                const int client_id,
                const std::string& text) {
            if (text.size() > max_pipe_frame_bytes(config)) {
                notify_status_from_state(
                    state,
                    BridgeStatus::CONNECTION_ERROR,
                    connection_id(client_id),
                    "Named-pipe message exceeds configured protocol frame limit.");
                return;
            }

            server->send_to(
                client_id,
                text,
                [state, client_id](const std::error_code& ec) {
                    if (!ec) {
                        return;
                    }
                    notify_status_from_state(
                        state,
                        BridgeStatus::CONNECTION_ERROR,
                        connection_id(client_id),
                        ec.message());
                });
        }

        void clear_runtime_after_start_failure(
                const SimpleNamedPipe::NamedPipeServer* server_identity) {
            std::lock_guard<std::mutex> lock(m_state->mutex);
            if (m_state->server.get() != server_identity ||
                m_state->phase != RuntimePhase::Starting) {
                return;
            }
            m_state->server.reset();
            m_state->client_ids.clear();
            m_state->phase = RuntimePhase::Stopped;
            m_state->transport_callback_admission_closed = false;
            m_state->pending_callback_shutdown = false;
            m_state->lifecycle_cv.notify_all();
        }

        static std::shared_ptr<SimpleNamedPipe::NamedPipeServer> collect_server_locked(
                const std::shared_ptr<RuntimeState>& state) {
            auto server = state->server;
            state->server.reset();
            state->client_ids.clear();
            return server;
        }

        static void drain_pending_callback_shutdown(
                const std::shared_ptr<RuntimeState>& state) {
            std::shared_ptr<SimpleNamedPipe::NamedPipeServer> server;
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                if (!state->pending_callback_shutdown ||
                    state->active_transport_callbacks != 0 ||
                    state->phase != RuntimePhase::Stopping) {
                    return;
                }
                state->pending_callback_shutdown = false;
                server = collect_server_locked(state);
            }
            std::thread([state, server = std::move(server)]() mutable {
                finalize_shutdown(state, std::move(server));
            }).detach();
        }

        static void finalize_shutdown(
                const std::shared_ptr<RuntimeState>& state,
                std::shared_ptr<SimpleNamedPipe::NamedPipeServer> server) {
            if (server) {
                server->stop();
            }
            bool should_notify = false;
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                should_notify = !state->stop_notified;
                state->stop_notified = true;
                state->phase = RuntimePhase::Stopped;
                state->transport_callback_admission_closed = false;
                state->pending_callback_shutdown = false;
                state->lifecycle_cv.notify_all();
            }
            if (should_notify) {
                notify_status_from_state(state, BridgeStatus::SERVER_STOPPED, {});
            }
        }
#       else
        void broadcast_notification(nlohmann::json notification) {
            (void)notification;
        }

        static void drain_pending_callback_shutdown(
                const std::shared_ptr<RuntimeState>& state) {
            (void)state;
        }
#       endif
    };

} // namespace optionx::bridges::protocol_v1

#endif // OPTIONX_HEADER_BRIDGES_PROTOCOL_V1_BRIDGE_PROTOCOL_NAMED_PIPE_BRIDGE_HPP_INCLUDED
