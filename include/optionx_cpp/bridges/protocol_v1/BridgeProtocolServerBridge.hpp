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
            std::int64_t completed_at_ms = 0;
            std::size_t byte_size = 0;
            bool dispatching = false;
        };

        struct WsConnectionState {
            std::shared_ptr<WsServer::Connection> connection;
            std::size_t pending_messages = 0;
            std::size_t pending_bytes = 0;
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
            std::unordered_map<WsServer::Connection*, WsConnectionState> ws_connections;
            std::unordered_map<std::string, StoredOperation> operations;
            std::unordered_map<std::string, std::uint64_t> event_revisions;
            std::deque<std::string> operation_order;
            std::string stream_id;
            std::size_t operation_cache_bytes = 0;
            std::uint64_t event_seq = 0;
            std::uint64_t lifecycle_generation = 0;
            std::uint64_t stopping_generation = 0;
            std::thread::id stopping_http_thread_id;
            std::thread::id stopping_ws_thread_id;
            RuntimePhase phase = RuntimePhase::Stopped;
            bool stop_notified = false;
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
            if (m_state->phase != RuntimePhase::Stopped) {
                config->dispatch_callbacks(
                    false,
                    "Bridge Protocol v1 server cannot be reconfigured while running.");
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
            broadcast_ws_notification(std::move(notification));
        }

        /// \brief Starts configured HTTP and WebSocket transports.
        void run() override {
            auto config = get_config_or_throw();
            std::uint64_t generation = 0;

            {
                std::unique_lock<std::mutex> lock(m_state->mutex);
                if (m_state->phase == RuntimePhase::Running ||
                    m_state->phase == RuntimePhase::Starting) {
                    return;
                }
                while (m_state->phase == RuntimePhase::Stopping) {
                    m_state->lifecycle_cv.wait(lock);
                }
            }
            join_stopped_threads();

            {
                std::lock_guard<std::mutex> lock(m_state->mutex);
                if (m_state->phase != RuntimePhase::Stopped) {
                    return;
                }
                m_state->phase = RuntimePhase::Starting;
                generation = ++m_state->lifecycle_generation;
                m_state->operation_order.clear();
                m_state->operations.clear();
                m_state->operation_cache_bytes = 0;
                m_state->event_revisions.clear();
                m_state->ws_connections.clear();
                m_state->event_seq = 0;
                m_state->stream_id = make_stream_id();
                m_state->stop_notified = false;
            }

            try {
                if (config->enable_http) {
                    if (!start_http_server(config, generation)) {
                        shutdown_for_generation(generation, true);
                        return;
                    }
                }
                if (is_runtime_running() && config->enable_websocket) {
                    if (!start_ws_server(config, generation)) {
                        shutdown_for_generation(generation, true);
                        return;
                    }
                }
                {
                    std::lock_guard<std::mutex> lock(m_state->mutex);
                    if (m_state->phase == RuntimePhase::Starting &&
                        m_state->lifecycle_generation == generation) {
                        m_state->phase = RuntimePhase::Running;
                        m_state->lifecycle_cv.notify_all();
                    }
                }
            } catch (...) {
                shutdown_for_generation(generation, true);
                throw;
            }
        }

        /// \brief Stops configured HTTP and WebSocket transports.
        void shutdown() override {
            shutdown_for_generation(0, false);
        }

    private:
        std::shared_ptr<RuntimeState> m_state;

        void shutdown_for_generation(
                const std::uint64_t expected_generation,
                const bool require_generation) {
            std::shared_ptr<HttpServer> http_server;
            std::shared_ptr<WsServer> ws_server;
            std::thread http_thread;
            std::thread ws_thread;
            bool notify_stopped = false;
            bool owns_shutdown = false;
            std::uint64_t stopping_generation = 0;
            const auto current_id = std::this_thread::get_id();

            {
                std::unique_lock<std::mutex> lock(m_state->mutex);
                if (m_state->phase == RuntimePhase::Stopping) {
                    if (current_id == m_state->stopping_http_thread_id ||
                        current_id == m_state->stopping_ws_thread_id) {
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
                if (require_generation &&
                    m_state->lifecycle_generation != expected_generation) {
                    return;
                }

                owns_shutdown = true;
                stopping_generation = m_state->lifecycle_generation;
                m_state->phase = RuntimePhase::Stopping;
                m_state->stopping_generation = stopping_generation;
                m_state->stopping_http_thread_id = m_state->http_thread.joinable()
                    ? m_state->http_thread.get_id()
                    : std::thread::id{};
                m_state->stopping_ws_thread_id = m_state->ws_thread.joinable()
                    ? m_state->ws_thread.get_id()
                    : std::thread::id{};
                http_server = m_state->http_server;
                ws_server = m_state->ws_server;
                m_state->http_server.reset();
                m_state->ws_server.reset();
                m_state->ws_connections.clear();
                if (m_state->config) {
                    fail_dispatching_operations_locked(*m_state->config, server_stopped_result());
                }
                if (!m_state->stop_notified) {
                    m_state->stop_notified = true;
                    notify_stopped = true;
                }
                if (m_state->http_thread.joinable() &&
                    m_state->http_thread.get_id() != current_id) {
                    http_thread = std::move(m_state->http_thread);
                } else if (m_state->http_thread.joinable() &&
                           m_state->http_thread.get_id() == current_id) {
                    m_state->http_thread.detach();
                }
                if (m_state->ws_thread.joinable() &&
                    m_state->ws_thread.get_id() != current_id) {
                    ws_thread = std::move(m_state->ws_thread);
                } else if (m_state->ws_thread.joinable() &&
                           m_state->ws_thread.get_id() == current_id) {
                    m_state->ws_thread.detach();
                }
            }

            if (!owns_shutdown) {
                return;
            }
            if (http_server) {
                http_server->stop();
            }
            if (ws_server) {
                ws_server->stop();
            }
            if (http_thread.joinable()) {
                http_thread.join();
            }
            if (ws_thread.joinable()) {
                ws_thread.join();
            }

            {
                std::lock_guard<std::mutex> lock(m_state->mutex);
                if (m_state->phase == RuntimePhase::Stopping &&
                    m_state->stopping_generation == stopping_generation) {
                    m_state->phase = RuntimePhase::Stopped;
                    m_state->stopping_generation = 0;
                    m_state->stopping_http_thread_id = std::thread::id{};
                    m_state->stopping_ws_thread_id = std::thread::id{};
                    m_state->lifecycle_cv.notify_all();
                }
            }

            if (notify_stopped) {
                notify_status(BridgeStatus::SERVER_STOPPED, {});
            }
        }

        void shutdown_failed_start(const std::uint64_t generation) {
            shutdown_for_generation(generation, true);
        }

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

        std::string current_stream_id() const {
            std::lock_guard<std::mutex> lock(m_state->mutex);
            return m_state->stream_id;
        }

        bool is_runtime_running() const {
            std::lock_guard<std::mutex> lock(m_state->mutex);
            return m_state->phase == RuntimePhase::Starting ||
                   m_state->phase == RuntimePhase::Running;
        }

        void join_stopped_threads() {
            std::thread http_thread;
            std::thread ws_thread;
            const auto current_id = std::this_thread::get_id();
            {
                std::lock_guard<std::mutex> lock(m_state->mutex);
                if (m_state->phase == RuntimePhase::Starting ||
                    m_state->phase == RuntimePhase::Running ||
                    m_state->phase == RuntimePhase::Stopping) {
                    return;
                }
                if (m_state->http_thread.joinable() &&
                    m_state->http_thread.get_id() != current_id) {
                    http_thread = std::move(m_state->http_thread);
                } else if (m_state->http_thread.joinable() &&
                           m_state->http_thread.get_id() == current_id) {
                    m_state->http_thread.detach();
                }
                if (m_state->ws_thread.joinable() &&
                    m_state->ws_thread.get_id() != current_id) {
                    ws_thread = std::move(m_state->ws_thread);
                } else if (m_state->ws_thread.joinable() &&
                           m_state->ws_thread.get_id() == current_id) {
                    m_state->ws_thread.detach();
                }
            }
            if (http_thread.joinable()) {
                http_thread.join();
            }
            if (ws_thread.joinable()) {
                ws_thread.join();
            }
            {
                std::lock_guard<std::mutex> lock(m_state->mutex);
                m_state->lifecycle_cv.notify_all();
            }
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
            return "optionx://bridge/protocol_v1/" +
                   config.installation_id +
                   "/" +
                   config.server_instance_id +
                   "/" +
                   std::to_string(config.bridge_id);
        }

        static std::string make_stream_id() {
            std::ostringstream out;
            out << "bridge-protocol-v1-"
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
                const BridgeProtocolServerConfig& config,
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
                std::string connection_id,
                std::string message = {}) const {
            bridge_status_callback_t callback;
            {
                std::lock_guard<std::mutex> lock(m_state->mutex);
                callback = m_state->status_callback;
            }
            if (callback) {
                try {
                    callback(BridgeStatusUpdate{
                        status,
                        std::move(connection_id),
                        std::move(message)
                    });
                } catch (...) {
                }
            }
        }

        void notify_signal_report(BridgeSignalReport report) const {
            signal_report_callback_t callback;
            {
                std::lock_guard<std::mutex> lock(m_state->mutex);
                callback = m_state->signal_report_callback;
            }
            if (callback) {
                try {
                    callback(report);
                } catch (...) {
                }
            }
        }

        bool start_http_server(
                const std::shared_ptr<BridgeProtocolServerConfig>& config,
                const std::uint64_t generation) {
            auto server = std::make_shared<HttpServer>();
            auto ready = std::make_shared<std::promise<bool>>();
            auto ready_set = std::make_shared<std::atomic<bool>>(false);
            auto ready_future = ready->get_future();
            server->config.address = config->address;
            server->config.port = config->http_port;
            server->config.thread_pool_size = 2;
            server->config.max_request_streambuf_size =
                config->request_body_limit == (std::numeric_limits<std::size_t>::max)()
                    ? config->request_body_limit
                    : config->request_body_limit + 1;
            server->config.timeout_content = config->content_timeout_seconds;
            configure_http_routes(server, config);

            {
                std::lock_guard<std::mutex> lock(m_state->mutex);
                if (m_state->phase != RuntimePhase::Starting ||
                    m_state->lifecycle_generation != generation) {
                    return false;
                }
                m_state->http_server = server;
                m_state->http_thread = std::thread([this, server, generation, ready, ready_set]() {
                    try {
                        server->start([this, ready, ready_set](unsigned short port) {
                            bool expected = false;
                            if (ready_set->compare_exchange_strong(expected, true)) {
                                ready->set_value(true);
                            }
                            notify_status(
                                BridgeStatus::SERVER_STARTED,
                                "http:" + std::to_string(port));
                        });
                    } catch (const std::exception& ex) {
                        bool expected = false;
                        if (ready_set->compare_exchange_strong(expected, true)) {
                            ready->set_value(false);
                        }
                        notify_status(BridgeStatus::SERVER_START_FAILED, "http", ex.what());
                        shutdown_failed_start(generation);
                    }
                });
            }
            return ready_future.get();
        }

        bool start_ws_server(
                const std::shared_ptr<BridgeProtocolServerConfig>& config,
                const std::uint64_t generation) {
            auto server = std::make_shared<WsServer>();
            auto ready = std::make_shared<std::promise<bool>>();
            auto ready_set = std::make_shared<std::atomic<bool>>(false);
            auto ready_future = ready->get_future();
            server->config.address = config->address;
            server->config.port = config->websocket_port;
            server->config.thread_pool_size = 1;
            server->config.max_message_size = config->request_body_limit;
            configure_ws_routes(server, config);

            {
                std::lock_guard<std::mutex> lock(m_state->mutex);
                if (m_state->phase != RuntimePhase::Starting ||
                    m_state->lifecycle_generation != generation) {
                    return false;
                }
                m_state->ws_server = server;
                m_state->ws_thread = std::thread([this, server, generation, ready, ready_set]() {
                    try {
                        server->start([this, ready, ready_set](unsigned short port) {
                            bool expected = false;
                            if (ready_set->compare_exchange_strong(expected, true)) {
                                ready->set_value(true);
                            }
                            notify_status(BridgeStatus::SERVER_STARTED, "ws:" + std::to_string(port));
                        });
                    } catch (const std::exception& ex) {
                        bool expected = false;
                        if (ready_set->compare_exchange_strong(expected, true)) {
                            ready->set_value(false);
                        }
                        notify_status(BridgeStatus::SERVER_START_FAILED, "ws", ex.what());
                        shutdown_failed_start(generation);
                    }
                });
            }
            return ready_future.get();
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
            endpoint.on_handshake =
                [config](
                    std::shared_ptr<WsServer::Connection> connection,
                    SimpleWeb::CaseInsensitiveMultimap& response_header) {
                    if (!websocket_subprotocol_accepted(*config, connection->header)) {
                        return SimpleWeb::StatusCode::client_error_upgrade_required;
                    }
                    if (!config->websocket_subprotocol.empty()) {
                        response_header.emplace(
                            "Sec-WebSocket-Protocol",
                            config->websocket_subprotocol);
                    }
                    return SimpleWeb::StatusCode::information_switching_protocols;
                };
            endpoint.on_open =
                [this, config](std::shared_ptr<WsServer::Connection> connection) {
                    if (!detail::authorized(*config, connection->header)) {
                        connection->send_close(1008, "authorization_failed");
                        return;
                    }
                    {
                        std::lock_guard<std::mutex> lock(m_state->mutex);
                        WsConnectionState state;
                        state.connection = connection;
                        m_state->ws_connections[connection.get()] = std::move(state);
                    }
                    notify_status(BridgeStatus::CLIENT_CONNECTED, "ws");
                };
            endpoint.on_close =
                [this](std::shared_ptr<WsServer::Connection> connection, int, const std::string&) {
                    {
                        std::lock_guard<std::mutex> lock(m_state->mutex);
                        m_state->ws_connections.erase(connection.get());
                    }
                    notify_status(BridgeStatus::CLIENT_DISCONNECTED, "ws");
                };
            endpoint.on_error =
                [this](std::shared_ptr<WsServer::Connection> connection, const SimpleWeb::error_code& ec) {
                    {
                        std::lock_guard<std::mutex> lock(m_state->mutex);
                        m_state->ws_connections.erase(connection.get());
                    }
                    notify_status(BridgeStatus::CONNECTION_ERROR, "ws", ec.message());
                };
            endpoint.on_message =
                [this, config](
                    std::shared_ptr<WsServer::Connection> connection,
                    std::shared_ptr<WsServer::InMessage> message) {
                    try {
                        const auto body = message->string();
                        const auto response = handle_message_body(*config, body);
                        send_ws_text(config, connection, response.dump(-1));
                    } catch (const std::exception& ex) {
                        notify_status(BridgeStatus::CONNECTION_ERROR, "ws", ex.what());
                        connection->send_close(1011, "handler_failed");
                    } catch (...) {
                        notify_status(BridgeStatus::CONNECTION_ERROR, "ws", "handler_failed");
                        connection->send_close(1011, "handler_failed");
                    }
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

        static bool is_valid_jsonrpc_id(const nlohmann::json& id) {
            return id.is_string() ||
                   id.is_null() ||
                   id.is_number_integer() ||
                   id.is_number_unsigned();
        }

        static bool websocket_subprotocol_accepted(
                const BridgeProtocolServerConfig& config,
                const SimpleWeb::CaseInsensitiveMultimap& headers) {
            const auto requested = detail::header_value(headers, "Sec-WebSocket-Protocol");
            if (requested.empty()) {
                return !config.require_websocket_subprotocol;
            }
            std::istringstream tokens(requested);
            std::string token;
            while (std::getline(tokens, token, ',')) {
                token = metatrader_file::detail::trim_ascii_copy(token);
                if (token == config.websocket_subprotocol) {
                    return true;
                }
            }
            return false;
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

        nlohmann::json handle_jsonrpc_command(
                const BridgeProtocolServerConfig& config,
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
                result["features"]["http"] = config.enable_http;
                result["features"]["websocket"] = config.enable_websocket;
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
                result["limits"]["max_ws_pending_messages"] = config.max_ws_pending_messages;
                result["limits"]["max_ws_pending_bytes"] = config.max_ws_pending_bytes;
                result["websocket"]["subprotocol"] = config.websocket_subprotocol;
                result["websocket"]["subprotocol_required"] =
                    config.require_websocket_subprotocol;
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
                const auto context = reject_unknown_keys(params.at("context"), context_keys, "Command context");
                if (!context.first) {
                    return context;
                }
            }
            if (params.contains("identity")) {
                const auto identity = reject_unknown_keys(params.at("identity"), identity_keys, "Command identity");
                if (!identity.first) {
                    return identity;
                }
            }
            if (params.contains("sizing")) {
                const auto sizing = reject_unknown_keys(params.at("sizing"), sizing_keys, "Command sizing");
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
            const auto trade_schema = reject_unknown_keys(trade, trade_keys, "Command " + std::string(trade_key));
            if (!trade_schema.first) {
                return trade_schema;
            }
            if (trade.contains("amount") && trade.at("amount").is_object()) {
                const auto amount = reject_unknown_keys(trade.at("amount"), amount_keys, "Command amount");
                if (!amount.first) {
                    return amount;
                }
            }
            if (trade.contains("expiry")) {
                const auto expiry = reject_unknown_keys(trade.at("expiry"), expiry_keys, "Command expiry");
                if (!expiry.first) {
                    return expiry;
                }
            }
            return {true, {}};
        }

        nlohmann::json handle_trade_affecting_command(
                const BridgeProtocolServerConfig& config,
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

            const auto idempotency_key = metatrader_file::detail::context_idempotency_key(params);
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
                metatrader_file::detail::canonical_trade_command_payload(
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
                prune_completed_operations_locked(config, byte_size, 1);
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
                        : "signal.submit routing.policy is not supported by this server bridge yet."
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
                        : "signal.submit accounts/all routing is not supported by this server bridge yet."
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
                    static_cast<void>(metatrader_file::detail::int64_value(selector, "account_id", 0));
                } catch (...) {
                    return {
                        false,
                        "Command account routing requires a numeric account_id in this bridge."
                    };
                }
            }
            return {true, {}};
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
                const BridgeProtocolServerConfig& config,
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
                    {"message", "Bridge Protocol v1 server stopped before dispatch completed."}
                }}
            };
        }

        static std::size_t operation_byte_size(
                const std::string& operation_key,
                const std::string& fingerprint,
                const nlohmann::json& result) {
            return operation_key.size() + fingerprint.size() + result.dump(-1).size();
        }

        void touch_operation_locked(const std::string& operation_key) {
            const auto it = std::find(
                m_state->operation_order.begin(),
                m_state->operation_order.end(),
                operation_key);
            if (it == m_state->operation_order.end()) {
                m_state->operation_order.push_back(operation_key);
                return;
            }
            auto key = std::move(*it);
            m_state->operation_order.erase(it);
            m_state->operation_order.push_back(std::move(key));
        }

        bool try_cached_operation_response_locked(
                const BridgeProtocolServerConfig& config,
                const nlohmann::json& id,
                const std::string& operation_key,
                const std::string& fingerprint,
                const std::string& idempotency_key,
                nlohmann::json& response) {
            prune_completed_operations_locked(config);
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
            touch_operation_locked(operation_key);
            response = detail::jsonrpc_result(id, existing->second.result);
            return true;
        }

        bool remember_operation(
                const BridgeProtocolServerConfig& config,
                const std::string& operation_key,
                const std::string& fingerprint,
                const nlohmann::json& result) {
            std::lock_guard<std::mutex> lock(m_state->mutex);
            const auto byte_size = operation_byte_size(operation_key, fingerprint, result);
            auto existing = m_state->operations.find(operation_key);
            prune_completed_operations_locked(
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
                const BridgeProtocolServerConfig& config,
                const std::string& operation_key,
                const nlohmann::json& result) {
            std::lock_guard<std::mutex> lock(m_state->mutex);
            const auto existing = m_state->operations.find(operation_key);
            if (existing != m_state->operations.end()) {
                const auto byte_size =
                    operation_byte_size(operation_key, existing->second.fingerprint, result);
                m_state->operation_cache_bytes =
                    m_state->operation_cache_bytes - existing->second.byte_size + byte_size;
                existing->second.result = result;
                existing->second.byte_size = byte_size;
                existing->second.completed_at_ms = metatrader_file::detail::unix_time_ms();
                existing->second.dispatching = false;
                touch_operation_locked(operation_key);
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
                prune_completed_operations_locked(config);
            }
        }

        void fail_dispatching_operations_locked(
                const BridgeProtocolServerConfig& config,
                const nlohmann::json& result) {
            for (auto& item : m_state->operations) {
                auto& operation = item.second;
                if (!operation.dispatching) {
                    continue;
                }
                const auto byte_size = operation_byte_size(
                    item.first,
                    operation.fingerprint,
                    result);
                m_state->operation_cache_bytes =
                    m_state->operation_cache_bytes - operation.byte_size + byte_size;
                operation.result = result;
                operation.byte_size = byte_size;
                operation.completed_at_ms = metatrader_file::detail::unix_time_ms();
                operation.dispatching = false;
            }
            prune_completed_operations_locked(config);
        }

        bool evict_oldest_completed_operation_locked() {
            for (auto it = m_state->operation_order.begin();
                 it != m_state->operation_order.end();) {
                const auto& key = *it;
                auto existing = m_state->operations.find(key);
                if (existing == m_state->operations.end()) {
                    it = m_state->operation_order.erase(it);
                    continue;
                }
                if (existing->second.dispatching) {
                    ++it;
                    continue;
                }
                m_state->operation_cache_bytes -= existing->second.byte_size;
                m_state->operations.erase(existing);
                m_state->operation_order.erase(it);
                return true;
            }
            return false;
        }

        void prune_completed_operations_locked(
                const BridgeProtocolServerConfig& config,
                const std::size_t required_bytes = 0,
                const std::size_t required_records = 0) {
            const auto now = metatrader_file::detail::unix_time_ms();
            for (auto it = m_state->operation_order.begin(); it != m_state->operation_order.end();) {
                auto existing = m_state->operations.find(*it);
                if (existing == m_state->operations.end()) {
                    it = m_state->operation_order.erase(it);
                    continue;
                }
                const auto& operation = existing->second;
                if (!operation.dispatching &&
                    operation.completed_at_ms > 0 &&
                    now - operation.completed_at_ms >= config.operation_cache_retention_ms) {
                    m_state->operation_cache_bytes -= operation.byte_size;
                    m_state->operations.erase(existing);
                    it = m_state->operation_order.erase(it);
                    continue;
                }
                ++it;
            }

            while (m_state->operations.size() + required_records >
                   config.dedupe_cache_size) {
                if (!evict_oldest_completed_operation_locked()) {
                    break;
                }
            }
            while (required_bytes <= config.max_operation_cache_bytes &&
                   m_state->operation_cache_bytes >
                       config.max_operation_cache_bytes - required_bytes) {
                if (!evict_oldest_completed_operation_locked()) {
                    break;
                }
            }
        }

        bool reserve_ws_send(
                const BridgeProtocolServerConfig& config,
                const std::shared_ptr<WsServer::Connection>& connection,
                const std::size_t byte_count) {
            std::lock_guard<std::mutex> lock(m_state->mutex);
            const auto existing = m_state->ws_connections.find(connection.get());
            if (existing == m_state->ws_connections.end()) {
                return false;
            }
            auto& state = existing->second;
            if (state.pending_messages >= config.max_ws_pending_messages ||
                byte_count > config.max_ws_pending_bytes ||
                state.pending_bytes > config.max_ws_pending_bytes - byte_count) {
                return false;
            }
            ++state.pending_messages;
            state.pending_bytes += byte_count;
            return true;
        }

        static void complete_ws_send(
                const std::shared_ptr<RuntimeState>& state,
                const std::shared_ptr<WsServer::Connection>& connection,
                const std::size_t byte_count,
                const bool erase_connection) {
            std::lock_guard<std::mutex> lock(state->mutex);
            const auto existing = state->ws_connections.find(connection.get());
            if (existing == state->ws_connections.end()) {
                return;
            }
            auto& ws_state = existing->second;
            if (ws_state.pending_messages > 0) {
                --ws_state.pending_messages;
            }
            ws_state.pending_bytes =
                ws_state.pending_bytes > byte_count ? ws_state.pending_bytes - byte_count : 0;
            if (erase_connection) {
                state->ws_connections.erase(existing);
            }
        }

        void send_ws_text(
                const std::shared_ptr<BridgeProtocolServerConfig>& config,
                const std::shared_ptr<WsServer::Connection>& connection,
                const std::string& text) {
            if (!connection || !config) {
                return;
            }
            if (!reserve_ws_send(*config, connection, text.size())) {
                {
                    std::lock_guard<std::mutex> lock(m_state->mutex);
                    m_state->ws_connections.erase(connection.get());
                }
                connection->send_close(1008, "backpressure");
                notify_status(BridgeStatus::CONNECTION_ERROR, "ws", "WebSocket send queue limit exceeded.");
                return;
            }
            auto state = m_state;
            connection->send(
                text,
                [state, connection, byte_count = text.size()](const SimpleWeb::error_code& ec) {
                    BridgeProtocolServerBridge::complete_ws_send(
                        state,
                        connection,
                        byte_count,
                        static_cast<bool>(ec));
                });
        }

        void broadcast_ws_notification(nlohmann::json notification) {
            std::shared_ptr<BridgeProtocolServerConfig> config;
            std::vector<std::shared_ptr<WsServer::Connection>> connections;
            {
                std::lock_guard<std::mutex> lock(m_state->mutex);
                config = m_state->config;
                for (const auto& item : m_state->ws_connections) {
                    if (item.second.connection) {
                        connections.push_back(item.second.connection);
                    }
                }
            }
            const auto text = notification.dump(-1);
            for (auto& connection : connections) {
                send_ws_text(config, connection, text);
            }
        }
    };

} // namespace optionx::bridges::protocol_v1

#endif // OPTIONX_HEADER_BRIDGES_PROTOCOL_V1_BRIDGE_PROTOCOL_SERVER_BRIDGE_HPP_INCLUDED
