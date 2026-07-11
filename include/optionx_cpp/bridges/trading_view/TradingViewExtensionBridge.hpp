#pragma once
#ifndef OPTIONX_HEADER_BRIDGES_TRADING_VIEW_TRADING_VIEW_EXTENSION_BRIDGE_HPP_INCLUDED
#define OPTIONX_HEADER_BRIDGES_TRADING_VIEW_TRADING_VIEW_EXTENSION_BRIDGE_HPP_INCLUDED

/// \file TradingViewExtensionBridge.hpp
/// \brief Defines the TradingView browser-extension HTTP bridge.

#include "TradingViewExtensionBridgeConfig.hpp"

#include <server_http.hpp>

#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_set>

namespace optionx::bridges::tradingview {

    /// \class TradingViewExtensionBridge
    /// \brief Receives TradingView extension alerts over local HTTP and publishes TradeSignal events.
    ///
    /// The bridge is intentionally narrow: it converts already-detected
    /// TradingView events into library signals. It does not execute trades and
    /// it does not assign meaning to level-alert crossings unless the user
    /// configured an explicit level alert rule.
    class TradingViewExtensionBridge final : public BaseBridge {
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

        struct RuntimeState {
            std::mutex mutex;
            bridge_status_callback_t status_callback;
            BaseBridge::trade_signal_callback_t trade_signal_callback;
            BaseBridge::signal_id_allocator_t signal_id_allocator;
            std::shared_ptr<HttpServer> server;
            std::thread server_thread;
            std::deque<std::string> dedupe_order;
            std::unordered_set<std::string> dedupe_keys;
            bool running = false;
        };

    public:
        /// \brief Constructs a bridge with empty runtime state.
        TradingViewExtensionBridge()
            : m_state(std::make_shared<RuntimeState>()) {}

        /// \brief Stops the HTTP server before destruction.
        ~TradingViewExtensionBridge() override {
            shutdown();
        }

        /// \brief Configures the bridge with TradingView HTTP settings.
        /// \param config Configuration object. Must be `TradingViewExtensionBridgeConfig`.
        /// \return `true` when configuration is valid and accepted.
        bool configure(std::unique_ptr<IBridgeConfig> config) override {
            if (!config) return false;

            const auto* typed =
                dynamic_cast<const TradingViewExtensionBridgeConfig*>(config.get());
            if (!typed) {
                config->dispatch_callbacks(false, "Invalid TradingView extension bridge config type.");
                return false;
            }

            auto next_config = std::make_shared<TradingViewExtensionBridgeConfig>(*typed);
            const auto validation = next_config->validate();
            config->dispatch_callbacks(validation.first, validation.second);
            if (!validation.first) {
                return false;
            }

            std::lock_guard<std::mutex> lock(m_mutex);
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

        /// \brief Returns the signal ID allocator slot.
        signal_id_allocator_t& on_signal_id() override {
            return m_state->signal_id_allocator;
        }

        /// \brief Ignores account updates; sizing percent is resolved downstream.
        /// \param info Account update received from the trading platform.
        void update_account_info(const AccountInfoUpdate& info) override {
            (void)info;
        }

        /// \brief Starts the local HTTP server.
        void run() override {
            auto config = get_config_or_throw();
            if (!get_signal_id_allocator()) {
                notify_status(
                    BridgeStatus::SERVER_START_FAILED,
                    {},
                    "TradingView extension bridge requires a signal ID allocator.");
                return;
            }

            {
                std::lock_guard<std::mutex> lock(m_state->mutex);
                if (m_state->running) {
                    return;
                }
            }

            auto server = std::make_shared<HttpServer>();
            server->config.address = config->address;
            server->config.port = config->port;
            server->config.thread_pool_size = 1;
            configure_routes(server, config);

            {
                std::lock_guard<std::mutex> lock(m_state->mutex);
                m_state->server = server;
                m_state->dedupe_order.clear();
                m_state->dedupe_keys.clear();
                m_state->running = true;
                m_state->server_thread = std::thread([state = m_state, server]() {
                    try {
                        server->start([state](unsigned short port) {
                            notify_status(
                                state,
                                BridgeStatus::SERVER_STARTED,
                                std::to_string(port));
                        });
                    } catch (const std::exception& ex) {
                        {
                            std::lock_guard<std::mutex> lock(state->mutex);
                            state->running = false;
                        }
                        notify_status(state, BridgeStatus::SERVER_START_FAILED, {}, ex.what());
                    }
                });
            }
        }

        /// \brief Stops the HTTP server and joins the server thread.
        void shutdown() override {
            std::shared_ptr<HttpServer> server;
            std::thread server_thread;
            bool was_running = false;
            {
                std::lock_guard<std::mutex> lock(m_state->mutex);
                was_running =
                    m_state->running ||
                    static_cast<bool>(m_state->server) ||
                    m_state->server_thread.joinable();
                server = m_state->server;
                m_state->server.reset();
                m_state->running = false;
                if (m_state->server_thread.joinable()) {
                    server_thread = std::move(m_state->server_thread);
                }
            }

            if (server) {
                server->stop();
            }
            if (server_thread.joinable() &&
                server_thread.get_id() != std::this_thread::get_id()) {
                server_thread.join();
            } else if (server_thread.joinable()) {
                server_thread.detach();
            }
            if (was_running) {
                notify_status(BridgeStatus::SERVER_STOPPED);
            }
        }

        /// \brief Returns the currently bound HTTP port.
        /// \return Non-zero bound port when the server has started.
        unsigned short bound_port() const noexcept {
            std::shared_ptr<HttpServer> server;
            {
                std::lock_guard<std::mutex> lock(m_state->mutex);
                server = m_state->server;
            }
            return server ? server->bound_port() : 0;
        }

    private:
        mutable std::mutex m_mutex;
        std::shared_ptr<RuntimeState> m_state;
        std::shared_ptr<TradingViewExtensionBridgeConfig> m_config;

        std::shared_ptr<TradingViewExtensionBridgeConfig> get_config_or_throw() const {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_config) {
                throw std::invalid_argument("TradingView extension bridge is not configured.");
            }
            return m_config;
        }

        signal_id_allocator_t get_signal_id_allocator() const {
            std::lock_guard<std::mutex> lock(m_state->mutex);
            return m_state->signal_id_allocator;
        }

        void notify_status(
                BridgeStatus status,
                std::string connection_id = {},
                std::string message = {}) const {
            notify_status(m_state, status, std::move(connection_id), std::move(message));
        }

        static void notify_status(
                const std::shared_ptr<RuntimeState>& state,
                BridgeStatus status,
                std::string connection_id = {},
                std::string message = {}) {
            bridge_status_callback_t callback;
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                callback = state->status_callback;
            }
            if (callback) {
                callback(BridgeStatusUpdate(
                    status,
                    std::move(connection_id),
                    std::move(message)));
            }
        }

        static std::string regex_path(const std::string& path) {
            std::string pattern = "^";
            for (const auto ch : path) {
                switch (ch) {
                case '.':
                case '\\':
                case '+':
                case '*':
                case '?':
                case '[':
                case '^':
                case ']':
                case '$':
                case '(':
                case ')':
                case '{':
                case '}':
                case '=':
                case '!':
                case '<':
                case '>':
                case '|':
                case ':':
                case '-':
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

        static SimpleWeb::CaseInsensitiveMultimap json_headers(
                const TradingViewExtensionBridgeConfig& config) {
            SimpleWeb::CaseInsensitiveMultimap headers;
            headers.emplace("Content-Type", "application/json");
            headers.emplace("Connection", "close");
            if (config.allow_cors) {
                headers.emplace("Access-Control-Allow-Origin", config.allowed_origin);
                headers.emplace("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
                headers.emplace(
                    "Access-Control-Allow-Headers",
                    "Content-Type, X-OptionX-Secret, Authorization");
            }
            return headers;
        }

        static void write_json(
                const std::shared_ptr<HttpServer::Response>& response,
                SimpleWeb::StatusCode status,
                nlohmann::json body,
                const TradingViewExtensionBridgeConfig& config) {
            response->close_connection_after_response = true;
            response->write(status, body.dump(), json_headers(config));
        }

        void configure_routes(
                const std::shared_ptr<HttpServer>& server,
                const std::shared_ptr<TradingViewExtensionBridgeConfig>& config) {
            server->resource[regex_path(config->health_path)]["GET"] =
                [config](
                    std::shared_ptr<HttpServer::Response> response,
                    std::shared_ptr<HttpServer::Request>) {
                    write_json(
                        response,
                        SimpleWeb::StatusCode::success_ok,
                        nlohmann::json{{"ok", true}},
                        *config);
                };

            server->resource[regex_path(config->signal_path)]["OPTIONS"] =
                [config](
                    std::shared_ptr<HttpServer::Response> response,
                    std::shared_ptr<HttpServer::Request>) {
                    write_json(
                        response,
                        SimpleWeb::StatusCode::success_ok,
                        nlohmann::json{{"ok", true}},
                        *config);
                };

            server->resource[regex_path(config->signal_path)]["POST"] =
                [state = m_state, config](
                    std::shared_ptr<HttpServer::Response> response,
                    std::shared_ptr<HttpServer::Request> request) {
                    handle_signal_post(state, config, response, request);
                };
        }

        static void handle_signal_post(
                const std::shared_ptr<RuntimeState>& state,
                const std::shared_ptr<TradingViewExtensionBridgeConfig>& config,
                const std::shared_ptr<HttpServer::Response>& response,
                const std::shared_ptr<HttpServer::Request>& request) {
            const auto body = request->content.string();
            if (body.size() > config->request_body_limit) {
                write_json(
                    response,
                    SimpleWeb::StatusCode::client_error_payload_too_large,
                    nlohmann::json{
                        {"ok", false},
                        {"accepted", false},
                        {"reason", "request_body_too_large"}
                    },
                    *config);
                return;
            }

            nlohmann::json payload;
            try {
                payload = nlohmann::json::parse(body);
            } catch (const std::exception& ex) {
                write_json(
                    response,
                    SimpleWeb::StatusCode::client_error_bad_request,
                    nlohmann::json{
                        {"ok", false},
                        {"accepted", false},
                        {"reason", "invalid_json"},
                        {"message", ex.what()}
                    },
                    *config);
                return;
            }

            const auto request_secret =
                request_header_value(request->header, "X-OptionX-Secret");
            auto result =
                detail::parse_extension_payload(payload, request_secret, *config);
            if (!result.authorized) {
                write_json(
                    response,
                    SimpleWeb::StatusCode::client_error_unauthorized,
                    std::move(result.response),
                    *config);
                return;
            }

            if (!result.accepted) {
                write_json(
                    response,
                    SimpleWeb::StatusCode::success_ok,
                    std::move(result.response),
                    *config);
                return;
            }

            if (!remember_dedupe_key(state, result.dedupe_key, config->dedupe_cache_size)) {
                write_json(
                    response,
                    SimpleWeb::StatusCode::success_ok,
                    nlohmann::json{
                        {"ok", true},
                        {"accepted", false},
                        {"duplicate", true},
                        {"reason", "duplicate"},
                        {"event_id", result.event_id},
                        {"dedupe_key", result.dedupe_key}
                    },
                    *config);
                return;
            }

            signal_id_allocator_t allocator;
            trade_signal_callback_t callback;
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                allocator = state->signal_id_allocator;
                callback = state->trade_signal_callback;
            }
            if (!allocator) {
                forget_dedupe_key(state, result.dedupe_key);
                write_json(
                    response,
                    SimpleWeb::StatusCode::server_error_internal_server_error,
                    nlohmann::json{
                        {"ok", false},
                        {"accepted", false},
                        {"reason", "missing_signal_id_allocator"}
                    },
                    *config);
                return;
            }

            const auto signal_id = allocator();
            if (signal_id == 0) {
                forget_dedupe_key(state, result.dedupe_key);
                write_json(
                    response,
                    SimpleWeb::StatusCode::server_error_internal_server_error,
                    nlohmann::json{
                        {"ok", false},
                        {"accepted", false},
                        {"reason", "signal_id_allocation_failed"}
                    },
                    *config);
                return;
            }

            result.signal->signal_id = signal_id;
            result.signal->bridge_id = config->bridge_id;
            if (callback) {
                callback(std::move(result.signal));
            }
            write_json(
                response,
                SimpleWeb::StatusCode::success_ok,
                std::move(result.response),
                *config);
        }

        static std::string request_header_value(
                const SimpleWeb::CaseInsensitiveMultimap& headers,
                const std::string& name) {
            const auto it = headers.find(name);
            return it == headers.end() ? std::string() : it->second;
        }

        static bool remember_dedupe_key(
                const std::shared_ptr<RuntimeState>& state,
                const std::string& dedupe_key,
                std::size_t cache_size) {
            if (dedupe_key.empty()) {
                return true;
            }
            std::lock_guard<std::mutex> lock(state->mutex);
            if (state->dedupe_keys.find(dedupe_key) != state->dedupe_keys.end()) {
                return false;
            }
            state->dedupe_keys.insert(dedupe_key);
            state->dedupe_order.push_back(dedupe_key);
            while (state->dedupe_order.size() > cache_size) {
                state->dedupe_keys.erase(state->dedupe_order.front());
                state->dedupe_order.pop_front();
            }
            return true;
        }

        static void forget_dedupe_key(
                const std::shared_ptr<RuntimeState>& state,
                const std::string& dedupe_key) {
            if (dedupe_key.empty()) {
                return;
            }
            std::lock_guard<std::mutex> lock(state->mutex);
            state->dedupe_keys.erase(dedupe_key);
        }
    };

} // namespace optionx::bridges::tradingview

#endif // OPTIONX_HEADER_BRIDGES_TRADING_VIEW_TRADING_VIEW_EXTENSION_BRIDGE_HPP_INCLUDED
