namespace optionx {
namespace platforms {
namespace intrade_bar {

    // Handles balance updates.
    void HttpClientModule::handle_balance_update() {
        request_balance([this](bool success, double balance, CurrencyType currency) {
            auto account_info = get_account_info();
            if (!account_info) {
                LOGIT_ERROR("Failed to get account info.");
                return;
            }

            if (success) {
                process_balance_success(balance, currency, account_info);
            } else {
                process_balance_failure(account_info);
            }
        });
    }

    // Processes successful balance updates.
    void HttpClientModule::process_balance_success(double balance, CurrencyType currency, std::shared_ptr<AccountInfoData> account_info) {
        const double previous_balance = account_info->balance;
        account_info->balance = balance;

        if (!account_info->connect) {
            account_info->connect = true;
            notify_account_status(account_info, modules::AccountInfoUpdateEvent::Status::CONNECTED);
        }

        auto now = std::chrono::steady_clock::now();
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last_trades_time).count();
        const int64_t trades_event_time = 60 * time_shield::MS_PER_SEC;

        if ((account_info->currency != currency) ||
            (std::abs(previous_balance - balance) > 0.01 && elapsed_ms >= trades_event_time)) {
            restart_authentication_flow();
        } else
        if (std::abs(previous_balance - balance) > 0.01) {
            notify_account_status(account_info, modules::AccountInfoUpdateEvent::Status::BALANCE_UPDATED);
        }
    }

    // Processes balance update failure.
    void HttpClientModule::process_balance_failure(std::shared_ptr<AccountInfoData> account_info) {
        account_info->connect = false;
        notify_account_status(account_info, modules::AccountInfoUpdateEvent::Status::DISCONNECTED);
    }

    // Restarts the authentication flow.
    void HttpClientModule::restart_authentication_flow() {
        m_balance_task.stop();
        auto auth_data = std::make_shared<AuthData>(*m_auth_data.get());
        start_authentication_flow(auth_data, [this](bool success, const std::string& reason, std::unique_ptr<IAuthData> auth_data) {
            m_balance_task.start();
            m_price_task.start();
        });
    }

} // namespace intrade_bar
} // namespace platforms
} // namespace optionx
