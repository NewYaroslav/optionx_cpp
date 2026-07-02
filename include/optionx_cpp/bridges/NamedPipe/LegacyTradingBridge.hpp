#pragma once
#ifndef _OPTIONX_BRIDGES_NAMED_PIPE_LEGACY_TRADING_BRIDGE_HPP_INCLUDED
#define _OPTIONX_BRIDGES_NAMED_PIPE_LEGACY_TRADING_BRIDGE_HPP_INCLUDED

/// \file LegacyTradingBridge.hpp
/// \brief Defines the legacy named-pipe trading bridge.

#include "utils.hpp"
#include "data.hpp"
#include "bridges/BaseBridge.hpp"
#include "LegacyTradingBridgeConfig.hpp"
#include "bridges/NamedPipe/detail/LegacyTradingProtocol.hpp"

#include <memory>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <SimpleNamedPipe/NamedPipeServer.hpp>
#endif

namespace optionx::bridges::named_pipe {

    /// \class LegacyTradingBridge
    /// \brief Adapter for the legacy named-pipe trading JSON protocol.
    ///
    /// The protocol was historically used with Intrade Bar, but the wire
    /// contract is broker-neutral: legacy clients send `contract` messages and
    /// receive `update_bet`, account, connection, and ping messages. Incoming
    /// contracts are published as `TradeSignal`; this bridge does not execute
    /// trades by itself.
    class LegacyTradingBridge final : public BaseBridge {
    private:
        struct RuntimeState {
            std::mutex mutex;
            std::shared_ptr<BaseAccountInfoData> account_info;
            bridge_status_callback_t status_callback;
            BaseBridge::trade_signal_callback_t trade_signal_callback;
            BaseBridge::signal_id_allocator_t signal_id_allocator;
            bool running = false;

#if defined(_WIN32)
            std::shared_ptr<SimpleNamedPipe::NamedPipeServer> server;
            std::set<int> client_ids;
#endif
        };

    public:
        /// \brief Constructs a bridge with empty runtime state.
        LegacyTradingBridge()
            : m_state(std::make_shared<RuntimeState>()) {}

        /// \brief Stops the bridge before destruction.
        ~LegacyTradingBridge() override {
            shutdown();
        }

        /// \brief Configures the bridge with legacy named-pipe settings.
        /// \param config Configuration object. Must be `LegacyTradingBridgeConfig`.
        /// \return `true` when configuration is valid and accepted.
        bool configure(std::unique_ptr<IBridgeConfig> config) override {
            if (!config) return false;

            const auto* typed =
                dynamic_cast<const LegacyTradingBridgeConfig*>(config.get());
            if (!typed) {
                config->dispatch_callbacks(false, "Invalid legacy trading bridge config type.");
                return false;
            }

            auto next_config = std::make_shared<LegacyTradingBridgeConfig>(*typed);
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

        /// \brief Broadcasts account snapshots to connected legacy clients.
        /// \param info Account update received from the trading platform.
        void update_account_info(const AccountInfoUpdate& info) override {
            if (!info.account_info) return;

            {
                std::lock_guard<std::mutex> lock(m_state->mutex);
                m_state->account_info = info.account_info;
            }

            switch (info.status) {
            case AccountUpdateStatus::CONNECTED:
            case AccountUpdateStatus::DISCONNECTED:
            case AccountUpdateStatus::FAILED_TO_CONNECT:
                broadcast(detail::format_connection_update(*info.account_info));
                break;
            case AccountUpdateStatus::BALANCE_UPDATED:
            case AccountUpdateStatus::ACCOUNT_TYPE_CHANGED:
            case AccountUpdateStatus::CURRENCY_CHANGED:
                broadcast(detail::format_balance_update(*info.account_info));
                break;
            default:
                break;
            }
        }

        /// \brief Sends a trade result update to connected legacy clients.
        /// \param request Original trade request produced from a signal.
        /// \param result Current trade result snapshot.
        void update_trade_result(
                const TradeRequest& request,
                const TradeResult& result) override {
            broadcast(detail::format_trade_result(request, result));
        }

        /// \brief Starts the named-pipe bridge.
        /// \details On Windows the server is started asynchronously and the
        ///          method returns after scheduling the server and ping task.
        ///          On non-Windows targets a failure status is reported.
        void run() override {
            auto config = get_config_or_throw();
            if (!get_signal_id_allocator()) {
                notify_status(
                    BridgeStatus::SERVER_START_FAILED,
                    {},
                    "Legacy trading bridge requires a signal ID allocator.");
                return;
            }

#if defined(_WIN32)
            {
                std::lock_guard<std::mutex> lock(m_state->mutex);
                if (m_state->running) return;
            }

            SimpleNamedPipe::ServerConfig server_config(
                config->named_pipe,
                config->buffer_size,
                config->pipe_timeout_ms);

            auto server = std::make_shared<SimpleNamedPipe::NamedPipeServer>(server_config);
            configure_server_callbacks(server);

            {
                std::lock_guard<std::mutex> lock(m_state->mutex);
                m_state->server = server;
                m_state->running = true;
                m_state->client_ids.clear();
            }

            try {
                server->start(true);
            } catch (const std::exception& ex) {
                clear_runtime_server();
                notify_status(BridgeStatus::SERVER_START_FAILED, {}, ex.what());
                return;
            }

            if (!m_task_manager.add_periodic_task(
                    "legacy-trading-bridge-ping",
                    config->ping_period_ms,
                    [state = m_state](std::shared_ptr<utils::Task> task) {
                        if (task->is_shutdown()) return;
                        broadcast(state, "{\"ping\":1}");
                    })) {
                server->stop();
                clear_runtime_server();
                notify_status(
                    BridgeStatus::SERVER_START_FAILED,
                    {},
                    "Failed to schedule legacy bridge ping task.");
                return;
            }

            m_task_manager.run();
#else
            (void)config;
            notify_status(
                BridgeStatus::SERVER_START_FAILED,
                {},
                "Legacy named-pipe bridge is available only on Windows.");
#endif
        }

        /// \brief Stops the bridge and clears connected clients.
        /// \details Drains the ping task manager and then stops the pipe server
        ///          if it was running.
        void shutdown() override {
#if defined(_WIN32)
            std::shared_ptr<SimpleNamedPipe::NamedPipeServer> server;
            {
                std::lock_guard<std::mutex> lock(m_state->mutex);
                server = m_state->server;
                m_state->server.reset();
                m_state->client_ids.clear();
                m_state->running = false;
            }

            m_task_manager.shutdown();
            if (server) {
                server->stop();
            }
#else
            m_task_manager.shutdown();
#endif
            notify_status(BridgeStatus::SERVER_STOPPED);
        }

    private:
        mutable std::mutex m_mutex;
        std::shared_ptr<RuntimeState> m_state;
        utils::TaskManager m_task_manager;
        std::shared_ptr<LegacyTradingBridgeConfig> m_config;

        std::shared_ptr<LegacyTradingBridgeConfig> get_config_or_throw() const {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_config) {
                throw std::invalid_argument("Legacy trading bridge is not configured.");
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

        void broadcast(const std::string& message) {
            broadcast(m_state, message);
        }

        static void broadcast(
                const std::shared_ptr<RuntimeState>& state,
                const std::string& message) {
#if defined(_WIN32)
            std::shared_ptr<SimpleNamedPipe::NamedPipeServer> server;
            std::vector<int> clients;
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                server = state->server;
                clients.assign(state->client_ids.begin(), state->client_ids.end());
            }

            if (!server) return;
            for (const int client_id : clients) {
                server->send_to(client_id, message);
            }
#else
            (void)state;
            (void)message;
#endif
        }

        static std::string connection_id(int client_id) {
            return std::to_string(client_id);
        }

#if defined(_WIN32)
        void configure_server_callbacks(
                const std::shared_ptr<SimpleNamedPipe::NamedPipeServer>& server) {
            server->on_connected = [state = m_state](int client_id) {
                std::shared_ptr<BaseAccountInfoData> account_info;
                std::shared_ptr<SimpleNamedPipe::NamedPipeServer> server;
                {
                    std::lock_guard<std::mutex> lock(state->mutex);
                    state->client_ids.insert(client_id);
                    account_info = state->account_info;
                    server = state->server;
                }

                if (account_info && server) {
                    server->send_to(client_id, detail::format_balance_update(*account_info));
                    server->send_to(client_id, detail::format_connection_update(*account_info));
                }

                notify_status(state, BridgeStatus::CLIENT_CONNECTED, connection_id(client_id));
            };

            server->on_start = [state = m_state](const SimpleNamedPipe::ServerConfig&) {
                notify_status(state, BridgeStatus::SERVER_STARTED);
            };

            server->on_disconnected =
                [state = m_state](int client_id, const std::error_code& ec) {
                    {
                        std::lock_guard<std::mutex> lock(state->mutex);
                        state->client_ids.erase(client_id);
                    }
                    notify_status(
                        state,
                        BridgeStatus::CLIENT_DISCONNECTED,
                        connection_id(client_id),
                        ec.message());
                };

            server->on_message =
                [this](int client_id, const std::string& message) {
                    handle_message(client_id, message);
                };

            server->on_error = [state = m_state](const std::error_code& ec) {
                notify_status(state, BridgeStatus::CONNECTION_ERROR, {}, ec.message());
            };
        }

        void clear_runtime_server() {
            std::lock_guard<std::mutex> lock(m_state->mutex);
            m_state->server.reset();
            m_state->client_ids.clear();
            m_state->running = false;
        }
#endif

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

        void handle_message(int client_id, const std::string& message) {
            try {
                const auto payload = nlohmann::json::parse(message);
                if (payload.contains("pong") || payload.contains("ping")) {
                    return;
                }
                if (!payload.contains("contract")) {
                    return;
                }

                auto config = get_config_or_throw();
                auto signal = detail::parse_contract(payload.at("contract"), config->min_payout);
                signal->bridge_id = config->bridge_id;

                auto signal_id_allocator = get_signal_id_allocator();
                if (!signal_id_allocator) {
                    notify_status(
                        BridgeStatus::CONNECTION_ERROR,
                        connection_id(client_id),
                        "Legacy trading bridge signal ID allocator is not configured.");
                    return;
                }
                signal->signal_id = signal_id_allocator();
                if (signal->signal_id == 0) {
                    notify_status(
                        BridgeStatus::CONNECTION_ERROR,
                        connection_id(client_id),
                        "Legacy trading bridge could not allocate signal ID.");
                    return;
                }

                trade_signal_callback_t callback;
                {
                    std::lock_guard<std::mutex> lock(m_state->mutex);
                    callback = m_state->trade_signal_callback;
                }
                if (callback) {
                    callback(std::move(signal));
                }
            } catch (const std::exception& ex) {
                LOGIT_PRINT_ERROR("Parsing legacy bridge message failed: ", ex.what());
                notify_status(
                    BridgeStatus::CONNECTION_ERROR,
                    connection_id(client_id),
                    ex.what());
            }
        }
    };

} // namespace optionx::bridges::named_pipe

#endif // _OPTIONX_BRIDGES_NAMED_PIPE_LEGACY_TRADING_BRIDGE_HPP_INCLUDED
