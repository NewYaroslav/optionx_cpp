#pragma once
#ifndef _OPTIONX_BRIDGES_INTRADE_BAR_LEGACY_BRIDGE_HPP_INCLUDED
#define _OPTIONX_BRIDGES_INTRADE_BAR_LEGACY_BRIDGE_HPP_INCLUDED

/// \file IntradeBarLegacyBridge.hpp
/// \brief Defines the Intrade Bar legacy named-pipe bridge.

#include "utils.hpp"
#include "data.hpp"
#include "bridges/BaseBridge.hpp"
#include "bridges/IntradeBarLegacyBridge/BridgeConfig.hpp"

#include <algorithm>
#include <cctype>
#include <limits>
#include <memory>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <SimpleNamedPipe/NamedPipeServer.hpp>
#endif

namespace optionx::bridges {

    /// \class IntradeBarLegacyBridge
    /// \brief Adapter for the legacy Intrade Bar named-pipe JSON protocol.
    class IntradeBarLegacyBridge final : public BaseBridge {
    private:
        struct RuntimeState {
            std::mutex mutex;
            std::shared_ptr<BaseAccountInfoData> account_info;
            bridge_status_callback_t status_callback;
            BaseBridge::place_trade_callback_t place_trade_callback;
            trade_result_callback_t trade_result_callback;
            bool running = false;

#if defined(_WIN32)
            std::shared_ptr<SimpleNamedPipe::NamedPipeServer> server;
            std::set<int> client_ids;
#endif
        };

    public:
        /// \brief Constructs a bridge with empty runtime state.
        IntradeBarLegacyBridge()
            : m_state(std::make_shared<RuntimeState>()) {}

        /// \brief Stops the bridge before destruction.
        ~IntradeBarLegacyBridge() override {
            shutdown();
        }

        /// \brief Configures the bridge with Intrade Bar legacy settings.
        bool configure(std::unique_ptr<IBridgeConfig> config) override {
            if (!config) return false;

            const auto* typed =
                dynamic_cast<const intrade_bar_legacy::BridgeConfig*>(config.get());
            if (!typed) {
                config->dispatch_callbacks(false, "Invalid Intrade Bar legacy bridge config type.");
                return false;
            }

            auto next_config = std::make_shared<intrade_bar_legacy::BridgeConfig>(*typed);
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

        /// \brief Returns the trade request callback slot.
        place_trade_callback_t& on_place_trade() override {
            return m_state->place_trade_callback;
        }

        /// \brief Returns the trade result callback slot.
        trade_result_callback_t& on_trade_result() override {
            return m_state->trade_result_callback;
        }

        /// \brief Broadcasts account snapshots to connected legacy clients.
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
                broadcast(format_connection_update(*info.account_info));
                break;
            case AccountUpdateStatus::BALANCE_UPDATED:
            case AccountUpdateStatus::ACCOUNT_TYPE_CHANGED:
            case AccountUpdateStatus::CURRENCY_CHANGED:
                broadcast(format_balance_update(*info.account_info));
                break;
            default:
                break;
            }
        }

        /// \brief Starts the named-pipe bridge.
        void run() override {
            auto config = get_config_or_throw();

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
                    "intrade-bar-legacy-ping",
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
                "Intrade Bar legacy named-pipe bridge is available only on Windows.");
#endif
        }

        /// \brief Stops the bridge and clears connected clients.
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

        /// \brief Parses a legacy `contract` object into a TradeRequest.
        static std::unique_ptr<TradeRequest> parse_contract(
                const nlohmann::json& contract,
                double min_payout) {
            auto request = std::make_unique<TradeRequest>();
            request->symbol = parse_symbol(contract.at("s").get<std::string>());
            parse_note(contract.value("note", std::string()), *request);
            request->amount = contract.at("a").get<double>();
            request->order_type = parse_order_type(contract.at("dir").get<std::string>());
            parse_expiry_or_duration(contract, *request);
            request->min_payout = min_payout;
            return request;
        }

        /// \brief Formats a trade result for the legacy `update_bet` message.
        static std::string format_trade_result(
                const TradeRequest& request,
                const TradeResult& result) {
            nlohmann::json update;
            update["s"] = request.symbol;
            update["note"] = compose_note(request);
            update["aid"] = result.trade_id;
            update["id"] = result.option_id;
            update["op"] = result.open_price;
            update["cp"] = result.close_price;
            update["dir"] = format_order_type(request.order_type);
            update["status"] = format_trade_state(result.trade_state);
            update["a"] = result.amount;
            update["profit"] = result.profit;
            update["payout"] = result.payout;
            update["dur"] = request.duration;
            update["ot"] = time_shield::ms_to_fsec(result.open_date);
            update["st"] = time_shield::ms_to_fsec(result.send_date);
            update["ct"] = time_shield::ms_to_fsec(result.close_date);

            nlohmann::json message;
            message["update_bet"] = std::move(update);
            return message.dump();
        }

        /// \brief Formats a legacy balance update message.
        static std::string format_balance_update(const BaseAccountInfoData& info) {
            nlohmann::json message;
            message["b"] = info.get_info<double>(AccountInfoType::BALANCE);
            message["rub"] =
                info.get_info<CurrencyType>(AccountInfoType::CURRENCY) == CurrencyType::RUB ? 1 : 0;
            message["demo"] =
                info.get_info<AccountType>(AccountInfoType::ACCOUNT_TYPE) == AccountType::DEMO ? 1 : 0;
            return message.dump();
        }

        /// \brief Formats a legacy connection status update message.
        static std::string format_connection_update(const BaseAccountInfoData& info) {
            nlohmann::json message;
            message["conn"] =
                info.get_info<bool>(AccountInfoType::CONNECTION_STATUS) ? 1 : 0;
            message["aid"] = info.get_info<std::int64_t>(AccountInfoType::USER_ID);
            return message.dump();
        }

    private:
        mutable std::mutex m_mutex;
        std::shared_ptr<RuntimeState> m_state;
        utils::TaskManager m_task_manager;
        std::shared_ptr<intrade_bar_legacy::BridgeConfig> m_config;

        std::shared_ptr<intrade_bar_legacy::BridgeConfig> get_config_or_throw() const {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_config) {
                throw std::invalid_argument("Intrade Bar legacy bridge is not configured.");
            }
            return m_config;
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

        static void send_trade_result(
                const std::shared_ptr<RuntimeState>& state,
                const std::unique_ptr<TradeRequest>& request,
                const std::unique_ptr<TradeResult>& result) {
            if (!request || !result) return;
            broadcast(state, format_trade_result(*request, *result));
        }

        static void dispatch_trade_result(
                const std::shared_ptr<RuntimeState>& state,
                std::unique_ptr<TradeRequest> request,
                std::unique_ptr<TradeResult> result) {
            send_trade_result(state, request, result);

            trade_result_callback_t callback;
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                callback = state->trade_result_callback;
            }
            if (callback) {
                callback(std::move(request), std::move(result));
            }
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
                    server->send_to(client_id, format_balance_update(*account_info));
                    server->send_to(client_id, format_connection_update(*account_info));
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
                auto request = parse_contract(payload.at("contract"), config->min_payout);
                request->add_callback([state = m_state](
                        std::unique_ptr<TradeRequest> req,
                        std::unique_ptr<TradeResult> res) {
                    dispatch_trade_result(state, std::move(req), std::move(res));
                });

                place_trade_callback_t callback;
                {
                    std::lock_guard<std::mutex> lock(m_state->mutex);
                    callback = m_state->place_trade_callback;
                }
                if (callback) {
                    callback(std::move(request));
                }
            } catch (const std::exception& ex) {
                LOGIT_PRINT_ERROR("Parsing legacy bridge message failed: ", ex.what());
                notify_status(
                    BridgeStatus::CONNECTION_ERROR,
                    connection_id(client_id),
                    ex.what());
            }
        }

        static std::string parse_symbol(const std::string& symbol) {
            static const std::vector<std::string> symbols = {
                "EURUSD", "USDJPY", "USDCHF", "USDCAD", "EURJPY", "AUDUSD",
                "NZDUSD", "EURGBP", "EURCHF", "AUDJPY", "GBPJPY", "EURCAD",
                "AUDCAD", "CADJPY", "NZDJPY", "AUDNZD", "GBPAUD", "EURAUD",
                "GBPCHF", "AUDCHF", "GBPNZD", "BTCUSDT"
            };

            std::string normalized;
            normalized.reserve(symbol.size());
            for (const unsigned char ch : symbol) {
                if (std::isalnum(ch) != 0) {
                    normalized.push_back(static_cast<char>(std::toupper(ch)));
                }
            }

            if (normalized == "BTCUSD" || normalized == "BTCUSDT") {
                return "BTCUSDT";
            }

            const auto it = std::find(symbols.begin(), symbols.end(), normalized);
            if (it != symbols.end()) {
                return *it;
            }

            throw std::invalid_argument("Invalid symbol in legacy trade request.");
        }

        static void parse_note(const std::string& note, TradeRequest& request) {
            const auto pos = note.find('&');
            if (pos == std::string::npos) {
                request.user_data = note;
                return;
            }
            request.signal_name = note.substr(0, pos);
            request.user_data = note.substr(pos + 1);
        }

        static OrderType parse_order_type(const std::string& direction) {
            OrderType order_type = OrderType::UNKNOWN;
            if (!to_enum(direction, order_type) || order_type == OrderType::UNKNOWN) {
                throw std::invalid_argument("Invalid order direction in legacy trade request.");
            }
            return order_type;
        }

        static std::uint32_t parse_duration_value(std::int64_t value) {
            if (value < 0 ||
                value > static_cast<std::int64_t>((std::numeric_limits<std::uint32_t>::max)())) {
                throw std::invalid_argument("Invalid duration in legacy trade request.");
            }
            return static_cast<std::uint32_t>(value);
        }

        static void parse_expiry_or_duration(
                const nlohmann::json& contract,
                TradeRequest& request) {
            if (contract.contains("exp")) {
                if (!contract.at("exp").is_number()) {
                    throw std::invalid_argument("Invalid expiry time in legacy trade request.");
                }

                const std::int64_t expiry_time = contract.at("exp").get<std::int64_t>();
                request.option_type = OptionType::CLASSIC;
                if (expiry_time < time_shield::SEC_PER_DAY) {
                    request.duration = parse_duration_value(expiry_time);
                    request.expiry_time = 0;
                } else {
                    request.duration = 0;
                    request.expiry_time = expiry_time;
                }
                return;
            }

            if (contract.contains("dur")) {
                if (!contract.at("dur").is_number()) {
                    throw std::invalid_argument("Invalid duration in legacy trade request.");
                }

                request.duration = parse_duration_value(contract.at("dur").get<std::int64_t>());
                request.option_type = OptionType::SPRINT;
                return;
            }

            throw std::invalid_argument("Missing expiry or duration in legacy trade request.");
        }

        static std::string compose_note(const TradeRequest& request) {
            if (request.signal_name.empty()) {
                return request.user_data;
            }
            return request.signal_name + "&" + request.user_data;
        }

        static std::string format_order_type(OrderType order_type) {
            if (order_type == OrderType::BUY) return "buy";
            if (order_type == OrderType::SELL) return "sell";
            return "none";
        }

        static std::string format_trade_state(TradeState state) {
            switch (state) {
            case TradeState::UNKNOWN:
            case TradeState::WAITING_OPEN:
                return "unknown";
            case TradeState::CHECK_ERROR:
                return "error";
            case TradeState::OPEN_ERROR:
            case TradeState::CANCELED_TRADE:
                return "open_error";
            case TradeState::OPEN_SUCCESS:
            case TradeState::IN_PROGRESS:
            case TradeState::WAITING_CLOSE:
                return "wait";
            case TradeState::WIN:
                return "win";
            case TradeState::LOSS:
                return "loss";
            case TradeState::STANDOFF:
            case TradeState::REFUND:
                return "standoff";
            default:
                break;
            }
            return "unknown";
        }
    };

} // namespace optionx::bridges

#endif // _OPTIONX_BRIDGES_INTRADE_BAR_LEGACY_BRIDGE_HPP_INCLUDED
