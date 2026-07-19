#pragma once
#ifndef OPTIONX_HEADER_BRIDGES_PROTOCOL_V1_BRIDGE_PROTOCOL_SERVER_BRIDGE_HPP_INCLUDED
#define OPTIONX_HEADER_BRIDGES_PROTOCOL_V1_BRIDGE_PROTOCOL_SERVER_BRIDGE_HPP_INCLUDED

/// \file BridgeProtocolServerBridge.hpp
/// \brief Defines the Bridge Protocol v1 HTTP/WebSocket server bridge.

namespace optionx::bridges::protocol_v1 {

    /// \class BridgeProtocolServerBridge
    /// \brief Serves JSON-RPC Bridge Protocol v1 commands over HTTP and WebSocket.
    class BridgeProtocolServerBridge final : public BaseBridge {
    private:
        class HttpServer final : public SimpleWeb::Server<SimpleWeb::HTTP> {
        public:
            unsigned short bound_port() const noexcept {
                return m_bound_port.load();
            }

        protected:
            void after_bind() override {
                m_bound_port.store(acceptor->local_endpoint().port());
            }

        private:
            std::atomic<unsigned short> m_bound_port{0};
        };

        class WsServer final : public SimpleWeb::SocketServer<SimpleWeb::WS> {
        public:
            unsigned short bound_port() const noexcept {
                return m_bound_port.load();
            }

        protected:
            void after_bind() override {
                m_bound_port.store(acceptor->local_endpoint().port());
            }

        private:
            std::atomic<unsigned short> m_bound_port{0};
        };

        struct StoredOperation {
            std::string fingerprint;
            nlohmann::json result;
            std::string request_storage_key;
        };

        struct RuntimeState {
            std::mutex mutex;
            std::shared_ptr<BridgeProtocolServerConfig> config;
            bridge_status_callback_t status_callback;
            BaseBridge::trade_signal_callback_t trade_signal_callback;
            BaseBridge::signal_report_callback_t signal_report_callback;
            BaseBridge::signal_id_allocator_t signal_id_allocator;
            std::shared_ptr<BaseAccountInfoData> account_info;
            std::shared_ptr<HttpServer> http_server;
            std::shared_ptr<WsServer> ws_server;
            std::thread http_thread;
            std::thread ws_thread;
            std::set<std::shared_ptr<WsServer::Connection>> ws_connections;
            std::unordered_map<std::string, StoredOperation> operations;
            std::unordered_map<std::string, std::string> request_id_index;
            std::deque<std::string> operation_order;
            std::uint64_t event_seq = 0;
            bool running = false;
        };

    public:
        /// \brief Constructs an unconfigured protocol server.
        BridgeProtocolServerBridge()
            : m_state(std::make_shared<RuntimeState>()) {}

        /// \brief Stops HTTP/WebSocket servers before destruction.
        ~BridgeProtocolServerBridge() override {
            shutdown();
        }

        /// \brief Configures the bridge with Bridge Protocol v1 server settings.
        bool configure(std::unique_ptr<IBridgeConfig> config) override {
            if (!config) return false;

            const auto* typed =
                dynamic_cast<const BridgeProtocolServerConfig*>(config.get());
            if (!typed) {
                config->dispatch_callbacks(false, "Invalid Bridge Protocol v1 server config type.");
                return false;
            }

            auto next_config = std::make_shared<BridgeProtocolServerConfig>(*typed);
            const auto validation = next_config->validate();
            config->dispatch_callbacks(validation.first, validation.second);
            if (!validation.first) {
                return false;
            }

            std::lock_guard<std::mutex> lock(m_state->mutex);
            if (m_state->running) {
                config->dispatch_callbacks(
                    false,
                    "Bridge Protocol v1 server cannot be reconfigured while running.");
                return false;
            }
            m_state->config = std::move(next_config);
            m_stream_id =
                "bridge-protocol-v1-" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
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

        /// \brief Returns the bound HTTP port, or zero before bind.
        unsigned short bound_http_port() const noexcept {
            std::lock_guard<std::mutex> lock(m_state->mutex);
            return m_state->http_server ? m_state->http_server->bound_port() : 0;
        }

        /// \brief Returns the bound WebSocket port, or zero before bind.
        unsigned short bound_websocket_port() const noexcept {
            std::lock_guard<std::mutex> lock(m_state->mutex);
            return m_state->ws_server ? m_state->ws_server->bound_port() : 0;
        }

        /// \brief Updates the account snapshot used by `account.balance.get`.
        void update_account_info(const AccountInfoUpdate& info) override {
            if (!info.account_info) return;

            std::shared_ptr<BridgeProtocolServerConfig> config;
            {
                std::lock_guard<std::mutex> lock(m_state->mutex);
                m_state->account_info = info.account_info;
                config = m_state->config;
            }
            if (!config) {
                return;
            }

            broadcast_ws_notification(make_balance_updated_notification(*config, *info.account_info));
        }

        /// \brief Broadcasts `trade.updated` notifications to connected WebSocket clients.
        void update_trade_result(
                const TradeRequest& request,
                const TradeResult& result) override {
            auto config = get_config();
            if (!config) {
                return;
            }
            const auto now = metatrader_file::detail::unix_time_ms();
            const auto seq = next_event_seq();
            auto notification = metatrader_file::detail::make_trade_updated_notification(
                make_event_id("trade", seq),
                source_uri(*config),
                m_stream_id,
                seq,
                result.close_date > 0 ? result.close_date : now,
                now,
                request,
                result);
            broadcast_ws_notification(std::move(notification));
        }

        /// \brief Starts configured HTTP and WebSocket transports.
        void run() override {
            auto config = get_config_or_throw();

            {
                std::lock_guard<std::mutex> lock(m_state->mutex);
                if (m_state->running) {
                    return;
                }
                m_state->running = true;
                m_state->operations.clear();
                m_state->request_id_index.clear();
                m_state->operation_order.clear();
                m_state->event_seq = 0;
            }

            if (config->enable_http) {
                start_http_server(config);
            }
            if (config->enable_websocket) {
                start_ws_server(config);
            }
        }

        /// \brief Stops configured HTTP and WebSocket transports.
        void shutdown() override {
            std::shared_ptr<HttpServer> http_server;
            std::shared_ptr<WsServer> ws_server;
            std::thread http_thread;
            std::thread ws_thread;
            bool was_running = false;

            {
                std::lock_guard<std::mutex> lock(m_state->mutex);
                was_running =
                    m_state->running ||
                    static_cast<bool>(m_state->http_server) ||
                    static_cast<bool>(m_state->ws_server) ||
                    m_state->http_thread.joinable() ||
                    m_state->ws_thread.joinable();
                http_server = m_state->http_server;
                ws_server = m_state->ws_server;
                m_state->http_server.reset();
                m_state->ws_server.reset();
                m_state->ws_connections.clear();
                m_state->running = false;
                if (m_state->http_thread.joinable()) {
                    http_thread = std::move(m_state->http_thread);
                }
                if (m_state->ws_thread.joinable()) {
                    ws_thread = std::move(m_state->ws_thread);
                }
            }

            if (http_server) {
                http_server->stop();
            }
            if (ws_server) {
                ws_server->stop();
            }
            if (http_thread.joinable() &&
                http_thread.get_id() != std::this_thread::get_id()) {
                http_thread.join();
            }
            if (ws_thread.joinable() &&
                ws_thread.get_id() != std::this_thread::get_id()) {
                ws_thread.join();
            }

            if (was_running) {
                notify_status(BridgeStatus::SERVER_STOPPED, {});
            }
        }

    private:
        std::shared_ptr<RuntimeState> m_state;
        std::string m_stream_id;

        std::shared_ptr<BridgeProtocolServerConfig> get_config() const {
            std::lock_guard<std::mutex> lock(m_state->mutex);
            return m_state->config;
        }

        std::shared_ptr<BridgeProtocolServerConfig> get_config_or_throw() const {
            auto config = get_config();
            if (!config) {
                throw std::invalid_argument("Bridge Protocol v1 server is not configured.");
            }
            return config;
        }

        static std::string regex_path(const std::string& path) {
            std::string pattern = "^";
            for (const char ch : path) {
                switch (ch) {
                case '.':
                case '+':
                case '*':
                case '?':
                case '^':
                case '$':
                case '(':
                case ')':
                case '[':
                case ']':
                case '{':
                case '}':
                case '|':
                case '\\':
                    pattern.push_back('\\');
                    break;
                default:
                    break;
                }
                pattern.push_back(ch);
            }
            pattern.push_back('$');
            return pattern;
        }

        static std::string source_uri(const BridgeProtocolServerConfig& config) {
            return "optionx://bridge/protocol_v1/" + std::to_string(config.bridge_id);
        }

        static std::string make_event_id(
                const std::string& prefix,
                const std::uint64_t seq) {
            return "evt-" + prefix + "-" + std::to_string(seq);
        }

        std::uint64_t next_event_seq() {
            std::lock_guard<std::mutex> lock(m_state->mutex);
            return ++m_state->event_seq;
        }

        nlohmann::json make_balance_updated_notification(
                const BridgeProtocolServerConfig& config,
                const BaseAccountInfoData& account) {
            const auto now = metatrader_file::detail::unix_time_ms();
            const auto seq = next_event_seq();
            return metatrader_file::detail::make_balance_updated_notification(
                make_event_id("balance", seq),
                source_uri(config),
                m_stream_id,
                seq,
                now,
                now,
                metatrader_file::detail::account_id_string(account),
                metatrader_file::detail::safe_account_balance(account),
                metatrader_file::detail::safe_account_currency(account));
        }

        void notify_status(
                BridgeStatus status,
                std::string connection_id,
                std::string message = {}) const {
            bridge_status_callback_t callback;
            {
                std::lock_guard<std::mutex> lock(m_state->mutex);
                callback = m_state->status_callback;
            }
            if (callback) {
                callback(BridgeStatusUpdate{
                    status,
                    std::move(connection_id),
                    std::move(message)
                });
            }
        }

        void notify_signal_report(BridgeSignalReport report) const {
            signal_report_callback_t callback;
            {
                std::lock_guard<std::mutex> lock(m_state->mutex);
                callback = m_state->signal_report_callback;
            }
            if (callback) {
                callback(report);
            }
        }

        void start_http_server(const std::shared_ptr<BridgeProtocolServerConfig>& config) {
            auto server = std::make_shared<HttpServer>();
            server->config.address = config->address;
            server->config.port = config->http_port;
            server->config.thread_pool_size = 1;
            configure_http_routes(server, config);

            std::lock_guard<std::mutex> lock(m_state->mutex);
            m_state->http_server = server;
            m_state->http_thread = std::thread([this, server]() {
                try {
                    server->start([this](unsigned short port) {
                        notify_status(BridgeStatus::SERVER_STARTED, "http:" + std::to_string(port));
                    });
                } catch (const std::exception& ex) {
                    notify_status(BridgeStatus::SERVER_START_FAILED, "http", ex.what());
                }
            });
        }

        void start_ws_server(const std::shared_ptr<BridgeProtocolServerConfig>& config) {
            auto server = std::make_shared<WsServer>();
            server->config.address = config->address;
            server->config.port = config->websocket_port;
            server->config.thread_pool_size = 1;
            configure_ws_routes(server, config);

            std::lock_guard<std::mutex> lock(m_state->mutex);
            m_state->ws_server = server;
            m_state->ws_thread = std::thread([this, server]() {
                try {
                    server->start([this](unsigned short port) {
                        notify_status(BridgeStatus::SERVER_STARTED, "ws:" + std::to_string(port));
                    });
                } catch (const std::exception& ex) {
                    notify_status(BridgeStatus::SERVER_START_FAILED, "ws", ex.what());
                }
            });
        }

        void configure_http_routes(
                const std::shared_ptr<HttpServer>& server,
                const std::shared_ptr<BridgeProtocolServerConfig>& config) {
            server->resource[regex_path(config->health_path)]["GET"] =
                [this, config](
                    std::shared_ptr<HttpServer::Response> response,
                    std::shared_ptr<HttpServer::Request>) {
                    write_http_json(
                        response,
                        SimpleWeb::StatusCode::success_ok,
                        nlohmann::json{
                            {"ok", true},
                            {"protocol_version", "1"},
                            {"http_port", bound_http_port()},
                            {"websocket_port", bound_websocket_port()}
                        },
                        *config);
                };

            server->resource[regex_path(config->command_path)]["OPTIONS"] =
                [config](
                    std::shared_ptr<HttpServer::Response> response,
                    std::shared_ptr<HttpServer::Request>) {
                    write_http_json(
                        response,
                        SimpleWeb::StatusCode::success_ok,
                        nlohmann::json{{"ok", true}},
                        *config);
                };

            server->resource[regex_path(config->command_path)]["POST"] =
                [this, config](
                    std::shared_ptr<HttpServer::Response> response,
                    std::shared_ptr<HttpServer::Request> request) {
                    handle_http_command(config, response, request);
                };
        }

        void configure_ws_routes(
                const std::shared_ptr<WsServer>& server,
                const std::shared_ptr<BridgeProtocolServerConfig>& config) {
            auto& endpoint = server->endpoint[regex_path(config->websocket_path)];
            endpoint.on_open =
                [this, config](std::shared_ptr<WsServer::Connection> connection) {
                    if (!detail::authorized(*config, connection->header)) {
                        connection->send_close(1008, "authorization_failed");
                        return;
                    }
                    {
                        std::lock_guard<std::mutex> lock(m_state->mutex);
                        m_state->ws_connections.insert(connection);
                    }
                    notify_status(BridgeStatus::CLIENT_CONNECTED, "ws");
                };
            endpoint.on_close =
                [this](std::shared_ptr<WsServer::Connection> connection, int, const std::string&) {
                    {
                        std::lock_guard<std::mutex> lock(m_state->mutex);
                        m_state->ws_connections.erase(connection);
                    }
                    notify_status(BridgeStatus::CLIENT_DISCONNECTED, "ws");
                };
            endpoint.on_error =
                [this](std::shared_ptr<WsServer::Connection>, const SimpleWeb::error_code& ec) {
                    notify_status(BridgeStatus::CONNECTION_ERROR, "ws", ec.message());
                };
            endpoint.on_message =
                [this, config](
                    std::shared_ptr<WsServer::Connection> connection,
                    std::shared_ptr<WsServer::InMessage> message) {
                    const auto body = message->string();
                    const auto response = handle_message_body(*config, body);
                    connection->send(response.dump(-1));
                };
        }

        static void write_http_json(
                const std::shared_ptr<HttpServer::Response>& response,
                SimpleWeb::StatusCode status,
                nlohmann::json body,
                const BridgeProtocolServerConfig& config) {
            response->close_connection_after_response = true;
            response->write(status, body.dump(-1), detail::json_headers(config));
        }

        void handle_http_command(
                const std::shared_ptr<BridgeProtocolServerConfig>& config,
                const std::shared_ptr<HttpServer::Response>& response,
                const std::shared_ptr<HttpServer::Request>& request) {
            if (!detail::authorized(*config, request->header)) {
                write_http_json(
                    response,
                    SimpleWeb::StatusCode::client_error_unauthorized,
                    detail::jsonrpc_error(
                        nullptr,
                        detail::jsonrpc_authorization_failed,
                        "Authorization failed.",
                        nlohmann::json{{"code", "authorization_failed"}}),
                    *config);
                return;
            }

            const auto body = request->content.string();
            if (body.size() > config->request_body_limit) {
                write_http_json(
                    response,
                    SimpleWeb::StatusCode::client_error_payload_too_large,
                    detail::jsonrpc_error(
                        nullptr,
                        detail::jsonrpc_invalid_request,
                        "Request body is too large.",
                        nlohmann::json{{"code", "request_body_too_large"}}),
                    *config);
                return;
            }

            write_http_json(
                response,
                SimpleWeb::StatusCode::success_ok,
                handle_message_body(*config, body),
                *config);
        }

        nlohmann::json handle_message_body(
                const BridgeProtocolServerConfig& config,
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
                const BridgeProtocolServerConfig& config,
                const nlohmann::json& request) {
            const auto id = request.contains("id") ? request.at("id") : nlohmann::json(nullptr);
            if (!request.is_object() ||
                request.value("jsonrpc", std::string()) != "2.0" ||
                !request.contains("id") ||
                !request.contains("method") ||
                !request.at("method").is_string()) {
                return detail::jsonrpc_error(
                    id,
                    detail::jsonrpc_invalid_request,
                    "Invalid JSON-RPC request.");
            }

            const auto method = request.at("method").get<std::string>();
            const auto params =
                request.contains("params") ? request.at("params") : nlohmann::json::object();

            if (method == "protocol.hello") {
                return detail::jsonrpc_result(
                    id,
                    nlohmann::json{
                        {"selected_protocol_version", "1"},
                        {"installation_id", config.installation_id},
                        {"server_instance_id", config.server_instance_id},
                        {"session_id", m_stream_id}
                    });
            }
            if (method == "protocol.capabilities.get") {
                auto result = detail::capabilities_snapshot();
                result["server_instance_id"] = config.server_instance_id;
                result["features"]["http"] = config.enable_http;
                result["features"]["websocket"] = config.enable_websocket;
                result["limits"]["max_message_bytes"] = config.request_body_limit;
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

        nlohmann::json handle_trade_affecting_command(
                const BridgeProtocolServerConfig& config,
                const nlohmann::json& id,
                const std::string& method,
                const nlohmann::json& params,
                const bool direct_trade_open) {
            const auto idempotency_key = metatrader_file::detail::context_idempotency_key(params);
            if (idempotency_key.empty()) {
                return detail::jsonrpc_error(
                    id,
                    detail::jsonrpc_invalid_params,
                    "Trade-affecting commands require context.idempotency_key.");
            }

            const auto operation_key = method + "\n" + idempotency_key;
            const auto request_key = detail::json_id_to_key(id).empty()
                ? std::string()
                : method + "\n" + detail::json_id_to_key(id);
            const auto fingerprint =
                metatrader_file::detail::canonical_trade_command_fingerprint(
                    params,
                    direct_trade_open);

            {
                std::lock_guard<std::mutex> lock(m_state->mutex);
                if (!request_key.empty()) {
                    const auto it = m_state->request_id_index.find(request_key);
                    if (it != m_state->request_id_index.end() &&
                        it->second != operation_key) {
                        return detail::jsonrpc_result(id, idempotency_conflict_result(
                            "The same JSON-RPC id was used with a different operation."));
                    }
                }

                const auto existing = m_state->operations.find(operation_key);
                if (existing != m_state->operations.end()) {
                    if (existing->second.fingerprint == fingerprint) {
                        if (!request_key.empty()) {
                            m_state->request_id_index[request_key] = operation_key;
                        }
                        return detail::jsonrpc_result(id, existing->second.result);
                    }
                    return detail::jsonrpc_result(id, idempotency_conflict_result(
                        "The same idempotency_key was used with a different payload."));
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
                remember_operation(config, operation_key, request_key, fingerprint, result);
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
            const auto result = nlohmann::json{
                {"status", "accepted"},
                {"final", false},
                {"operation_id", "mem:" + std::to_string(config.bridge_id) + ":" + idempotency_key},
                {"signal_ref", {
                    {"signal_id", std::to_string(signal_id)}
                }}
            };

            callback(std::move(signal));
            remember_operation(config, operation_key, request_key, fingerprint, result);
            return detail::jsonrpc_result(id, result);
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

        void remember_operation(
                const BridgeProtocolServerConfig& config,
                const std::string& operation_key,
                const std::string& request_key,
                const std::string& fingerprint,
                const nlohmann::json& result) {
            std::lock_guard<std::mutex> lock(m_state->mutex);
            if (m_state->operations.find(operation_key) == m_state->operations.end()) {
                m_state->operation_order.push_back(operation_key);
            }
            m_state->operations[operation_key] =
                StoredOperation{fingerprint, result, request_key};
            if (!request_key.empty()) {
                m_state->request_id_index[request_key] = operation_key;
            }

            while (m_state->operation_order.size() > config.dedupe_cache_size) {
                const auto oldest = m_state->operation_order.front();
                m_state->operation_order.pop_front();
                const auto it = m_state->operations.find(oldest);
                if (it != m_state->operations.end()) {
                    if (!it->second.request_storage_key.empty()) {
                        m_state->request_id_index.erase(it->second.request_storage_key);
                    }
                    m_state->operations.erase(it);
                }
            }
        }

        void broadcast_ws_notification(nlohmann::json notification) {
            std::vector<std::shared_ptr<WsServer::Connection>> connections;
            {
                std::lock_guard<std::mutex> lock(m_state->mutex);
                connections.assign(
                    m_state->ws_connections.begin(),
                    m_state->ws_connections.end());
            }
            const auto text = notification.dump(-1);
            for (auto& connection : connections) {
                if (connection) {
                    connection->send(text);
                }
            }
        }
    };

} // namespace optionx::bridges::protocol_v1

#endif // OPTIONX_HEADER_BRIDGES_PROTOCOL_V1_BRIDGE_PROTOCOL_SERVER_BRIDGE_HPP_INCLUDED
