namespace optionx {
namespace platforms {
namespace intrade_bar {

    void HttpClientModule::validate_email_pass(
            std::shared_ptr<AuthData> auth_data,
            modules::ConnectRequestEvent::callback_t connect_callback) {
        // Validate email and password
        if (auth_data->email.empty() || auth_data->password.empty()) {
            LOGIT_ERROR_IF(auth_data->email.empty(), "Validation failed: Email is missing.");
            LOGIT_ERROR_IF(auth_data->password.empty(), "Validation failed: Password is missing.");
            connect_callback(false, "Validation failed: Email or password is missing.", auth_data->clone_unique());
            return;
        }

        // Attempt to retrieve session from storage
        auto session = storage::ServiceSessionDB::get_instance().get_session_value(
            to_str(ApiType::INTRADE_BAR), auth_data->email);

        if (!session) {
            LOGIT_TRACE("Session not found for email: ", auth_data->email);
            request_main_page(auth_data, connect_callback);
            return;
        }

        LOGIT_TRACE("Session retrieved for email: ", auth_data->email);

        // Set cookies from the session
        m_cookies = *session;
        if (parse_cookies(m_cookies, m_user_id, m_user_hash)) {
            LOGIT_TRACE("Cookies parsed successfully for user_id: ", m_user_id);

            // Start authentication flow
            start_authentication_flow(auth_data,
                [this, auth_data, connect_callback](bool success, const std::string& reason, std::unique_ptr<IAuthData> _auth_data) {
                    if (success) {
                        connect_callback(true, std::string(), auth_data->clone_unique());
                    } else {
                        LOGIT_ERROR("Authentication failed: ", reason);
                        request_main_page(auth_data, connect_callback);
                    }
                });
        } else {
            LOGIT_ERROR("Failed to parse cookies: ", m_cookies, ". Invalid user_id: ", m_user_id, " or user_hash. Size m_user_hash: ", m_user_hash.size());
            request_main_page(auth_data, connect_callback);
        }
    }

    void HttpClientModule::validate_user_token(
            std::shared_ptr<AuthData> auth_data,
            modules::ConnectRequestEvent::callback_t connect_callback) {
        // Validate user_id and token
        if (auth_data->user_id.empty() || auth_data->token.empty()) {
            LOGIT_ERROR_IF(auth_data->user_id.empty(), "Validation failed: User ID is missing.");
            LOGIT_ERROR_IF(auth_data->token.empty(), "Validation failed: Token is missing.");
            connect_callback(false, "Validation failed: User ID or token is missing.", auth_data->clone_unique());
            return;
        }

        LOGIT_TRACE("Validation passed for user_id: ", auth_data->user_id);

        // Set user ID, token, and cookies
        m_user_id = auth_data->user_id;
        m_user_hash = auth_data->token;
        m_cookies = "user_id=" + m_user_id + "; user_hash=" + m_user_hash;

        // Start authentication flow
        start_authentication_flow(auth_data, connect_callback);
    }

    void HttpClientModule::start_authentication_flow(
            std::shared_ptr<AuthData> auth_data,
            modules::ConnectRequestEvent::callback_t connect_callback) {
        LOGIT_TRACE0();
        request_profile([this, auth_data, connect_callback](bool success, CurrencyType currency, AccountType account_type) {
            if (!success) {
                connect_callback(false, "Failed to retrieve profile information.", auth_data->clone_unique());
                LOGIT_ERROR("Failed to retrieve profile information.");
                return;
            }

            handle_account_type_switch(auth_data, account_type, currency, connect_callback);
        });
    }

    void HttpClientModule::handle_account_type_switch(
            std::shared_ptr<AuthData> auth_data,
            AccountType account_type,
            CurrencyType currency,
            modules::ConnectRequestEvent::callback_t connect_callback) {
        LOGIT_TRACE0();
        if (auth_data->account_type != account_type) {
            request_switch_account_type([this, auth_data, currency, connect_callback](bool success) {
                if (!success) {
                    connect_callback(false, "Failed to switch account type.", auth_data->clone_unique());
                    LOGIT_ERROR("Failed to switch account type.");
                    return;
                }

                handle_currency_switch(auth_data, currency, connect_callback);
            });
        } else {
            handle_currency_switch(auth_data, currency, connect_callback);
        }
    }

    void HttpClientModule::handle_currency_switch(
            std::shared_ptr<AuthData> auth_data,
            CurrencyType currency,
            modules::ConnectRequestEvent::callback_t connect_callback) {
        LOGIT_TRACE0();
        if (auth_data->currency != currency) {
            request_switch_currency([this, auth_data, connect_callback](bool success) {
                if (!success) {
                    connect_callback(false, "Failed to switch currency.", auth_data->clone_unique());
                    LOGIT_ERROR("Failed to switch currency.");
                    return;
                }

                finalize_authentication(auth_data, connect_callback);
            });
        } else {
            finalize_authentication(auth_data, connect_callback);
        }
    }

    void HttpClientModule::finalize_authentication(
            std::shared_ptr<AuthData> auth_data,
            modules::ConnectRequestEvent::callback_t connect_callback) {
        LOGIT_TRACE0();
        request_balance([this, auth_data, connect_callback](bool success, double balance, CurrencyType currency) {
            if (!success) {
                using Status = modules::AccountInfoUpdateEvent::Status;
                auto account_info = get_account_info();
                if (account_info->connect) {
                    account_info->connect = false;
                    modules::AccountInfoUpdateEvent event(account_info, Status::DISCONNECTED);
                    notify(event);
                }

                connect_callback(false, "Failed to retrieve balance.", auth_data->clone_unique());
                LOGIT_ERROR("Failed to retrieve balance.");
                return;
            }

            try {
                auto account_info = get_account_info();
                using Status = modules::AccountInfoUpdateEvent::Status;
                if (!account_info->connect) {
                    account_info->account_type = auth_data->account_type;
                    account_info->currency = currency;
                    account_info->connect = true;
                    const double account_balance = account_info->balance;
                    account_info->balance = balance;
                    modules::AccountInfoUpdateEvent event(account_info, Status::CONNECTED);
                    notify(event);
                    if (std::abs(account_balance - balance) >= 0.01) {
                        modules::AccountInfoUpdateEvent event(account_info, Status::BALANCE_UPDATED);
                        notify(event);
                    }
                }

                connect_callback(true, std::string(), auth_data->clone_unique());
                m_balance_task.start();
                m_price_task.start();

                LOGIT_TRACE("Authentication successful. Connection established.");
            } catch (const std::exception& ex) {
                LOGIT_ERROR("Exception during account info update: ", ex.what());
                connect_callback(false, "Error updating account information.", auth_data->clone_unique());
            }
        });
    }

} // namespace intrade_bar
} // namespace platforms
} // namespace optionx

