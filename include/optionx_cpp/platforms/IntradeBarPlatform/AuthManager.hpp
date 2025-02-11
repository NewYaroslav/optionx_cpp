#pragma once
#ifndef _OPTIONX_PLATFORMS_INTRADERBAR_AUTH_MANAGER_HPP_INCLUDED
#define _OPTIONX_PLATFORMS_INTRADERBAR_AUTH_MANAGER_HPP_INCLUDED

/// \file AuthManager.hpp
/// \brief Handles authentication management for the Intrade Bar platform.

namespace optionx::platforms::intrade_bar {

    /// \class AuthManager
    /// \brief Manages the authentication process for the Intrade Bar platform.
    /// \details Handles user authentication, session management, and account information updates.
    class AuthManager final : public modules::BaseModule {
    public:

        /// \brief Constructs the authentication manager.
        /// \param platform Reference to the trading platform.
        /// \param request_manager Reference to the request manager for making HTTP requests.
        /// \param account_info Shared pointer to the account information data.
        explicit AuthManager(
                BaseTradingPlatform& platform,
                RequestManager& request_manager,
                std::shared_ptr<BaseAccountInfoData> account_info)
                : BaseModule(platform.event_hub()), m_request_manager(request_manager),
                  m_account_info(std::move(account_info))  {
            subscribe<events::AuthDataEvent>(this);
            subscribe<events::ConnectRequestEvent>(this);
            subscribe<events::RestartAuthEvent>(this);
            subscribe<events::DisconnectRequestEvent>(this);
            platform.register_module(this);
        }

        /// \brief Default destructor.
        virtual ~AuthManager() = default;

        /// \brief Processes incoming events and dispatches them to the appropriate handlers.
        /// \param event The received event.
        void on_event(const utils::Event* const event) override;

        /// \brief Processes internal authentication tasks.
        void process() override;

        /// \brief Shuts down authentication-related operations.
        void shutdown() override;

    private:
        RequestManager&    m_request_manager; ///< Reference to the request manager.
        utils::TaskManager m_task_manager;    ///< Task manager for handling async tasks.
        std::shared_ptr<BaseAccountInfoData> m_account_info; ///< Shared pointer to account information.
        std::unique_ptr<AuthData> m_new_auth_data;        ///< Temporary authentication data storage.
        std::shared_ptr<AuthData> m_auth_data; ///< Currently active authentication data.

        /// \brief Stores authentication credentials after a successful login.
        /// \param user_id The user ID obtained after login.
        /// \param user_hash The user authentication hash.
        void set_auth_credentials(
                const std::string& user_id,
                const std::string& user_hash);

        /// \brief Initiates email/password validation for authentication.
        /// \param connect_callback The callback function to notify the result.
        void validate_email_pass(connection_callback_t connect_callback);

        /// \brief Initiates user token validation for authentication.
        /// \param connect_callback The callback function to notify the result.
        void validate_user_token(connection_callback_t connect_callback);

        /// \brief Completes the authentication process after login.
        /// \param connect_callback The callback function to notify the result.
        void finalize_authentication(connection_callback_t connect_callback);

        /// \brief Starts the authentication process.
        /// \param callback The callback function to notify the result.
        void start_authentication(connection_callback_t callback);

        /// \brief Handles authentication failure.
        /// \param reason The reason for the failure.
        /// \param callback The callback function to notify the result.
        void handle_auth_failure(
            const std::string& reason,
            connection_callback_t callback);

        /// \brief Executes the authentication process, handling necessary steps.
        /// \param callback The callback function to notify the result.
        void execute_authentication_flow(connection_callback_t callback);

        /// \brief Handles an account type switch (e.g., Demo to Real).
        /// \param account_type The new account type.
        /// \param currency The currency type.
        /// \param callback The callback function to notify the result.
        void handle_account_type_switch(
            AccountType account_type,
            CurrencyType currency,
            connection_callback_t callback);

        /// \brief Handles currency switch during authentication.
        /// \param currency The new currency type.
        /// \param callback The callback function to notify the result.
        void handle_currency_switch(
            CurrencyType currency,
            connection_callback_t callback);

        /// \brief Performs a multi-step authentication flow, making HTTP requests in sequence.
        /// \details Steps:
        /// 1. Requests the main page to obtain necessary authentication parameters.
        /// 2. Sends login credentials to obtain the user ID and hash.
        /// 3. Uses the obtained credentials to authenticate.
        /// If any step fails, the process stops and the callback is invoked with an error message.
        /// \param result_callback The callback function to be called upon success or failure.
        void perform_auth_flow(
            std::function<void(
                bool success,
                const std::string& reason)> result_callback);

        /// \brief Handles an authentication event.
        /// \param event The received authentication data event.
        void handle_event(const events::AuthDataEvent& event);

        /// \brief Handles a connection request event.
        /// \param event The received connection request event.
        void handle_event(const events::ConnectRequestEvent& event);

        /// \brief Handles an authentication restart request event.
        /// \param event The received restart authentication request event.
        void handle_event(const events::RestartAuthEvent& event);

        /// \brief Handles a disconnection request event.
        /// \param event The received disconnection request event.
        void handle_event(const events::DisconnectRequestEvent& event);

        /// \brief Retrieves the account information object.
        /// \return Shared pointer to `AccountInfoData`.
        std::shared_ptr<AccountInfoData> get_account_info();
    };

    void AuthManager::on_event(const utils::Event* const event) {
        if (const auto* msg = dynamic_cast<const events::AuthDataEvent*>(event)) {
            handle_event(*msg);
        } else
        if (const auto* msg = dynamic_cast<const events::ConnectRequestEvent*>(event)) {
            handle_event(*msg);
        } else
        if (const auto* msg = dynamic_cast<const events::DisconnectRequestEvent*>(event)) {
            handle_event(*msg);
        } else
        if (const auto* msg = dynamic_cast<const events::RestartAuthEvent*>(event)) {
            handle_event(*msg);
        }
    };

    void AuthManager::process() {
        m_task_manager.process();
    }

    void AuthManager::shutdown() {
        m_task_manager.shutdown();
    }

    void AuthManager::set_auth_credentials(
            const std::string& user_id,
            const std::string& user_hash) {
        std::string cookies = "user_id=" + user_id + "; user_hash=" + user_hash;
        storage::ServiceSessionDB::get_instance().set_session_value(to_str(PlatformType::INTRADE_BAR), m_auth_data->email, cookies);
        m_request_manager.set_auth_credentials(user_id, user_hash);
    }

    void AuthManager::handle_event(const events::AuthDataEvent& event) {
        if (auto new_auth_data = std::dynamic_pointer_cast<AuthData>(event.auth_data)) {
            LOGIT_TRACE0();
            m_new_auth_data = std::make_unique<AuthData>(*new_auth_data.get());
        }
    }

    void AuthManager::handle_event(const events::ConnectRequestEvent& event) {
        auto callback = event.callback;
        m_task_manager.add_single_task([this, callback](std::shared_ptr<utils::Task> task) {
            // Отменяем все запросы
            m_request_manager.cancel_requests();

            if (task->is_shutdown()) {
                using Status = events::AccountInfoUpdateEvent::Status;
                auto account_info = get_account_info();
                if (account_info->connect) {
                    account_info->connect = false;
                    notify(events::AccountInfoUpdateEvent(
                        account_info,
                        Status::DISCONNECTED,
                        "Shutdown."));
                }
                return;
            }

            // Обработаем подключение после получения результата всех HTTP запросов
            m_task_manager.add_single_task([this, callback](std::shared_ptr<utils::Task> task) {
                using Status = events::AccountInfoUpdateEvent::Status;
                if (task->is_shutdown()) {
                    auto account_info = get_account_info();
                    if (account_info->connect) {
                        account_info->connect = false;
                        notify(events::AccountInfoUpdateEvent(
                            account_info,
                            Status::DISCONNECTED,
                            "Shutdown."));
                    }
                    return;
                }

                auto account_info = get_account_info();

                if (account_info->connect) {
                    account_info->connect = false;
                    notify(events::AccountInfoUpdateEvent(
                        account_info,
                        Status::DISCONNECTED,
                        "Reconnecting."));
                }

                // Отменяем все запросы
                m_request_manager.cancel_requests();

                if (!m_new_auth_data) {
                    const std::string error_text("Authentication data is missing.");
                    handle_auth_failure(error_text, std::move(callback));
                    return;
                }

                m_auth_data = std::make_shared<AuthData>(*m_new_auth_data.get());
                account_info->account_type = m_auth_data->account_type;
                account_info->currency     = m_auth_data->currency;
                notify(events::AccountInfoUpdateEvent(
                    account_info,
                    Status::CONNECTING));

                using AuthMethod = AuthData::AuthMethod;
                switch (m_auth_data->auth_method) {
                case AuthMethod::NONE: {
                        const std::string error_text("Authentication method is not specified.");
                        LOGIT_ERROR(error_text);
                        callback({false, error_text, m_auth_data->clone_unique()});
                        notify(events::AccountInfoUpdateEvent(
                            account_info,
                            Status::FAILED_TO_CONNECT,
                            error_text));
                    } break;
                case AuthMethod::EMAIL_PASSWORD:
                    // Process authentication via email and password
                    validate_email_pass(std::move(callback));
                    break;
                case AuthMethod::USER_TOKEN:
                    // Validate user authentication token
                    validate_user_token(std::move(callback));
                    break;
                default: {
                        const std::string error_text("Unsupported authentication method.");
                        handle_auth_failure(error_text, std::move(callback));
                    } break;
                };
            });
        });
    }

    void AuthManager::handle_event(const events::DisconnectRequestEvent& event) {
        auto callback = event.callback;
        m_task_manager.add_single_task([this, callback](std::shared_ptr<utils::Task> task) {
            LOGIT_INFO("Cancelling all pending requests before disconnecting.");
            m_request_manager.cancel_requests();

            if (task->is_shutdown()) {
                using Status = events::AccountInfoUpdateEvent::Status;
                auto account_info = get_account_info();
                if (account_info->connect) {
                    account_info->connect = false;
                    notify(events::AccountInfoUpdateEvent(
                        account_info,
                        Status::DISCONNECTED,
                        "Shutdown."));
                }
                return;
            }

            // Обработаем отключение после получения результата всех HTTP запросов
            m_task_manager.add_single_task([this, callback](std::shared_ptr<utils::Task> task) {
                auto account_info = get_account_info();
                if (account_info->connect) {
                    using Status = events::AccountInfoUpdateEvent::Status;
                    account_info->connect = false;
                    LOGIT_INFO("Disconnected from the platform.");
                    const std::string event_text("Successfully disconnected.");
                    callback({true, event_text, m_auth_data ? m_auth_data->clone_unique() : nullptr});
                    notify(events::AccountInfoUpdateEvent(
                        get_account_info(),
                        Status::DISCONNECTED,
                        event_text));
                } else {
                    LOGIT_WARN("Disconnect request received, but the account was already disconnected.");
                    const std::string error_text("Already disconnected.");
                    callback({false, error_text, m_auth_data ? m_auth_data->clone_unique() : nullptr});
                }
            });
        });
    }

    void AuthManager::handle_event(const events::RestartAuthEvent& event) {
        start_authentication([this](const ConnectionResult& result) {
            if (!result.success) {
                auto account_info = get_account_info();
                if (account_info->connect) {
                    using Status = events::AccountInfoUpdateEvent::Status;
                    account_info->connect = false;
                    notify(events::AccountInfoUpdateEvent(
                        get_account_info(),
                        Status::DISCONNECTED,
                        result.reason));
                }
                return;
            }

            finalize_authentication(
                [this](const ConnectionResult& result) {
            });
        });
    }

    void AuthManager::validate_email_pass(connection_callback_t callback) {
        // Validate email and password
        if (m_auth_data->email.empty() ||
            m_auth_data->password.empty()) {
            LOGIT_ERROR_IF(m_auth_data->email.empty(), "Validation failed: Email is missing.");
            LOGIT_ERROR_IF(m_auth_data->password.empty(), "Validation failed: Password is missing.");

            const std::string error_text("Login failed: Please provide both email and password.");
            handle_auth_failure(error_text, std::move(callback));
            return;
        }

        // Attempt to retrieve session from storage
        auto session = storage::ServiceSessionDB::get_instance().get_session_value(
            to_str(PlatformType::INTRADE_BAR),
            m_auth_data->email);

        if (!session) {
            LOGIT_TRACE("Session not found for email: ", m_auth_data->email);
            execute_authentication_flow(std::move(callback));
            return;
        }

        LOGIT_PRINT_TRACE("Session retrieved for email: ", m_auth_data->email);

        // Set cookies from the session
        std::string cookies = *session;
        auto parsed_data = parse_cookies(cookies);

        if (parsed_data) {
            const auto& [user_id, user_hash] = *parsed_data;
            LOGIT_PRINT_TRACE("Cookies parsed successfully for user_id: ", user_id);
            m_request_manager.set_auth_credentials(user_id, user_hash);

            // Start authentication flow
            start_authentication(
                    [this, callback](const ConnectionResult &result) {
                if (!result.success) {
                    LOGIT_PRINT_ERROR("Authentication failed: ", result.reason);
                    execute_authentication_flow(std::move(callback));
                    return;
                }

                finalize_authentication(std::move(callback));
            });
        } else {
            LOGIT_PRINT_ERROR("Failed to parse cookies: ", cookies);
            execute_authentication_flow(std::move(callback));
        }
    }

    void AuthManager::validate_user_token(connection_callback_t callback) {
        // Validate user_id and token
        if (m_auth_data->user_id.empty() || m_auth_data->token.empty()) {
            LOGIT_ERROR_IF(m_auth_data->user_id.empty(), "Validation failed: User ID is missing.");
            LOGIT_ERROR_IF(m_auth_data->token.empty(), "Validation failed: Token is missing.");

            const std::string error_text("Validation failed: User ID or token is missing.");
            handle_auth_failure(error_text, std::move(callback));
            return;
        }

        LOGIT_TRACE("Validation passed for user_id: ", m_auth_data->user_id);

        // Set user ID, token, and cookies
        m_request_manager.set_auth_credentials(m_auth_data->user_id, m_auth_data->token);
        start_authentication([this, callback](const ConnectionResult& result) {
            if (!result.success) {
                const std::string error_text("Login failed: Please provide both User ID and token.");
                handle_auth_failure(error_text, std::move(callback));
                return;
            }

            finalize_authentication(std::move(callback));
        });
    }

    void AuthManager::handle_auth_failure(
            const std::string& reason,
            connection_callback_t callback) {
        using Status = events::AccountInfoUpdateEvent::Status;
        LOGIT_ERROR("Authentication failed: ", reason);
        callback({false, reason, m_auth_data ? m_auth_data->clone_unique() : nullptr});
        notify(events::AccountInfoUpdateEvent(
            get_account_info(),
            Status::FAILED_TO_CONNECT,
            reason));
    }

    void AuthManager::execute_authentication_flow(
            connection_callback_t callback) {
        perform_auth_flow([this, callback](
                bool success,
                const std::string& reason) {
            if (!success) {
                handle_auth_failure(reason, std::move(callback));
                return;
            }

            start_authentication([this, callback](const ConnectionResult& result) {
                if (!result.success) {
                    handle_auth_failure(result.reason, std::move(callback));
                    return;
                }

                finalize_authentication(std::move(callback));
            });
        });
    }

    void AuthManager::perform_auth_flow(
            std::function<void(
                bool success,
                const std::string& reason)> result_callback) {
        LOGIT_TRACE0();

        // Step 3: Handle authentication response
        auto handle_auth_response = [this, result_callback](
                    bool success,
                    const std::string& user_id,
                    const std::string& user_hash,
                    const std::string& cookies,
                    const std::string& reason) {
            if (!success) {
                result_callback(false, reason);
                return;
            }

            // Store authentication details
            LOGIT_PRINT_TRACE("Authentication successful. User ID: ", user_id);
            set_auth_credentials(user_id, user_hash);
            result_callback(true, std::string());
        };

        // Step 2: Handle login response
        auto handle_login_response = [this, handle_auth_response](
                bool success,
                const std::string& user_id,
                const std::string& user_hash,
                const std::string& cookies,
                const std::string& reason) {
            if (!success) {
                handle_auth_response(false, std::string(), std::string(), std::string(), reason);
                return;
            }

            // Proceed to authentication
            m_request_manager.request_auth(user_id, user_hash, cookies, m_auth_data,
                [handle_auth_response, user_id, user_hash, cookies](
                        bool success,
                        const std::string& reason) {
                    handle_auth_response(success, user_id, user_hash, cookies, reason);
                });
        };

        // Step 1: Handle main page response
        auto handle_main_page_response = [this, handle_login_response](
                    bool success,
                    const std::string& req_id,
                    const std::string& req_value,
                    const std::string& cookies,
                    const std::string& reason) {
            if (!success) {
                handle_login_response(false, std::string(), std::string(), std::string(), reason);
                return;
            }

            // Proceed to login
            m_request_manager.request_login(req_id, req_value, cookies, m_auth_data,
                [handle_login_response](
                        bool success,
                        const std::string& user_id,
                        const std::string& user_hash,
                        const std::string& cookies,
                        const std::string& reason) {
                    handle_login_response(success, user_id, user_hash, cookies, reason);
                });
        };

        // Start the authentication process
        m_request_manager.request_main_page(m_auth_data, handle_main_page_response);
    }

    void AuthManager::start_authentication(connection_callback_t callback) {
        LOGIT_TRACE0();
        m_request_manager.request_profile([this, callback](
                bool success,
                CurrencyType currency,
                AccountType account_type) {
            if (!success) {
                const std::string error_text = "Failed to retrieve profile information.";
                LOGIT_ERROR(error_text);
                callback({false, error_text, m_auth_data->clone_unique()});
                return;
            }
            handle_account_type_switch(account_type, currency, std::move(callback));
        });
    }

    void AuthManager::handle_account_type_switch(
            AccountType account_type,
            CurrencyType currency,
            connection_callback_t callback) {
        LOGIT_TRACE0();
        if (m_auth_data->account_type != account_type) {
            m_request_manager.request_switch_account_type([this, currency, callback](bool success) {
                if (!success) {
                    LOGIT_ERROR("Failed to switch account type.");
                    callback({false, "Failed to switch account type.", m_auth_data->clone_unique()});
                    return;
                }

                using Status = events::AccountInfoUpdateEvent::Status;
                auto account_info = get_account_info();
                account_info->account_type = m_auth_data->account_type;
                notify(events::AccountInfoUpdateEvent(account_info, Status::ACCOUNT_TYPE_CHANGED));

                handle_currency_switch(currency, std::move(callback));
            });
        } else {
            handle_currency_switch(currency, std::move(callback));
        }
    }

    void AuthManager::handle_currency_switch(
            CurrencyType currency,
            connection_callback_t callback) {
        LOGIT_TRACE0();
        if (m_auth_data->currency != currency) {
            m_request_manager.request_switch_currency([this, callback](bool success) {
                if (!success) {
                    LOGIT_ERROR("Failed to switch currency.");
                    callback({false, "Failed to switch currency.", m_auth_data->clone_unique()});
                    return;
                }

                using Status = events::AccountInfoUpdateEvent::Status;
                auto account_info = get_account_info();
                account_info->currency = m_auth_data->currency;
                notify(events::AccountInfoUpdateEvent(account_info, Status::CURRENCY_CHANGED));

                callback({true, std::string(), m_auth_data->clone_unique()});
            });
        } else {
            callback({true, std::string(), m_auth_data->clone_unique()});
        }
    }

    void AuthManager::finalize_authentication(connection_callback_t callback) {
        LOGIT_TRACE0();
        m_request_manager.request_balance([this, callback](
                bool success,
                double balance,
                CurrencyType currency) {
            using Status = events::AccountInfoUpdateEvent::Status;
            if (!success) {
                const std::string error_text("Failed to retrieve balance.");
                LOGIT_ERROR(error_text);
                callback({false, error_text, m_auth_data->clone_unique()});

                auto account_info = get_account_info();
                if (account_info->connect) {
                    account_info->connect = false;
                    notify(events::AccountInfoUpdateEvent(account_info, Status::DISCONNECTED, error_text));
                } else {
                    notify(events::AccountInfoUpdateEvent(account_info, Status::FAILED_TO_CONNECT, error_text));
                }
                return;
            }

            LOGIT_TRACE("Authentication successful. Connection established.");
            callback({true, std::string(), m_auth_data->clone_unique()});

            auto account_info = get_account_info();
            account_info->account_type = m_auth_data->account_type;
            account_info->currency = currency;
            const double account_balance = account_info->balance;
            account_info->balance = balance;

            if (!account_info->connect) {
                account_info->connect  = true;
                notify(events::AccountInfoUpdateEvent(account_info, Status::CONNECTED));
            }

            if (std::abs(account_balance - balance) >= 0.01) {
                notify(events::AccountInfoUpdateEvent(account_info, Status::BALANCE_UPDATED));
            }
        });
    }

    std::shared_ptr<AccountInfoData> AuthManager::get_account_info() {
        if (auto account_info = std::dynamic_pointer_cast<AccountInfoData>(m_account_info)) {
            return account_info;
        }
        LOGIT_FATAL("Failed to cast IAccountInfoData to AccountInfoData");
        throw std::runtime_error("Invalid account information type");
    }

} // namespace optionx::platforms::intrade_bar

#endif // _OPTIONX_PLATFORMS_INTRADERBAR_AUTH_MANAGER_HPP_INCLUDED
