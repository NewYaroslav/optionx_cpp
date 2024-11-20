namespace optionx {
namespace platforms {
namespace intrade_bar {

    void HttpClientModule::handle_event(const modules::AccountInfoRequestEvent& event) {

    }

    void HttpClientModule::handle_event(const modules::BalanceRequestEvent& event) {
        m_last_trades_time = std::chrono::steady_clock::now();
    }

    void HttpClientModule::handle_event(const modules::TradeRequestEvent& event) {
        m_last_trades_time = std::chrono::steady_clock::now();
        auto request = event.request;
        auto result  = event.result;
        request_execute_trade(request, result, [this, request, result](bool success) {
            request_balance([this, request, result](bool success, double balance, CurrencyType currency) {
                if (success) {
                    result->balance = balance;
                } else {
                    auto account_info = get_account_info();
                    result->balance = account_info->balance;
                }
                result->state = OrderState::OPEN_SUCCESS;
                result->current_state = OrderState::STANDOFF;
            });
        });
    }

    void HttpClientModule::handle_event(const modules::TradeStatusEvent& event) {
        m_last_trades_time = std::chrono::steady_clock::now();
        auto &request = event.request;
        auto &result  = event.result;
        request_trade_check(result->option_id, [this, request, result](bool success, double price, double profit) {
            if (!success) {
                LOGIT_ERROR("Failed to retrieve trade result for option ID: ", result->option_id);
                auto account_info = get_account_info();
                result->payout = account_info->get_account_info<int64_t>(AccountInfoType::PAYOUT, request, time_shield::ms_to_sec(result->open_date));
                result->profit =
                    result->current_state == OrderState::STANDOFF ? 0 :
                    (result->current_state == OrderState::WIN ?
                     result->payout * result->amount : -result->amount);
                result->state = result->current_state = OrderState::CHECK_ERROR;
                return;
            }
            request_balance([this, request, result, price, profit](bool success, double balance, CurrencyType currency) {
                process_trade_status(price, profit, (success ? balance : 0.0), request, result);
            });
        });
    }

    void HttpClientModule::handle_event(const modules::AuthDataEvent& event) {
        LOGIT_TRACE0();
        if (auto msg = std::dynamic_pointer_cast<AuthData>(event.auth_data)) {
            const std::string referer(msg->host + "/");
            modules::HttpClientModule::m_client.set_host(msg->host);
            modules::HttpClientModule::m_client.set_user_agent(msg->user_agent);
            modules::HttpClientModule::m_client.set_accept_language(msg->accept_language);
            modules::HttpClientModule::m_client.set_origin(msg->host);
            modules::HttpClientModule::m_client.set_referer(referer);
            modules::HttpClientModule::m_client.set_verbose(true);
            m_auth_data = std::make_unique<AuthData>(*msg.get());
            LOGIT_TRACE0();
        }
    }

    void HttpClientModule::handle_event(const modules::ConnectRequestEvent& event) {
        LOGIT_TRACE0();

        m_balance_task.stop();
        m_price_task.stop();

        if (!m_auth_data) {
            LOGIT_ERROR("Authentication data is missing.");
            event.callback(false, "Authentication data is not available.", nullptr);
            return;
        }

        using Status = modules::AccountInfoUpdateEvent::Status;
        auto account_info = get_account_info();
        if (account_info->connect) {
            account_info->connect = false;
            modules::AccountInfoUpdateEvent event(account_info, Status::DISCONNECTED);
            notify(event);
        }
        account_info->account_type = m_auth_data->account_type;
        account_info->currency = m_auth_data->currency;

        modules::AccountInfoUpdateEvent account_info_event(account_info, Status::CONNECTING);
        notify(account_info_event);

        using AuthMethod = AuthData::AuthMethod;

        switch (m_auth_data->auth_method) {
        case AuthMethod::NONE:
            LOGIT_ERROR("Authentication method is not specified.");
            event.callback(false, "Authentication method is not specified.", m_auth_data->clone_unique());
            break;
        case AuthMethod::EMAIL_PASSWORD:
            // Process authentication via email and password
            validate_email_pass(std::make_shared<AuthData>(*m_auth_data.get()), event.callback);
            break;
        case AuthMethod::USER_TOKEN:
            // Validate user authentication token
            validate_user_token(std::make_shared<AuthData>(*m_auth_data.get()), event.callback);
            break;
        default:
            // Handle unsupported authentication methods
            LOGIT_ERROR("Unsupported authentication method.");
            event.callback(false, "Unsupported authentication method.", m_auth_data->clone_unique());
            break;
        }
    }

    // Initializes periodic tasks for balance and price updates.
    void HttpClientModule::initialize_tasks() {
        m_balance_task.set_callback([this]() { handle_balance_update(); });
        m_price_task.set_callback([this]() { handle_price_update(); });

        m_balance_task.set_period(5 * time_shield::MS_PER_MIN);
        m_price_task.set_period(2 * time_shield::MS_PER_SEC);

        m_balance_task.start();
        m_price_task.start();
    }

} // namespace intrade_bar
} // namespace platforms
} // namespace optionx
