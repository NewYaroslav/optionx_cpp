namespace optionx {
namespace platforms {
namespace intrade_bar {

    void HttpClientModule::request_main_page(
            std::shared_ptr<AuthData> auth_data,
            modules::ConnectRequestEvent::callback_t connect_callback) {
        LOGIT_TRACE0();
        // Validate email and password
        if (auth_data->email.empty() || auth_data->password.empty()) {
            LOGIT_ERROR_IF(auth_data->email.empty(), "Email is missing.");
            LOGIT_ERROR_IF(auth_data->password.empty(), "Password is missing.");
            connect_callback(false, "Email or password is missing.", auth_data->clone_unique());
            return;
        }

        // Prepare the HTTP GET request
        auto future = m_client.get(
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
        auto callback = [this, auth_data, connect_callback](kurlyk::HttpResponsePtr response) {
            // Validate the response
            if (!validate_response(response, connect_callback, auth_data)) {
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
                connect_callback(false, "Failed to process server response.", auth_data->clone_unique());
                return;
            }
            // Extract parsed data
            const auto& [req_id, req_value, cookies] = *parsed_data;
            // Proceed with login request
            request_login(req_id, req_value, cookies, auth_data, connect_callback);
        };

        // Add the HTTP request task for asynchronous processing
        LOGIT_TRACE0();
        add_http_request_task(std::move(future), std::move(callback));
    }

    void HttpClientModule::request_login(
            const std::string& req_id,
            const std::string& req_value,
            const std::string& cookies,
            std::shared_ptr<AuthData> auth_data,
            modules::ConnectRequestEvent::callback_t connect_callback) {
        LOGIT_TRACE(req_id, req_value, cookies);
        // Prepare query parameters
        kurlyk::QueryParams query = {
            {"email", auth_data->email},
            {"password", auth_data->password},
        };
        if (!req_id.empty() && !req_value.empty()) {
            query.emplace(req_id, req_value);
        }

        // Prepare HTTP GET request
        auto future = m_client.post(
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
        auto callback = [this, cookies, auth_data, connect_callback](kurlyk::HttpResponsePtr response) {
            if (!validate_response(response, connect_callback, auth_data)) {
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
                connect_callback(false, "Failed to parse login.", auth_data->clone_unique());
                return;
            }
            const auto [user_id, user_hash] = *login_result;
            // Proceed with authentication request
            request_auth(user_id, user_hash, cookies, auth_data, connect_callback);
        };

        // Add the task to handle the HTTP request
        add_http_request_task(std::move(future), std::move(callback));
    }

    void HttpClientModule::request_auth(
            const std::string& user_id,
            const std::string& user_hash,
            const std::string& cookies,
            std::shared_ptr<AuthData> auth_data,
            modules::ConnectRequestEvent::callback_t connect_callback) {
        LOGIT_TRACE0();
        // Prepare query parameters
        kurlyk::QueryParams query = {
            {"id", user_id},
            {"hash", user_hash},
        };

        // Send POST request
        auto future = m_client.post(
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
        auto callback = [this, user_id, user_hash, cookies, auth_data, connect_callback](kurlyk::HttpResponsePtr response) {
            if (!validate_response(response, connect_callback, auth_data)) {
                return;
            }

            m_user_id = user_id;
            m_user_hash = user_hash;
            m_cookies = "user_id=" + user_id + "; user_hash=" + user_hash;
            storage::ServiceSessionDB::get_instance().set_session_value(to_str(ApiType::INTRADE_BAR), auth_data->email, m_cookies);
            LOGIT_PRINT_TRACE("User ID: ", m_user_id);

            // Start authentication flow
            start_authentication_flow(auth_data, connect_callback);
        };

        add_http_request_task(std::move(future), std::move(callback));
    }

    void HttpClientModule::request_balance(
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
        auto future = m_client.post(
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

    void HttpClientModule::request_switch_account_type(
            std::function<void(bool success)> switch_callback) {
        LOGIT_TRACE0();
        // Prepare query parameters
        kurlyk::QueryParams query = {
            {"user_id", m_user_id},
            {"user_hash", m_user_hash},
        };

        // Send POST request
        auto future = m_client.post(
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

    void HttpClientModule::request_switch_currency(
            std::function<void(bool success)> switch_callback) {
        LOGIT_TRACE0();
        // Prepare query parameters
        kurlyk::QueryParams query = {
            {"user_id", m_user_id},
            {"user_hash", m_user_hash},
        };

        // Send POST request
        auto future = m_client.post(
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

    void HttpClientModule::request_profile(
            std::function<void(
                bool success,
                CurrencyType currency,
                AccountType account)> profile_callback) {
        LOGIT_TRACE0();

        // Send POST request
        auto future = m_client.get(
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

    void HttpClientModule::request_price(
            std::function<void(bool success, const std::vector<TickInfo>& ticks)> price_callback) {
        LOGIT_TRACE0();

        // Отправка GET-запроса
        auto future = m_client.get(
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
            int64_t receipt_time = OPTIONX_TIMESTAMP_MS;
            std::vector<TickInfo> ticks;
            try {
                json j = json::parse(response->content); // Парсинг JSON
                for (auto& el : j.items()) {
                    const std::string symbol_name = el.key();
                    TickInfo tick;
                    tick.symbol = normalize_symbol_name(symbol_name);

                    if (tick.symbol.substr(3, 3) == "JPY") {
                        tick.digits = 3;
                    }

                    tick.tick.ask = el.value()["ask"];
                    tick.tick.bid = el.value()["bid"];
                    tick.tick.tick_time = el.value()["Updates"];
                    tick.tick.tick_time = time_shield::sec_to_ms(tick.tick.tick_time);
                    tick.tick.receipt_time = receipt_time;
                    tick.enable_real_time();
                    tick.initialize();
                    ticks.push_back(std::move(tick));
                }

                price_callback(true, ticks);
            } catch (const std::exception& ex) {
                LOGIT_ERROR("Error parsing price response: ", ex.what());
                price_callback(false, {});
            }
        };

        // Добавление задачи HTTP-запроса
        add_http_request_task(std::move(future), std::move(callback));
    }

    /// \brief Requests trade check result and parses the response.
    /// \param deal_id The ID of the deal to check.
    /// \param callback A function that takes the following parameters:
    ///        - success: True if the request succeeded and was parsed successfully.
    ///        - price: The trade price.
    ///        - profit: The trade profit.
    /// \param retry_attempts Number of attempts to retry the request if the response is empty.
    void HttpClientModule::request_trade_check(
            int64_t deal_id,
            std::function<void(bool success, double price, double profit)> callback_check,
            int retry_attempts) {

        // Prepare query parameters
        kurlyk::QueryParams query = {
            {"user_id", m_user_id},
            {"user_hash", m_user_hash},
            {"trade_id", std::to_string(deal_id)}
        };

        // Send POST request
        auto future = m_client.post(
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
                request_trade_check(deal_id, callback_check, retry_attempts - 1);
                return;
            }

            try {
                // Check for error in the response
                if (content.find("error") != std::string::npos) {
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

    /// \brief Sends a request to open a trade and processes the response.
    /// \param request A shared pointer to the TradeRequest containing trade details.
    /// \param result A shared pointer to the TradeResult to store the result of the trade.
    /// \param request_callback Callback function invoked with the result of the request:
    ///        - success: Indicates if the trade was successfully opened.
    void HttpClientModule::request_execute_trade(
            std::shared_ptr<TradeRequest> request,
            std::shared_ptr<TradeResult> result,
            std::function<void(bool success)> request_callback) {

        if (!request || !result) {
            LOGIT_ERROR("TradeRequest or TradeResult is null.");
            request_callback(false);
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
        } else if (request->option_type == OptionType::CLASSIC) {
            const int64_t zone_offset = 3 * time_shield::SEC_PER_HOUR;
            query.emplace("time", time_shield::to_string("%hh:%mm", request->expiry_time + zone_offset));
            query.emplace("date", time_shield::to_string("%DD-%MM-%YYYY", request->expiry_time + zone_offset));
            query.emplace("trade_type", "classic");
        }

        query.emplace("status", (request->order_type == OrderType::BUY) ? "1" : "2");

        // Send POST request
        auto future = m_client.post(
            "/ajax5_new.php",
            kurlyk::QueryParams(),  // No additional URL parameters
            m_api_headers,          // Headers
            kurlyk::utils::to_query_string(query),  // Body parameters
            get_rate_limit(RateLimitType::TRADE_EXECUTION)
        );

        // Define the response handler
        auto callback = [this, request, result, request_callback](kurlyk::HttpResponsePtr response) {
            const int64_t timestamp = OPTIONX_TIMESTAMP_MS;

            if (!validate_response(response)) {
                LOGIT_ERROR("Invalid response received for trade open request.");
                result->state = result->current_state = OrderState::OPEN_ERROR;
                result->error_code = OrderErrorCode::PARSING_ERROR;
                result->error_desc = "Trade open failed due to invalid response.";
                result->delay = timestamp - result->send_date;
                result->ping = result->delay / 2;
                result->open_date = timestamp;
                result->close_date = (request->option_type == OptionType::SPRINT)
                                     ? timestamp + time_shield::sec_to_ms(request->duration)
                                     : time_shield::sec_to_ms(request->expiry_time);
                request_callback(false);
                return;
            }

            if (parse_execute_trade(response->content, request, result)) {
                request_callback(true);
            } else {
                request_callback(false);
            }
        };

        // Add the HTTP request task
        add_http_request_task(std::move(future), std::move(callback));
    }

} // namespace intrade_bar
} // namespace platforms
} // namespace optionx
