namespace optionx {
namespace platforms {
namespace intrade_bar {

    // Initializes rate limits.
    void HttpClientModule::initialize_rate_limits() {
        using RateLimitType = modules::HttpClientModule::RateLimitType;
        set_rate_limit_rpm(RateLimitType::GENERAL, 60);
        set_rate_limit_rps(RateLimitType::TRADE_EXECUTION, 1);
        set_rate_limit_rps(RateLimitType::TRADE_RESULT, 1);
        set_rate_limit_rps(RateLimitType::BALANCE, 1);
        set_rate_limit_rpm(RateLimitType::ACCOUNT_INFO, 6);
        set_rate_limit_rpm(RateLimitType::ACCOUNT_SETTINGS, 12);
        set_rate_limit_rps(RateLimitType::TICK_DATA, 1);
    }

    // Initializes headers for the HTTP client.
    void HttpClientModule::initialize_headers() {
        m_api_headers = {
            {"Accept", "*/*"},
            {"Content-Type", "application/x-www-form-urlencoded; charset=UTF-8"},
            {"X-Requested-With", "XMLHttpRequest"}
        };

        kurlyk::Headers headers = {
            {"Connection", "keep-alive"},
            {"Cache-Control", "no-cache"},
            {"pragma", "no-cache"},
            {"sec-ch-ua-mobile", "?0"},
            {"sec-ch-ua-platform", "Windows"},
            {"sec-fetch-dest", "document"},
            {"sec-fetch-mode", "navigate"},
            {"sec-fetch-site", "same-origin"},
            {"sec-fetch-user", "?1"},
            {"sec-gpc", "1"}
        };
        modules::HttpClientModule::m_client.set_headers(headers);
    }

    // Configures the HTTP client.
    void HttpClientModule::configure_client() {
        modules::HttpClientModule::m_client.set_follow_location(true);
        modules::HttpClientModule::m_client.set_max_redirects(10);
        using RateLimitType = modules::HttpClientModule::RateLimitType;
        modules::HttpClientModule::m_client.assign_rate_limit_id(
            get_rate_limit(RateLimitType::GENERAL),
            kurlyk::RateLimitType::General);
    }

    // Notifies about account status updates.
    void HttpClientModule::notify_account_status(std::shared_ptr<AccountInfoData> account_info, modules::AccountInfoUpdateEvent::Status status) {
        modules::AccountInfoUpdateEvent event(account_info, status);
        notify(event);
    }

    std::shared_ptr<AccountInfoData> HttpClientModule::get_account_info() {
        // Attempt to cast the account information to the expected type
        if (auto account_info = std::dynamic_pointer_cast<AccountInfoData>(modules::HttpClientModule::m_account_info)) {
            return account_info;
        }
        LOGIT_FATAL("Failed to cast IAccountInfoData to AccountInfoData");
        throw std::runtime_error("Invalid account information type");
    }

} // namespace intrade_bar
} // namespace platforms
} // namespace optionx
