#pragma once
#ifndef _OPTIONX_PLATFORMS_INTRADERBAR_REQUEST_MANAGER_HPP_INCLUDED
#define _OPTIONX_PLATFORMS_INTRADERBAR_REQUEST_MANAGER_HPP_INCLUDED

/// \file RequestManager.hpp
/// \brief Handles HTTP requests for user authentication, trade execution, balance updates, and market data retrieval.

namespace optionx::platforms::intrade_bar {

    /// \class RequestManager
    /// \brief Manages HTTP requests for the Intrade Bar platform.
    ///
    /// The `RequestManager` class is responsible for handling HTTP-based communication
    /// with the trading platform, including authentication, balance retrieval, price updates,
    /// and trade execution.
    class RequestManager final : public modules::BaseModule {
    public:

        /// \brief Constructs the request manager.
        /// \param platform Reference to the trading platform.
        /// \param client Reference to the HTTP client module.
        explicit RequestManager(
                BaseTradingPlatform& platform,
                HttpClientModule& client)
                : BaseModule(platform.event_hub()), m_client(client) {
            subscribe<events::AuthDataEvent>(this);

            m_api_headers = {
                {"Accept", "*/*"},
                {"Content-Type", "application/x-www-form-urlencoded; charset=UTF-8"},
                {"X-Requested-With", "XMLHttpRequest"}
            };

            const kurlyk::Headers headers = {
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
            get_http_client().set_headers(headers);
            platform.register_module(this);
        }

        /// \brief Default destructor.
        virtual ~RequestManager() = default;

        /// \brief Processes incoming events and dispatches them to the appropriate handlers.
        /// \param event The received event.
        void on_event(const utils::Event* const event) override {
            if (const auto* msg = dynamic_cast<const events::AuthDataEvent*>(event)) {
                handle_event(*msg);
            }
        }

        /// \brief Sets authentication credentials for HTTP requests.
        /// \param user_id The user ID.
        /// \param user_hash The authentication hash.
        void set_auth_credentials(
                const std::string& user_id,
                const std::string& user_hash) {
            m_user_id = user_id;
            m_user_hash = user_hash;
            m_cookies = "user_id=" + user_id + "; user_hash=" + user_hash;
        }

        /// \brief Cancels all ongoing HTTP requests.
        void cancel_requests() {
            get_http_client().cancel_requests();
        }
        
        /// \brief Performs a sweep over known domain variants to find a working endpoint.
        /// \param callback Callback to receive the first working domain, if any.
        void request_find_working_domain(
            std::function<void(
                bool success,
                std::string &host)> find_callback);
                
        /// \brief Checks if the currently set host in the HTTP client is available.
        /// \param check_callback Callback that receives the result: true if available, false otherwise.
        void request_check_current_host_available(
            std::function<void(bool success)> check_callback);

        /// \brief Requests user profile information.
        /// \param profile_callback Callback function to receive profile details.
        void request_profile(
            std::function<void(
                bool success,
                CurrencyType currency,
                AccountType account)> profile_callback);

        /// \brief Requests the main page to extract authentication parameters.
        /// \param auth_data Shared pointer to authentication data.
        /// \param callback Callback function to receive authentication parameters.
        void request_main_page(
            std::shared_ptr<AuthData> auth_data,
            std::function<void(
                bool success,
                const std::string& req_id,
                const std::string& req_value,
                const std::string& cookies,
                const std::string& reason)> callback);

        /// \brief Sends login credentials for authentication.
        /// \param req_id The request ID obtained from the main page.
        /// \param req_value The request value obtained from the main page.
        /// \param cookies Cookies extracted from the main page.
        /// \param auth_data Shared pointer to authentication data.
        /// \param result_callback Callback function to receive login result.
        void request_login(
            const std::string& req_id,
            const std::string& req_value,
            const std::string& cookies,
            std::shared_ptr<AuthData> auth_data,
            std::function<void(
                bool success,
                const std::string& user_id,
                const std::string& user_hash,
                const std::string& cookies,
                const std::string& reason)> result_callback);

        /// \brief Performs authentication using user credentials.
        /// \param user_id The user ID.
        /// \param user_hash The authentication hash.
        /// \param cookies Cookies extracted from previous authentication steps.
        /// \param auth_data Shared pointer to authentication data.
        /// \param result_callback Callback function to receive authentication result.
        void request_auth(
            const std::string& user_id,
            const std::string& user_hash,
            const std::string& cookies,
            std::shared_ptr<AuthData> auth_data,
            std::function<void(
                bool success,
                const std::string& reason)> result_callback);

        /// \brief Requests the user's account balance.
        /// \param balance_callback Callback function to receive the balance data.
        void request_balance(
            std::function<void(
                bool success,
                double balance,
                CurrencyType currency)> balance_callback);

        /// \brief Switches between real and demo account types.
        /// \param switch_callback Callback function to receive switch result.
        void request_switch_account_type(
            std::function<void(bool success)> switch_callback);

        /// \brief Switches the account's currency.
        /// \param switch_callback Callback function to receive switch result.
        void request_switch_currency(
            std::function<void(bool success)> switch_callback);

        /// \brief Requests the latest price updates.
        /// \param price_callback Callback function to receive tick data.
        void request_price(
            std::function<void(
                bool success,
                std::vector<TickData> ticks)> price_callback);

        /// \brief Requests the trade check result.
        /// \param deal_id The deal ID to check.
        /// \param retry_attempts Number of retries if the response is empty.
        /// \param callback_check Callback function to receive trade status.
        void request_trade_check(
            int64_t deal_id,
            int retry_attempts,
            std::function<void(
                bool success,
                double price,
                double profit)> callback_check);

        /// \brief Sends a trade execution request.
        /// \param request Shared pointer to the trade request.
        /// \param result Shared pointer to the trade result.
        /// \param request_callback Callback function to receive execution result.
        void request_execute_trade(
            std::shared_ptr<TradeRequest> request,
            std::function<void(
                bool success,
                int64_t option_id,
                int64_t open_date,
                double open_price,
                const std::string& error_desc)> result_callback);

    private:
        HttpClientModule& m_client;      ///< Reference to the HTTP client module.
        kurlyk::Headers   m_api_headers; ///< Default API headers.
        std::string       m_user_id;     ///< User ID for authentication.
        std::string       m_user_hash;   ///< User authentication hash.
        std::string       m_cookies;     ///< Session cookies.
        int m_domain_index_min = 0;      ///< Minimum domain index to scan (0 = intrade.bar).
        int m_domain_index_max = 0;      ///< Maximum domain index to scan (e.g., intrade1000.bar).

        /// \brief Returns a reference to the HTTP client.
        /// \return Reference to the `kurlyk::HttpClient` instance.
        kurlyk::HttpClient& get_http_client() {
            return m_client.get_http_client();
        }

        /// \brief Gets the rate limit for the specified type.
        /// \param rate_limit_id The type of rate limit to retrieve.
        /// \return The rate limit value for the specified type.
        template<class T>
        uint32_t get_rate_limit(T rate_limit_id) const {
            return m_client.get_rate_limit<T>(rate_limit_id);
        }

        /// \brief Adds a new HTTP request task to the list.
        /// \param future The future object representing the pending HTTP response.
        /// \param callback The callback function to handle the response.
        void add_http_request_task(
                std::future<kurlyk::HttpResponsePtr> future,
                std::function<void(kurlyk::HttpResponsePtr)> callback) {
            m_client.add_http_request_task(std::move(future), std::move(callback));
        }

        /// \brief Handles authentication event updates.
        /// \param event The received authentication data event.
        void handle_event(const events::AuthDataEvent& event) {
            if (auto msg = std::dynamic_pointer_cast<AuthData>(event.auth_data)) {
                LOGIT_TRACE0();
                const std::string referer(msg->host + "/");
                auto& client = get_http_client();
                if (!msg->auto_find_domain) {
                    client.set_host(msg->host);
                    client.set_origin(msg->host);
                } else {
                    m_domain_index_min = std::abs(msg->domain_index_min);
                    m_domain_index_max = std::abs(msg->domain_index_max);
                    if (m_domain_index_min > m_domain_index_max) {
                        std::swap(m_domain_index_min, m_domain_index_max);
                    }
                }
                client.set_user_agent(msg->user_agent);
                client.set_accept_language(msg->accept_language);
                client.set_referer(referer);
                client.set_retry_attempts(10, time_shield::MS_PER_SEC);
                client.set_timeout(30);
                client.set_connect_timeout(15);
            }
        }

        void start_authentication(
            std::shared_ptr<AuthData> auth_data,
            connection_callback_t connect_callback);

        void handle_account_type_switch(
            std::shared_ptr<AuthData> auth_data,
            AccountType account_type,
            CurrencyType currency,
            connection_callback_t connect_callback);

        void handle_currency_switch(
            std::shared_ptr<AuthData> auth_data,
            CurrencyType currency,
            connection_callback_t connect_callback);

        void finalize_authentication(
            std::shared_ptr<AuthData> auth_data,
            connection_callback_t connect_callback);
    };

    void RequestManager::request_main_page(
            std::shared_ptr<AuthData> auth_data,
            std::function<void(
                bool success,
                const std::string& req_id,
                const std::string& req_value,
                const std::string& cookies,
                const std::string& reason)> result_callback) {
        LOGIT_TRACE0();

        // Validate email and password
        if (auth_data->email.empty() ||
            auth_data->password.empty()) {
            LOGIT_ERROR_IF(auth_data->email.empty(), "Email is missing.");
            LOGIT_ERROR_IF(auth_data->password.empty(), "Password is missing.");
            result_callback(false, std::string(), std::string(), std::string(), "Email or password is missing.");
            return;
        }

        // Prepare the HTTP GET request
        auto future = get_http_client().get(
            "/",
            kurlyk::QueryParams(),
            {
                {"Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.9"},
                {"Content-Type", "application/x-www-form-urlencoded"},
                {"Upgrade-Insecure-Requests", "1"}
            },
            get_rate_limit(RateLimitType::ACCOUNT_INFO)
        );

        // Handle the HTTP response
        auto callback = [this, auth_data, result_callback](
                kurlyk::HttpResponsePtr response) {
            // Validate the response
            if (!validate_response(response, [&result_callback](const std::string& error_text){
                    result_callback(false, std::string(), std::string(), std::string(), error_text);
                })) {
                return;
            }

            // Parse the response content
            auto parsed_data = parse_main_page_response(response->content, response->headers);
            if (!parsed_data) {
#               ifdef OPTIONX_LOG_UNIQUE_FILE_INDEX
                const int log_index = OPTIONX_LOG_UNIQUE_FILE_INDEX;
                LOGIT_STREAM_ERROR_TO(log_index) << response->content;
                LOGIT_PRINT_ERROR(
                    "Failed to process server response; content log was written to file: ",
                    LOGIT_GET_LAST_FILE_NAME(log_index)
                );
#               else
                LOGIT_ERROR(
                     "Failed to process server response.",
                    (response ? response->status_code : -1)
                );
#               endif
                result_callback(false, std::string(), std::string(), std::string(), "Failed to process server response.");
                return;
            }
            // Extract parsed data
            const auto& [req_id, req_value, cookies] = *parsed_data;

            result_callback(true, req_id, req_value, cookies, "Failed to process server response.");
        };

        // Add the HTTP request task for asynchronous processing
        add_http_request_task(std::move(future), std::move(callback));
    }

    void RequestManager::request_login(
            const std::string& req_id,
            const std::string& req_value,
            const std::string& cookies,
            std::shared_ptr<AuthData> auth_data,
            std::function<void(
                bool success,
                const std::string& user_id,
                const std::string& user_hash,
                const std::string& cookies,
                const std::string& reason)> result_callback) {
        LOGIT_DEBUG(req_id, req_value, cookies);

        // Prepare query parameters
        kurlyk::QueryParams query = {
            {"email", auth_data->email},
            {"password", auth_data->password},
        };
        if (!req_id.empty() && !req_value.empty()) {
            query.emplace(req_id, req_value);
        }

        // Prepare HTTP GET request
        auto future = get_http_client().post(
            "/login",
            kurlyk::QueryParams(),
            {
                {"Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.9"},
                {"Content-Type", "application/x-www-form-urlencoded"},
                {"Upgrade-Insecure-Requests", "1"},
                {"Connection", "keep-alive"},
                {"Cookie", cookies}
            },
            kurlyk::utils::to_query_string(query),
            get_rate_limit(RateLimitType::ACCOUNT_INFO)
        );

        // Define callback for handling HTTP response
        auto callback = [this, cookies, auth_data, result_callback](
                kurlyk::HttpResponsePtr response) {
            if (!validate_response(response, [&result_callback](const std::string& error_text){
                    result_callback(false, std::string(), std::string(), std::string(), error_text);
                })) {
                return;
            }

            auto login_result = parse_login(response->content);
            if (!login_result) {
#               ifdef OPTIONX_LOG_UNIQUE_FILE_INDEX
                const int log_index = OPTIONX_LOG_UNIQUE_FILE_INDEX;
                LOGIT_STREAM_ERROR_TO(log_index) << response->content;
                LOGIT_PRINT_ERROR("Failed to parse login; content log was written to file: ", LOGIT_GET_LAST_FILE_NAME(log_index));
#               else
                LOGIT_PRINT_ERROR("Failed to parse login.");
#               endif
                result_callback(false, std::string(), std::string(), std::string(), "Failed to parse login.");
                return;
            }

            const auto [user_id, user_hash] = *login_result;
            result_callback(true, user_id, user_hash, cookies, std::string());
        };

        // Add the task to handle the HTTP request
        add_http_request_task(std::move(future), std::move(callback));
    }

    void RequestManager::request_auth(
            const std::string& user_id,
            const std::string& user_hash,
            const std::string& cookies,
            std::shared_ptr<AuthData> auth_data,
            std::function<void(
                bool success,
                const std::string& reason)> result_callback) {
        LOGIT_TRACE0();

        // Prepare query parameters
        kurlyk::QueryParams query = {
            {"id", user_id},
            {"hash", user_hash},
        };

        // Send POST request
        auto future = get_http_client().post(
            "/auth",
            query,
            {
                {"Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.9"},
                {"Content-Type", "application/x-www-form-urlencoded"},
                {"Upgrade-Insecure-Requests", "1"},
                {"Cookie", cookies}
            },
            std::string(),
            get_rate_limit(RateLimitType::ACCOUNT_INFO)
        );

        // Define callback for handling HTTP response
        auto callback = [this, result_callback](
                kurlyk::HttpResponsePtr response) {
            if (!validate_response(response, [&result_callback](const std::string& error_text){
                    result_callback(false, error_text);
                })) {
                return;
            }

            result_callback(true, std::string());
        };

        add_http_request_task(std::move(future), std::move(callback));
    }

    void RequestManager::request_balance(
            std::function<void(
                bool success,
                double balance,
                CurrencyType currency)> balance_callback) {
        LOGIT_TRACE0();

        // Prepare query parameters
        kurlyk::QueryParams query = {
            {"user_id", m_user_id},
            {"user_hash", m_user_hash},
        };

        // Send POST request
        auto future = get_http_client().post(
            "/balance.php",
            kurlyk::QueryParams(),
            m_api_headers,
            kurlyk::utils::to_query_string(query),
            get_rate_limit(RateLimitType::BALANCE)
        );

        auto callback = [this, balance_callback](kurlyk::HttpResponsePtr response) {
            if (!validate_response(response)) {
                balance_callback(false, 0.0, CurrencyType::UNKNOWN);
                return;
            }

            auto balance_result = parse_balance(response->content);
            if (!balance_result) {
                LOGIT_ERROR("Failed to parse balance.");
                balance_callback(false, 0.0, CurrencyType::UNKNOWN);
                return;
            }

            const auto [balance, currency] = *balance_result;
            balance_callback(true, balance, currency);
        };

        add_http_request_task(std::move(future), std::move(callback));
    }

    void RequestManager::request_switch_account_type(
            std::function<void(bool success)> switch_callback) {
        LOGIT_TRACE0();

        // Prepare query parameters
        kurlyk::QueryParams query = {
            {"user_id", m_user_id},
            {"user_hash", m_user_hash},
        };

        // Send POST request
        auto future = get_http_client().post(
            "/user_real_trade.php",
            kurlyk::QueryParams(),
            m_api_headers,
            kurlyk::utils::to_query_string(query),
            get_rate_limit(RateLimitType::ACCOUNT_SETTINGS)
        );

        auto callback = [this, switch_callback](kurlyk::HttpResponsePtr response) {
            if (!validate_response(response)) {
                switch_callback(false);
                return;
            }
            bool success = (response->content == "ok");
            if (!success) {
                LOGIT_PRINT_ERROR("Response validation failed: expected 'ok', but received '", response->content, "'.");
            }
            switch_callback(success);
        };

        add_http_request_task(std::move(future), std::move(callback));
    }

    void RequestManager::request_switch_currency(
            std::function<void(bool success)> switch_callback) {
        LOGIT_TRACE0();
    
        // Prepare query parameters
        kurlyk::QueryParams query = {
            {"user_id", m_user_id},
            {"user_hash", m_user_hash},
        };

        // Send POST request
        auto future = get_http_client().post(
            "/user_currency_edit.php",
            kurlyk::QueryParams(),
            m_api_headers,
            kurlyk::utils::to_query_string(query),
            get_rate_limit(RateLimitType::ACCOUNT_SETTINGS)
        );

        auto callback = [this, switch_callback](kurlyk::HttpResponsePtr response) {
            if (!validate_response(response)) {
                switch_callback(false);
                return;
            }
            bool success = (response->content == "ok");
            if (!success) {
                LOGIT_PRINT_ERROR("Response validation failed: expected 'ok', but received '", response->content, "'.");
            }
            switch_callback(success);
        };

        add_http_request_task(std::move(future), std::move(callback));
    }
    
    void RequestManager::request_find_working_domain(
            std::function<void(
                bool success,
                std::string& host)> find_callback) {
        LOGIT_TRACE0();
        
        const int min_index = m_domain_index_min;
        const int max_index = m_domain_index_max;
        int total_requests = max_index - min_index + 1 + (min_index > 0 ? 1 : 0);  // include index 0 if min > 0

        struct DomainCheckState {
            int total_requests = 0;
            int completed_requests = 0;
            std::vector<int> successful_indices;
            std::function<void(bool, std::string&)> on_complete;
        };

        auto state = std::make_shared<DomainCheckState>();
        state->total_requests = total_requests;
        state->on_complete = std::move(find_callback);

        auto& client = get_http_client();
        client.set_head_only(true);
        client.set_retry_attempts(3, time_shield::MS_PER_SEC);
        client.set_timeout(5);
        client.set_connect_timeout(5);

        for (int i = 0; i <= max_index; ++i) {
            if (i == 1 && i < min_index) i = min_index; // skip to min_index if i == 1 and < min
            
            std::string host = (i == 0)
                ? "https://intrade.bar"
                : "https://intrade" + std::to_string(i) + ".bar";

            client.set_host(host);
            auto future = client.get("/", {}, {});

            auto callback = [this, state, i](kurlyk::HttpResponsePtr response) {
                if (!response->ready) return;

                if (response->status_code == 200) {
                    state->successful_indices.push_back(i);
                }

                int finished = ++state->completed_requests;
                if (finished == state->total_requests) {
                    bool success = false;
                    std::string selected_host;

                    if (!state->successful_indices.empty()) {
                        int min_index = *std::min_element(
                            state->successful_indices.begin(), state->successful_indices.end());
                        selected_host = (min_index == 0)
                            ? "https://intrade.bar"
                            : "https://intrade" + std::to_string(min_index) + ".bar";
                        success = true;
                    }

                    LOGIT_PRINT_INFO("Auto-selected domain:", selected_host, "; success:", success);

                    if (success) {
                        get_http_client().set_host(selected_host);
                        get_http_client().set_origin(selected_host);
                    }
                    
                    state->on_complete(success, selected_host);
                }
            };
            
            add_http_request_task(std::move(future), std::move(callback));
        }
        
        client.set_head_only(false);
        client.set_retry_attempts(10, time_shield::MS_PER_SEC);
        client.set_timeout(30);
        client.set_connect_timeout(15);
    }
    
    /// \brief Checks if the currently set host in the HTTP client is available.
    /// \param callback Callback that receives the result: true if available, false otherwise.
    void RequestManager::request_check_current_host_available(
            std::function<void(bool)> check_callback) {
        LOGIT_TRACE0();

        auto& client = get_http_client();
        client.set_head_only(true); // Use HEAD to avoid downloading body
        client.set_retry_attempts(3, time_shield::MS_PER_SEC);
        client.set_timeout(5);
        client.set_connect_timeout(5);

        auto future = client.get("/", {}, {});
        auto cb = [check_callback = std::move(check_callback)](kurlyk::HttpResponsePtr response) {
            if (!response->ready) return;
            bool success = response->status_code == 200;
            LOGIT_PRINT_DEBUG("Current host ping check:",
                " success =", success,
                "; status =", response->status_code);
            check_callback(success);
        };

        add_http_request_task(std::move(future), std::move(cb));
        client.set_head_only(false); // Restore to default after request
        client.set_retry_attempts(10, time_shield::MS_PER_SEC);
        client.set_timeout(30);
        client.set_connect_timeout(15);
    }

    void RequestManager::request_profile(
            std::function<void(
                bool success,
                CurrencyType currency,
                AccountType account)> profile_callback) {
        LOGIT_TRACE0();

        // Send POST request
        auto future = get_http_client().get(
            "/profile",
            kurlyk::QueryParams(),
            {
                {"Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0"},
                {"Upgrade-Insecure-Requests", "1"},
                {"Cookie", m_cookies}
            },
            get_rate_limit(RateLimitType::ACCOUNT_INFO)
        );

        auto callback = [this, profile_callback](kurlyk::HttpResponsePtr response) {
            if (!validate_response(response)) {
                profile_callback(false, CurrencyType::UNKNOWN, AccountType::UNKNOWN);
                return;
            }

            auto [currency, account] = parse_profile_response(response->content);
            if (currency == CurrencyType::UNKNOWN || account == AccountType::UNKNOWN) {
                //LOGIT_PRINT_ERROR("Response validation failed: expected 'ok', but received '", response->content, "'.");
#               ifdef OPTIONX_LOG_UNIQUE_FILE_INDEX
                const int log_index = OPTIONX_LOG_UNIQUE_FILE_INDEX;
                LOGIT_STREAM_ERROR_TO(log_index) << response->content;
                LOGIT_PRINT_ERROR(
                    "Response validation failed: currency=",currency,";account=",account,"; Content log was written to file: ",
                    LOGIT_GET_LAST_FILE_NAME(log_index)
                );
#               else
                LOGIT_PRINT_ERROR("Response validation failed: currency=", currency, ";account=", account);
#               endif
                profile_callback(false, currency, account);
                return;
            }

            LOGIT_TRACE(currency, account);
            profile_callback(true, currency, account);
        };

        add_http_request_task(std::move(future), std::move(callback));
    }

    void RequestManager::request_price(
            std::function<void(
                bool success,
                std::vector<TickData> ticks)> price_callback) {
        // Отправка GET-запроса
        auto future = get_http_client().get(
            "/price_now",
            kurlyk::QueryParams(),
            {{"Accept", "*/*"}, {"Cookie", m_cookies}},
            get_rate_limit(RateLimitType::TICK_DATA)
        );

        // Коллбэк для обработки ответа
        auto callback = [this, price_callback](kurlyk::HttpResponsePtr response) {
            if (!validate_response(response)) {
                price_callback(false, {});
                return;
            }

            using json = nlohmann::json;
            int64_t received_ms = OPTIONX_TIMESTAMP_MS;
            std::vector<TickData> ticks;
            try {
                json j = json::parse(response->content); // Парсинг JSON
                for (auto& el : j.items()) {
                    const std::string symbol_name = el.key();
                    TickData tick;
                    tick.provider = to_str(PlatformType::INTRADE_BAR);
                    tick.symbol = normalize_symbol_name(symbol_name);
                    tick.volume_digits = 0;

                    if (tick.symbol.substr(3, 3) == "JPY") {
                        tick.price_digits = 3;
                    } else {
                        tick.price_digits = 5;
                    }

                    tick.tick.ask = el.value()["ask"];
                    tick.tick.bid = el.value()["bid"];
                    tick.tick.time_ms = el.value()["Updates"];
                    tick.tick.time_ms = time_shield::sec_to_ms(tick.tick.time_ms);
                    tick.tick.received_ms = received_ms;
                    tick.tick.set_flag(TickUpdateFlags::NONE);
                    tick.set_flag(TickStatusFlags::INITIALIZED);
                    tick.set_flag(TickStatusFlags::REALTIME);
                    ticks.push_back(std::move(tick));
                }

                price_callback(true, std::move(ticks));
            } catch (const std::exception& ex) {
                LOGIT_ERROR("Error parsing price response: ", ex.what());
                price_callback(false, {});
            }
        };

        // Добавление задачи HTTP-запроса
        add_http_request_task(std::move(future), std::move(callback));
    }

    void RequestManager::request_trade_check(
            int64_t deal_id,
            int retry_attempts,
            std::function<void(
                bool success,
                double price,
                double profit)> callback_check) {

        // Prepare query parameters
        kurlyk::QueryParams query = {
            {"user_id", m_user_id},
            {"user_hash", m_user_hash},
            {"trade_id", std::to_string(deal_id)}
        };

        // Send POST request
        auto future = get_http_client().post(
            "/trade_check2.php",
            kurlyk::QueryParams(), // No additional URL parameters
            m_api_headers,         // Headers
            kurlyk::utils::to_query_string(query),
            get_rate_limit(RateLimitType::TRADE_RESULT)
        );

        auto callback = [this, deal_id, callback_check, retry_attempts](kurlyk::HttpResponsePtr response) {
            if (!validate_response(response)) {
                callback_check(false, 0.0, 0.0);
                return;
            }

            const std::string& content = response->content;

            // Retry if the response is empty and retry attempts remain
            if (content.empty() && retry_attempts > 0) {
                LOGIT_0ERROR();
                request_trade_check(deal_id, retry_attempts - 1, callback_check);
                return;
            }

            try {
                // Check for error in the response
                if (content.find("error") != std::string::npos) {
                    // Retry if the response contains 'error' and retry attempts remain
                    if (retry_attempts > 0) {
                        LOGIT_0ERROR();
                        request_trade_check(deal_id, retry_attempts - 1, callback_check);
                        return;
                    }
#                   ifdef OPTIONX_LOG_UNIQUE_FILE_INDEX
                    const int log_index = OPTIONX_LOG_UNIQUE_FILE_INDEX;
                    LOGIT_STREAM_ERROR_TO(log_index) << content;
                    LOGIT_PRINT_ERROR(
                        "Trade check failed. Response contains 'error'. Content log was written to file: ",
                        LOGIT_GET_LAST_FILE_NAME(log_index)
                    );
#                   else
                    LOGIT_PRINT_ERROR("Trade check failed. Response contains 'error'.");
#                   endif
                    callback_check(false, 0.0, 0.0);
                    return;
                }

                // Parse the response for price and profit
                size_t separator_pos = content.find(';');
                if (separator_pos == std::string::npos) {
#                   ifdef OPTIONX_LOG_UNIQUE_FILE_INDEX
                    const int log_index = OPTIONX_LOG_UNIQUE_FILE_INDEX;
                    LOGIT_STREAM_ERROR_TO(log_index) << content;
                    LOGIT_PRINT_ERROR(
                        "Trade check failed. Invalid response format. Content log was written to file: ",
                        LOGIT_GET_LAST_FILE_NAME(log_index)
                    );
#                   else
                    LOGIT_PRINT_ERROR("Trade check failed. Invalid response format.");
#                   endif
                    callback_check(false, 0.0, 0.0);
                    return;
                }

                double price = std::stod(content.substr(0, separator_pos));
                double profit = std::stod(content.substr(separator_pos + 1));
                callback_check(true, price, profit);
            } catch (const std::exception& ex) {
                LOGIT_ERROR("Error parsing trade check response: ", ex.what());
                callback_check(false, 0.0, 0.0);
            }
        };

        // Add the HTTP request task with the response handler
        add_http_request_task(std::move(future), std::move(callback));
    }

    void RequestManager::request_execute_trade(
            std::shared_ptr<TradeRequest> request,
            std::function<void(
                bool success,
                int64_t option_id,
                int64_t open_date,
                double open_price,
                const std::string& error_desc)> result_callback) {

        if (!request) {
            LOGIT_ERROR("TradeRequest is null.");
            result_callback(false, 0, 0, 0, "TradeRequest is null.");
            return;
        }

        kurlyk::QueryParams query;

        // Populate query parameters
        query.emplace("user_id", m_user_id);
        query.emplace("user_hash", m_user_hash);
        query.emplace("option", request->symbol);
        query.emplace("investment", std::to_string(request->amount));

        if (request->option_type == OptionType::SPRINT) {
            query.emplace("time", std::to_string(request->duration / time_shield::SEC_PER_MIN));
            query.emplace("date", "0");
            query.emplace("trade_type", "sprint");
        } else
        if (request->option_type == OptionType::CLASSIC) {
            const int64_t zone_offset = 3 * time_shield::SEC_PER_HOUR;
            query.emplace("time", time_shield::to_string("%hh:%mm", request->expiry_time + zone_offset));
            query.emplace("date", time_shield::to_string("%DD-%MM-%YYYY", request->expiry_time + zone_offset));
            query.emplace("trade_type", "classic");
        }

        query.emplace("status", (request->order_type == OrderType::BUY) ? "1" : "2");

        // Send POST request
        auto future = get_http_client().post(
            "/ajax5_new.php",
            kurlyk::QueryParams(),  // No additional URL parameters
            m_api_headers,          // Headers
            kurlyk::utils::to_query_string(query),  // Body parameters
            get_rate_limit(RateLimitType::TRADE_EXECUTION)
        );

        // Define the response handler
        auto callback = [this, result_callback](kurlyk::HttpResponsePtr response) {
            if (!validate_response(response)) {
                LOGIT_ERROR("Invalid response received for trade open request.");
                result_callback(false, 0, 0, 0, "Trade open failed due to invalid response.");
                return;
            }
            parse_execute_trade(response->content, std::move(result_callback));
        };

        // Add the HTTP request task
        add_http_request_task(std::move(future), std::move(callback));
    }

} // namespace optionx::platforms::intrade_bar

#endif // _OPTIONX_PLATFORMS_INTRADERBAR_REQUEST_MANAGER_HPP_INCLUDED
