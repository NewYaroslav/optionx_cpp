#pragma once
#ifndef _OPTIONX_PLATFORMS_TRADEUP_AUTH_MANAGER_HPP_INCLUDED
#define _OPTIONX_PLATFORMS_TRADEUP_AUTH_MANAGER_HPP_INCLUDED

/// \file AuthManager.hpp
/// \brief Handles authentication for the TradeUp platform.

namespace optionx::platforms::tradeup {

    /// \class AuthManager
    class AuthManager final : public modules::BaseModule {
    public:
        AuthManager(BaseTradingPlatform& platform,
                    RequestManager& request_manager,
                    std::shared_ptr<BaseAccountInfoData> account_info)
            : BaseModule(platform.event_bus()),
              m_request_manager(request_manager),
              m_account_info(std::move(account_info)) {
            subscribe<events::AuthDataEvent>();
            subscribe<events::ConnectRequestEvent>();
            subscribe<events::DisconnectRequestEvent>();
            platform.register_module(this);
        }

        void on_event(const utils::Event* const event) override;
        void process() override {}
        void shutdown() override {}

    private:
        RequestManager& m_request_manager;
        std::shared_ptr<BaseAccountInfoData> m_account_info;
        std::unique_ptr<AuthData> m_temp_auth_data;
        std::shared_ptr<AuthData> m_auth_data;

        void handle_event(const events::AuthDataEvent& event);
        void handle_event(const events::ConnectRequestEvent& event);
        void handle_event(const events::DisconnectRequestEvent& event);

        std::shared_ptr<AccountInfoData> get_account_info();
        
        /// \brief Handles authentication failure.
        void handle_auth_failure(const std::string& reason, connection_callback_t callback);

        // Шаги процесса
        void validate_email_pass(connection_callback_t callback);
        void do_login(connection_callback_t cb);
        void do_refresh_session(connection_callback_t cb);
        void verify_login(std::string token, std::string cookies, connection_callback_t cb);
        void close_ws_and_wait(std::string token, std::string cookies, connection_callback_t cb);
        void apply_ws_auth_and_wait(std::string token, std::string cookies, connection_callback_t cb);
    };
    
    // ------------------------------------------------------------------------

    inline void AuthManager::on_event(const utils::Event* const event) {
        if (event->is<events::AuthDataEvent>()) {
            handle_event(event->asRef<events::AuthDataEvent>());
        } else 
        if (event->is<events::ConnectRequestEvent>()) {
            handle_event(event->asRef<events::ConnectRequestEvent>());
        } else 
        if (event->is<events::DisconnectRequestEvent>()) {
            handle_event(event->asRef<events::DisconnectRequestEvent>());
        }
    }

    inline void AuthManager::handle_event(const events::AuthDataEvent& event) {
        if (auto auth_data = std::dynamic_pointer_cast<AuthData>(event.auth_data)) {
            LOGIT_TRACE0();
            auto [success, message] = auth_data->validate();
            if (success) {
                m_temp_auth_data = std::make_unique<AuthData>(*auth_data.get());
            } else {
                m_temp_auth_data.reset();
            }
            auth_data->dispatch_callbacks(true, message);
        }
    }

    inline void AuthManager::handle_event(const events::ConnectRequestEvent& event) {
        LOGIT_TRACE0();
        auto callback = event.callback; // тип connection_callback_t (по значению — отлично)
        m_task_manager.add_single_task(
            "event(ConnectRequestEvent)-single-1",
            [this, callback = std::move(callback)](std::shared_ptr<utils::Task> task) {
                LOGIT_TRACE0();
                m_request_manager.cancel_requests();

                if (task->is_shutdown()) {
                    using Status = events::AccountInfoUpdateEvent::Status;
                    auto account_info = get_account_info();
                    if (account_info->connect) {
                        account_info->connect = false;
                        notify(events::AccountInfoUpdateEvent(
                            account_info, Status::DISCONNECTED, "Shutdown."));
                    }
                    return;
                }

                m_task_manager.add_single_task(
                    "event(ConnectRequestEvent)-single-2",
                    [this, callback](std::shared_ptr<utils::Task> task) {
                        using Status = events::AccountInfoUpdateEvent::Status;

                        if (task->is_shutdown()) {
                            auto account_info = get_account_info();
                            if (account_info->connect) {
                                account_info->connect = false;
                                notify(events::AccountInfoUpdateEvent(
                                    account_info, Status::DISCONNECTED, "Shutdown."));
                            }
                            return;
                        }

                        auto account_info = get_account_info();

                        if (account_info->connect) {
                            account_info->connect = false;
                            notify(events::AccountInfoUpdateEvent(
                                account_info, Status::DISCONNECTED, "Reconnecting."));
                        }

                        // Отменяем все запросы
                        m_request_manager.cancel_requests();

                        if (!m_temp_auth_data) {
                            const std::string error_message("Authentication data is missing.");
                            LOGIT_ERROR(error_message);
                            handle_auth_failure(error_message, std::move(callback));
                            return;
                        }

                        m_auth_data = std::make_shared<AuthData>(*m_temp_auth_data.get());
                        account_info->account_type = m_auth_data->account_type;
                        notify(events::AccountInfoUpdateEvent(account_info, Status::CONNECTING));
                        
                        validate_email_pass(std::move(callback));
                    }
                );
            }
        );
    }

    inline void AuthManager::handle_event(const events::DisconnectRequestEvent& event) {
        auto info = get_account_info();
        if (info && info->connect) {
            info->connect = false;
            notify(events::AccountInfoUpdateEvent(
                info, events::AccountInfoUpdateEvent::Status::DISCONNECTED));
        }
        if (event.callback) {
            event.callback({true, std::string(), m_auth_data ? m_auth_data->clone_unique() : nullptr});
        }
    }
    
    inline void AuthManager::validate_email_pass(connection_callback_t callback) {
        // Validate email and password
        if (m_auth_data->email.empty() || m_auth_data->password.empty()) {
            LOGIT_ERROR_IF(m_auth_data->email.empty(), "Validation failed: Email is missing.");
            LOGIT_ERROR_IF(m_auth_data->password.empty(), "Validation failed: Password is missing.");
            const std::string error_text("Login failed: Please provide both email and password.");
            handle_auth_failure(error_text, std::move(callback));
            return;
        }

        // Попытка взять сессию из хранилища
        auto session = storage::ServiceSessionDB::get_instance().get_session_value(
            to_str(PlatformType::TRADEUP), m_auth_data->email);

        if (!session) {
            LOGIT_PRINT_TRACE("Session not found for email: ", m_auth_data->email);
            do_login(std::move(callback));
            return;
        }

        LOGIT_PRINT_TRACE("Session retrieved for email: ", m_auth_data->email);
        std::string token = *session;

        if (m_request_manager.initialize_session(m_auth_data, token)) {
            // Проверяем живость сессии
            m_request_manager.request_login_success(
                [this, token, callback = std::move(callback)]
                (bool success, std::string error_message) mutable {
                    LOGIT_TRACE(success, error_message);
                    if (!success) {
                        do_login(std::move(callback));
                        return;
                    }
                    do_refresh_session(std::move(callback));
                }
            );
        } else {
            do_login(std::move(callback));
        }
    }

    inline void AuthManager::do_login(connection_callback_t cb) {
        m_request_manager.request_login(
            m_auth_data,
            [this, cb = std::move(cb)]
            (bool success,
             std::string error_message,   // FIX: пропущенная запятая была
             std::string user_id,
             std::string token,
             std::string affs_id,
             std::string cookies)
            {
                if (!success) {
                    return cb({false, std::move(error_message), m_auth_data->clone_unique()});
                }
                verify_login(std::move(token), std::move(cookies), std::move(cb));
            }
        );
    }
    
    inline void AuthManager::do_refresh_session(connection_callback_t cb) {
        // FIX: request_session_extension не принимает m_auth_data в твоём модуле — убрал
        m_request_manager.request_session_extension(
            [this, cb = std::move(cb)]
            (bool success,
             std::string error_message,
             std::string ret,
             std::string message,
             std::string token,
             std::string cookies,
             int64_t /*expire*/)
            {
                if (!success) {
                    return cb({false, std::move(error_message), m_auth_data->clone_unique()});
                }
                // Тут token = текущий (m_token), ret = возможно новый; при необходимости можно обновить m_token по ret.
                verify_login(std::move(token), std::move(cookies), std::move(cb));
            }
        );
    }

    inline void AuthManager::verify_login(std::string token, std::string cookies, connection_callback_t cb) {
        m_request_manager.request_login_success(
            [this, token = std::move(token), cookies = std::move(cookies), cb = std::move(cb)]
            (bool success, std::string error_message) mutable {
                if (!success) {
                    return cb({false, std::move(error_message), m_auth_data->clone_unique()});
                }
                close_ws_and_wait(std::move(token), std::move(cookies), std::move(cb));
            }
        );
    }

    inline void AuthManager::close_ws_and_wait(std::string token, std::string cookies, connection_callback_t cb) {
        auto cid = utils::make_cid();

        await_once<events::WebSocketResultEvent>(
            [cid](const auto& ev){ return ev.correlation_id == cid; },
            [this, token = std::move(token), cookies = std::move(cookies), cb = std::move(cb)](const auto&) mutable {
                apply_ws_auth_and_wait(std::move(token), std::move(cookies), std::move(cb));
            }
        );

        notify(events::WebSocketRequestEvent(cid, events::WebSocketAction::CLOSE));
    }

    inline void AuthManager::apply_ws_auth_and_wait(std::string token, std::string cookies, connection_callback_t cb) {
        using Status = events::AccountInfoUpdateEvent::Status;

        // 1) Ждём применение WS-авторизации
        await_once<events::WebSocketAuthAppliedEvent>(
            [](const auto&){ return true; },
            [this, cb = std::move(cb), token](const events::WebSocketAuthAppliedEvent& ev) mutable {
                if (!ev.success) {
                    LOGIT_PRINT_ERROR("Auth failed: ", ev.error_message);
                    return cb({false, ev.error_message, m_auth_data->clone_unique()});
                }

                // 2) После успешной авторизации — инициируем OPEN и ждём его результат
                const auto cid_open = utils::make_cid();

                await_once<events::WebSocketResultEvent>(
                    [cid_open, token = std::move(token)](const auto& ev){ return ev.correlation_id == cid_open; },
                    [this, cb = std::move(cb)](const events::WebSocketResultEvent& ev_open) mutable {
                        if (!ev_open.success) {
                            // Точно знаем, что не подключились
                            std::string err = ev_open.error_message.empty()
                                              ? std::string("WebSocket OPEN failed.")
                                              : ev_open.error_message;
                            return handle_auth_failure(err, std::move(cb));
                        }

                        // Сохраняем токен в БД — ок делать до OPEN
                        storage::ServiceSessionDB::get_instance().set_session_value(
                        to_str(PlatformType::TRADEUP), m_auth_data->email, token);

                        LOGIT_TRACE("Authentication successful. Connection established.");
                        cb({true, "", m_auth_data->clone_unique()});

                        auto account_info = get_account_info();
                        account_info->account_type = m_auth_data->account_type;
                        if (!account_info->connect) {
                            account_info->connect  = true;
                            notify(events::AccountInfoUpdateEvent(account_info, Status::CONNECTED));
                        }
                    }
                );

                notify(events::WebSocketRequestEvent(cid_open, events::WebSocketAction::OPEN));
            }
        );

        // Отправляем данные для авторизации по WS
        notify(events::WebSocketAuthDataEvent(m_auth_data, std::move(token), std::move(cookies)));
    }

    inline std::shared_ptr<AccountInfoData> AuthManager::get_account_info() {
        if (auto account_info = std::dynamic_pointer_cast<AccountInfoData>(m_account_info)) {
            return account_info;
        }
        LOGIT_FATAL("Failed to cast IAccountInfoData to AccountInfoData");
        throw std::runtime_error("Invalid account information type");
    }
    
    inline void AuthManager::handle_auth_failure(const std::string& reason, connection_callback_t callback) {
        using Status = events::AccountInfoUpdateEvent::Status;
        LOGIT_PRINT_ERROR("Authentication failed: ", reason);
        callback({false, reason, m_auth_data ? m_auth_data->clone_unique() : nullptr});
        notify(events::AccountInfoUpdateEvent(get_account_info(), Status::FAILED_TO_CONNECT, reason));
    }

} // namespace optionx::platforms::tradeup

#endif // _OPTIONX_PLATFORMS_TRADEUP_AUTH_MANAGER_HPP_INCLUDED

