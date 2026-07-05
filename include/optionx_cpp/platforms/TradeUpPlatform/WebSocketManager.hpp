#pragma once
#ifndef OPTIONX_HEADER_PLATFORMS_TRADE_UP_PLATFORM_WEB_SOCKET_MANAGER_HPP_INCLUDED
#define OPTIONX_HEADER_PLATFORMS_TRADE_UP_PLATFORM_WEB_SOCKET_MANAGER_HPP_INCLUDED

/// \file WebSocketManager.hpp
/// \brief Maintains two WS connections (real/demo), auth & OPEN/CLOSE orchestration.

namespace optionx::platforms::tradeup {

    /// \class WebSocketManager
    class WebSocketManager final : public components::BaseComponent {
    public:
        explicit WebSocketManager(BaseTradingPlatform& platform)
            : BaseComponent(platform.event_bus())
        {
            subscribe<events::WebSocketAuthDataEvent>();
            subscribe<events::WebSocketRequestEvent>();
            subscribe<events::ConnectRequestEvent>();
            subscribe<events::DisconnectRequestEvent>();
            platform.register_component(this);

            // Real WS events
            m_real_ws.on_event([this](std::unique_ptr<kurlyk::WebSocketEventData> e) {
                switch (e->event_type) {
                    case kurlyk::WebSocketEventType::WS_OPEN: {
                        LOGIT_INFO(e->status_code, e->error_code);
                        m_is_real_error      = false;
                        m_is_real_connected  = true;
                        m_real_balance_ready = false;

                        // Send token
                        const std::string msg = std::string("{\"x-api-token\":\"") + m_token + "\"}";
                        const auto submit_result = e->sender->submit_message(msg, 0, [](const std::error_code& ec){
                            LOGIT_ERROR_IF(ec, ec);
                        });
                        if (!submit_result) {
                            LOGIT_PRINT_ERROR("REAL auth message rejected: ", submit_result.error_code.message());
                            m_is_real_connected = false;
                            m_real_balance_ready = false;
                            fail_open_if_needed("REAL auth message rejected: " + submit_result.error_code.message());
                            break;
                        }

                        // Start/refresh ping for REAL
                        const auto epoch = ++m_real_ping_epoch;
                        m_task_manager.add_periodic_task(
                            "ws_ping_real",
                            30000, // 30s
                            [this, epoch](std::shared_ptr<utils::Task> task){
                                if (task->is_shutdown()) return;
                                if (epoch != m_real_ping_epoch) return;
                                if (!m_is_real_connected) return;
                                static constexpr const char* kPing = R"({"id":"","param":"","operation":"PING"})";
                                log_ws_submit_result("REAL ping message", m_real_ws.submit_message(kPing, 0, [](const std::error_code& ec){
                                    LOGIT_ERROR_IF(ec, ec);
                                }));
                            }
                        );
                        break;
                    }
                    case kurlyk::WebSocketEventType::WS_MESSAGE: {
                        handle_real_message(e->message);
                        break;
                    }
                    case kurlyk::WebSocketEventType::WS_CLOSE: {
                        LOGIT_INFO(e->status_code, e->error_code);
                        m_is_real_error      = false;
                        m_is_real_connected  = false;
                        m_real_balance_ready = false;
                        fail_open_if_needed("Real socket closed.");
                        check_close_completion();
                        break;
                    }
                    case kurlyk::WebSocketEventType::WS_ERROR: {
                        if (m_is_real_error) return;
                        LOGIT_ERROR(e->status_code, e->error_code);
                        m_is_real_error      = true;
                        m_is_real_connected  = false;
                        m_real_balance_ready = false;
                        fail_open_if_needed("Real socket error.");
                        check_close_completion();
                        break;
                    }
                    default: break;
                }
                // Быстрая проверка успеха OPEN (если второй уже готов)
                maybe_complete_open();
            });

            // Demo WS events
            m_demo_ws.on_event([this](std::unique_ptr<kurlyk::WebSocketEventData> e) {
                switch (e->event_type) {
                    case kurlyk::WebSocketEventType::WS_OPEN: {
                        LOGIT_INFO(e->status_code, e->error_code);
                        m_is_demo_error      = false;
                        m_is_demo_connected  = true;
                        m_demo_balance_ready = false;

                        // Send token
                        const std::string msg = std::string("{\"x-api-token\":\"") + m_token + "\"}";
                        const auto submit_result = e->sender->submit_message(msg, 0, [](const std::error_code& ec){
                            LOGIT_ERROR_IF(ec, ec);
                        });
                        if (!submit_result) {
                            LOGIT_PRINT_ERROR("DEMO auth message rejected: ", submit_result.error_code.message());
                            m_is_demo_connected = false;
                            m_demo_balance_ready = false;
                            fail_open_if_needed("DEMO auth message rejected: " + submit_result.error_code.message());
                            break;
                        }

                        // Start/refresh ping for DEMO
                        const auto epoch = ++m_demo_ping_epoch;
                        m_task_manager.add_periodic_task(
                            "ws_ping_demo",
                            40000, // 40s
                            [this, epoch](std::shared_ptr<utils::Task> task){
                                if (task->is_shutdown()) return;
                                if (epoch != m_demo_ping_epoch) return;
                                if (!m_is_demo_connected) return;
                                static constexpr const char* kPing = R"({"id":"","param":"","operation":"PING"})";
                                log_ws_submit_result("DEMO ping message", m_demo_ws.submit_message(kPing, 0, [](const std::error_code& ec){
                                    LOGIT_ERROR_IF(ec, ec);
                                }));
                            }
                        );
                        break;
                    }
                    case kurlyk::WebSocketEventType::WS_MESSAGE: {
                        handle_demo_message(e->message);
                        break;
                    }
                    case kurlyk::WebSocketEventType::WS_CLOSE: {
                        LOGIT_INFO(e->status_code, e->error_code);
                        m_is_demo_error      = false;
                        m_is_demo_connected  = false;
                        m_demo_balance_ready = false;
                        fail_open_if_needed("Demo socket closed.");
                        check_close_completion();
                        break;
                    }
                    case kurlyk::WebSocketEventType::WS_ERROR: {
                        if (m_is_demo_error) return;
                        LOGIT_ERROR(e->status_code, e->error_code);
                        m_is_demo_error      = true;
                        m_is_demo_connected  = false;
                        m_demo_balance_ready = false;
                        fail_open_if_needed("Demo socket error.");
                        check_close_completion();
                        break;
                    }
                    default: break;
                }
                // Быстрая проверка успеха OPEN
                maybe_complete_open();
            });
        }

        virtual ~WebSocketManager() {
            shutdown();
        }

        void on_event(const utils::Event* const event) override {
            if (event->is<events::WebSocketAuthDataEvent>()) {
                handle_event(event->asRef<events::WebSocketAuthDataEvent>());
            } else
            if (event->is<events::WebSocketRequestEvent>()) {
                handle_event(event->asRef<events::WebSocketRequestEvent>());
            } else
            if (event->is<events::ConnectRequestEvent>()) {
                handle_event(event->asRef<events::ConnectRequestEvent>());
            } else
            if (event->is<events::DisconnectRequestEvent>()) {
                handle_event(event->asRef<events::DisconnectRequestEvent>());
            }
        }

        void process() override {
            m_task_manager.process();
        }

        void shutdown() override {
            // Останавливаем все локальные периодики/таймеры
            m_task_manager.shutdown();

            // Отключаем оба сокета
            if (m_is_connected.load(std::memory_order_acquire)) {
                m_is_connected.store(false, std::memory_order_release);
            }
            m_real_ws.disconnect();
            m_demo_ws.disconnect();

            // Сброс флагов
            m_is_real_connected  = false;
            m_is_demo_connected  = false;
            m_real_balance_ready = false;
            m_demo_balance_ready = false;
            m_open_cid.clear();
            m_close_cid.clear();
        }

        void set_max_send_queue_size(size_t max) {
            m_real_ws.set_max_send_queue_size(max);
            m_demo_ws.set_max_send_queue_size(max);
        }

    private:
        // Sockets
        kurlyk::WebSocketClient m_real_ws;
        kurlyk::WebSocketClient m_demo_ws;

        // Local scheduler
        utils::TaskManager m_real_tm;
		utils::TaskManager m_demo_tm;
		utils::TaskManager m_task_manager;

        // Auth/session
        std::string m_token;
        std::string m_cookie; // если надо хранить

        // Error/connection flags
        bool              m_is_real_error = false;
        bool              m_is_demo_error = false;
        std::atomic<bool> m_is_connected{false};
        std::atomic<bool> m_is_real_connected{false};
        std::atomic<bool> m_is_demo_connected{false};

        // Data readiness
        std::atomic<bool> m_real_balance_ready{false};
        std::atomic<bool> m_demo_balance_ready{false};

        // Epochs for timers
        std::atomic<uint64_t> m_open_epoch{0};
        std::atomic<uint64_t> m_close_epoch{0};
        std::atomic<uint64_t> m_real_ping_epoch{0};
        std::atomic<uint64_t> m_demo_ping_epoch{0};

        // Pending CIDs
        std::vector<utils::CorrelationId> m_open_cid;
        std::vector<utils::CorrelationId> m_close_cid;

        static void log_ws_submit_result(
                const char* operation,
                const kurlyk::SubmitResult& result) {
            if (!result) {
                LOGIT_PRINT_ERROR(operation, " rejected: ", result.error_code.message());
            }
        }

        // ---- Handlers ----
        void handle_real_message(const std::string& msg) {
            // Мини-детектор прихода балансов
            if (msg.find("\"evt\":\"balancesResults\"") != std::string::npos) {
                m_real_balance_ready = true;
            }
            // Тут можно парсить JSON и эмитить баланс/валюту
            // notify(events::AccountInfoUpdateEvent(..., Status::BALANCE_UPDATED));
        }

        void handle_demo_message(const std::string& msg) {
            if (msg.find("\"evt\":\"balancesResults\"") != std::string::npos) {
                m_demo_balance_ready = true;
            }
        }

        void handle_event(const events::WebSocketAuthDataEvent& event) {
            if (auto auth_data = std::dynamic_pointer_cast<AuthData>(event.auth_data)) {
                auto [ok, message] = auth_data->validate();
                if (!ok) {
                    notify(events::WebSocketAuthAppliedEvent(false, std::move(message)));
                    return;
                }

                if (m_real_ws.is_connected() || m_demo_ws.is_connected() || m_is_connected.load()) {
                    notify(events::WebSocketAuthAppliedEvent(false, "WS already connected or connecting."));
                    return;
                }

                // URLs
                m_token  = event.token;
                m_cookie = event.cookies;
                const std::string host_ws = "wss://" + utils::remove_http_prefix(auth_data->host);

                // Setup REAL
                m_real_ws.set_url(host_ws, "/trade-api-ws/api/ws/user");
                m_real_ws.set_user_agent(auth_data->user_agent);
                m_real_ws.set_accept_language(auth_data->accept_language);
                m_real_ws.set_accept_encoding(true, true, true, true);
                m_real_ws.set_cookie(m_cookie);
                m_real_ws.set_reconnect(true);
                m_real_ws.set_request_timeout(20);
                m_real_ws.set_proxy_server(auth_data->proxy_server);
                m_real_ws.set_proxy_auth(auth_data->proxy_auth);
                m_real_ws.set_proxy_type(auth_data->proxy_type);

                // Setup DEMO
                m_demo_ws.set_url(host_ws, "/trade-api-ws/demo/ws/user");
                m_demo_ws.set_user_agent(auth_data->user_agent);
                m_demo_ws.set_accept_language(auth_data->accept_language);
                m_demo_ws.set_accept_encoding(true, true, true, true);
                m_demo_ws.set_cookie(m_cookie);
                m_demo_ws.set_reconnect(true);
                m_demo_ws.set_request_timeout(20);
                m_demo_ws.set_proxy_server(auth_data->proxy_server);
                m_demo_ws.set_proxy_auth(auth_data->proxy_auth);
                m_demo_ws.set_proxy_type(auth_data->proxy_type);

                notify(events::WebSocketAuthAppliedEvent(true, {}));
            } else {
                notify(events::WebSocketAuthAppliedEvent(false, "Invalid auth data type."));
            }
        }

        void handle_event(const events::WebSocketRequestEvent& event) {
            LOGIT_TRACE0();

            if (event.action == events::WebSocketAction::OPEN) {
                // Консолидация всех OPEN-запросов
                m_open_cid.push_back(event.correlation_id);

                // Если уже в процессе/подключены — просто дождёмся условий
                if (!m_is_connected.exchange(true)) {
                    // fresh OPEN: сбрасываем флаги готовности балансов
                    m_real_balance_ready = false;
                    m_demo_balance_ready = false;
                    m_is_real_connected  = false;
                    m_is_demo_connected  = false;

                    // Таймаут ожидания приходов balancesResults
                    const auto epoch = ++m_open_epoch;
                    m_task_manager.add_single_task(
                        "ws_open_timeout",
                        15000, // 15s таймаут
                        [this](std::shared_ptr<utils::Task> task){
                            if (task->is_shutdown()) return;
                            if (m_open_cid.empty()) return;    // нечего завершать
                            const bool ok = (m_real_balance_ready.load() && m_demo_balance_ready.load());
                            complete_open(ok, ok ? "" : "Timeout waiting for balances.");
                        }
                    );

                    // Запускаем подключение
                    m_real_ws.connect();
                    m_demo_ws.connect();
					
					m_real_tm.shutdown();
					m_demo_tm.shutdown();
					
					m_real_tm.add_periodic_task(
						"ws_ping_real",
						30000, // 30s
						[this](std::shared_ptr<utils::Task> task){
							if (task->is_shutdown()) return;
							if (!m_is_real_connected) return;
							static constexpr const std::string ping_str("{\"id\":\"\",\"param\":\"\",\"operation\":\"PING\"}");
							log_ws_submit_result("REAL scheduled ping message", m_real_ws.submit_message(ping_str, 0, [](const std::error_code& ec){
								LOGIT_ERROR_IF(ec, ec);
							}));
						}
					);
					m_demo_tm.add_periodic_task(
						"ws_ping_demo",
						40000, // 40s
						[this](std::shared_ptr<utils::Task> task){
							if (task->is_shutdown()) return;
							if (!m_is_demo_connected) return;
							static constexpr const std::string ping_str("{\"id\":\"\",\"param\":\"\",\"operation\":\"PING\"}");
							log_ws_submit_result("DEMO scheduled ping message", m_demo_ws.submit_message(ping_str, 0, [](const std::error_code& ec){
								LOGIT_ERROR_IF(ec, ec);
							}));
						}
					);
                }

                // быстрый путь — если уже всё пришло
                maybe_complete_open();
                return;
            }

            if (event.action == events::WebSocketAction::CLOSE) {
                m_close_cid.push_back(event.correlation_id);

                if (!m_is_connected.exchange(false)) {
                    // уже закрыты — можно сразу репортить успех закрытия
                    complete_close(true, "");
                    return;
                }

                // Таймаут ожидания фактического закрытия обоих
                const auto epoch = ++m_close_epoch;
                m_task_manager.add_single_task(
                    "ws_close_timeout",
                    5000, // 5s
                    [this, epoch](std::shared_ptr<utils::Task> task){
                        if (task->is_shutdown()) return;
                        if (epoch != m_close_epoch) return;
                        if (m_close_cid.empty()) return;
                        const bool both_down = (!m_is_real_connected.load() && !m_is_demo_connected.load());
                        complete_close(both_down, both_down ? "" : "Timeout waiting for CLOSE.");
                    }
                );

                m_real_ws.disconnect();
                m_demo_ws.disconnect();
                return;
            }
        }

        void handle_event(const events::ConnectRequestEvent&) {
            LOGIT_TRACE0();
            if (m_is_connected.exchange(false)) {
                m_real_ws.disconnect();
                m_demo_ws.disconnect();
            }
        }

        void handle_event(const events::DisconnectRequestEvent&) {
            LOGIT_TRACE0();
            if (m_is_connected.exchange(false)) {
                m_real_ws.disconnect();
                m_demo_ws.disconnect();
            }
        }

        // ---- OPEN/CLOSE helpers ----
        void maybe_complete_open() {
            if (m_open_cid.empty()) return;
            if (m_real_balance_ready.load() && m_demo_balance_ready.load()) {
                complete_open(true, "");
            }
        }

        void fail_open_if_needed(const std::string& error) {
            if (m_open_cid.empty()) return;
            complete_open(false, error);
        }

        void complete_open(bool success, std::string error) {
            // Гасим текущий «эпоху OPEN», чтобы таймер не сработал повторно
            ++m_open_epoch;

            // Рассылаем результат всем ожидающим cid
            for (const auto& cid : m_open_cid) {
                notify_async(events::WebSocketResultEvent(
                    cid, events::WebSocketAction::OPEN, success, error));
            }
            m_open_cid.clear();

            // Если успех — считаем, что соединились
            if (success) {
                // ничего спец. не делаем — состояние управляет флагами сокетов
            }
        }

        void check_close_completion() {
            if (m_close_cid.empty()) return;
            if (!m_is_real_connected.load() && !m_is_demo_connected.load()) {
                complete_close(true, "");
            }
        }

        void complete_close(bool success, std::string error) {
            ++m_close_epoch;
            for (const auto& cid : m_close_cid) {
                notify_async(events::WebSocketResultEvent(
                    cid, events::WebSocketAction::CLOSE, success, error));
            }
            m_close_cid.clear();
        }
    };

} // namespace optionx::platforms::tradeup

#endif // OPTIONX_HEADER_PLATFORMS_TRADE_UP_PLATFORM_WEB_SOCKET_MANAGER_HPP_INCLUDED
