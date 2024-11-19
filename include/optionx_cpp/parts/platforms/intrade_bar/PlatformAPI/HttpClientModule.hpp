#pragma once
#ifndef _OPTIONX_PLATFORMS_INTRADERBAR_HTTP_CLIENT_MODULE_HPP_INCLUDED
#define _OPTIONX_PLATFORMS_INTRADERBAR_HTTP_CLIENT_MODULE_HPP_INCLUDED

/// \file HttpClientModule.hpp
/// \brief

#include "../../../modules/HttpClientModule.hpp"
#include "../AuthData.hpp"
#include "../AccountInfoData.hpp"
#include "HttpClientModule/HttpUtils.hpp"
#include "HttpClientModule/StringParsers.hpp"
#include <kurlyk.hpp>
#include <log-it/LogIt.hpp>

namespace optionx {
namespace platforms {
namespace intrade_bar {

    class HttpClientModule final : public modules::HttpClientModule {
    public:

        /// \brief Constructor initializing the HTTP client module with an event hub.
        /// \param hub Reference to the event hub for event handling.
        /// \param account_info Shared pointer to account information data.
        explicit HttpClientModule(EventHub& hub, std::shared_ptr<IAccountInfoData> account_info)
            : modules::HttpClientModule(hub, std::move(account_info)) {
            setup();
        }

        /// \brief Default virtual destructor.
        virtual ~HttpClientModule() = default;

    private:
        kurlyk::Headers             m_http_headers;
        kurlyk::Headers             m_api_headers;
        kurlyk::Headers             m_profile_headers;
        std::unique_ptr<AuthData>   m_auth_data;
        std::string                 m_user_id;
        std::string                 m_user_hash;
        std::string                 m_cookies;

        /// \brief Handles the account information request event.
        /// \param event The account information request event.
        void handle_event(const modules::AccountInfoRequestEvent& event) override final {

        }

        /// \brief Handles the balance request event.
        /// \param event The balance request event.
        void handle_event(const modules::BalanceRequestEvent& event) override final {

        }

        /// \brief Handles the trade request event.
        /// \param event The trade request event.
        void handle_event(const modules::TradeRequestEvent& event) override final {

        }

        /// \brief Handles the trade status update event.
        /// \param event The trade status update event.
        void handle_event(const modules::TradeStatusEvent& event) override final {

        }

        /// \brief Handles the authorization data event.
        /// \param event The authorization data event.
        void handle_event(const modules::AuthDataEvent& event) override final {
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

        /// \brief Handles the connection request event by processing authentication details.
        /// \param event The connection request event, including a callback to process the connection result.
        void handle_event(const modules::ConnectRequestEvent& event) override final;

        void process_additional_logic() override final {

        }

        /// \brief Initializes rate limits.
        void initialize_rate_limits() override final {
            using RateLimitType = modules::HttpClientModule::RateLimitType;
            set_rate_limit_rpm(RateLimitType::GENERAL, 60);
            set_rate_limit_rps(RateLimitType::OPEN_TRADE, 1);
            set_rate_limit_rps(RateLimitType::TRADE_RESULT, 1);
            set_rate_limit_rps(RateLimitType::BALANCE, 5);
            set_rate_limit_rpm(RateLimitType::ACCOUNT_INFO, 6);
            set_rate_limit_rpm(RateLimitType::ACCOUNT_SETTINGS, 12);
        }

        /// \brief Initializes HTTP client.
        void initialize_client() override final {
            kurlyk::Headers api_headers = {
                {"Accept", "*/*"},
                {"Content-Type", "application/x-www-form-urlencoded; charset=UTF-8"},
                {"X-Requested-With", "XMLHttpRequest"}
                //{"Cookie", m_cookies}
            };
            m_api_headers = std::move(api_headers);

            kurlyk::Headers profile_headers = {
                {"Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0"},
                {"Upgrade-Insecure-Requests", "1"}
                //{"Cookie", m_cookies}
            };
            m_profile_headers = std::move(profile_headers);

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
            modules::HttpClientModule::m_client.set_follow_location(true);
            modules::HttpClientModule::m_client.set_max_redirects(10);
            using RateLimitType = modules::HttpClientModule::RateLimitType;
            modules::HttpClientModule::m_client.assign_rate_limit_id(get_rate_limit(RateLimitType::GENERAL), kurlyk::RateLimitType::General);
        }

        void validate_user_token(
                std::shared_ptr<AuthData> auth_data,
                modules::ConnectRequestEvent::callback_t connect_callback) {
        }

        void request_main_page(
                std::shared_ptr<AuthData> auth_data,
                modules::ConnectRequestEvent::callback_t connect_callback);


        void request_login(
                const std::string& req_id,
                const std::string& req_value,
                const std::string& cookies,
                std::shared_ptr<AuthData> auth_data,
                modules::ConnectRequestEvent::callback_t connect_callback);

        void request_auth(
                const std::string& user_id,
                const std::string& user_hash,
                const std::string& cookies,
                std::shared_ptr<AuthData> auth_data,
                modules::ConnectRequestEvent::callback_t connect_callback);

        void start_authentication_flow(
			std::shared_ptr<AuthData> auth_data,
			modules::ConnectRequestEvent::callback_t connect_callback);

        void handle_account_type_switch(
                    std::shared_ptr<AuthData> auth_data,
                    AccountType account_type,
                    CurrencyType currency,
                    modules::ConnectRequestEvent::callback_t connect_callback);

        void handle_currency_switch(
                    std::shared_ptr<AuthData> auth_data,
                    CurrencyType currency,
                    modules::ConnectRequestEvent::callback_t connect_callback);

        void finalize_authentication(
                std::shared_ptr<AuthData> auth_data,
                modules::ConnectRequestEvent::callback_t connect_callback);

        void request_balance(
                    std::function<void(
                        bool success,
                        double balance,
                        CurrencyType currency)> balance_callback);

        void request_switch_account_type(
                    std::function<void(bool success)> switch_callback);

        void request_switch_currency(
                    std::function<void(bool success)> switch_callback);

        void request_profile(
                    std::function<void(
                        bool success,
                        CurrencyType currency,
                        AccountType account)> profile_callback);

        /// \brief Retrieves the current account information as a shared pointer.
        /// \return Shared pointer to the account information data.
        /// \throws std::runtime_error if the account information cannot be cast to the expected type.
        std::shared_ptr<AccountInfoData> get_account_info();

    }; // HttpClientModule

//------------------------------------------------------------------------------

    void HttpClientModule::handle_event(const modules::ConnectRequestEvent& event) {
        LOGIT_TRACE0();
        if (!m_auth_data) {
            LOGIT_ERROR("Authentication data is missing.");
            event.callback(false, "Authentication data is not available.", nullptr);
            return;
        }

        auto account_info = get_account_info();
        if (account_info->connect) {
            account_info->connect = false;
            modules::AccountInfoUpdateEvent event(account_info);
            notify(event);
        }

        using AuthMethod = AuthData::AuthMethod;

        switch (m_auth_data->auth_method) {
        case AuthMethod::NONE:
            LOGIT_ERROR("Authentication method is not specified.");
            event.callback(false, "Authentication method is not specified.", m_auth_data->clone_unique());
            break;
        case AuthMethod::EMAIL_PASSWORD:
            // Process authentication via email and password
            request_main_page(std::make_shared<AuthData>(*m_auth_data.get()), event.callback);
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
            //m_cookies = "user_id=" + user_id + "; user_hash=" + user_hash + "; " + cookies;
            m_cookies = "user_id=" + user_id + "; user_hash=" + user_hash;
            LOGIT_PRINT_TRACE("User ID: ", m_user_id);

            // Start authentication flow
            start_authentication_flow(auth_data, connect_callback);
        };

        add_http_request_task(std::move(future), std::move(callback));
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
                connect_callback(false, "Failed to retrieve balance.", auth_data->clone_unique());
                LOGIT_ERROR("Failed to retrieve balance.");
                return;
            }

            try {
                auto account_info = get_account_info();
                account_info->connect = true;
                connect_callback(true, std::string(), auth_data->clone_unique());
                modules::AccountInfoUpdateEvent event(account_info);
                notify(event);
                LOGIT_TRACE("Authentication successful. Connection established.");
            } catch (const std::exception& ex) {
                LOGIT_ERROR("Exception during account info update: ", ex.what());
                connect_callback(false, "Error updating account information.", auth_data->clone_unique());
            }
        });
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
        // Prepare query parameters
        /*
        kurlyk::QueryParams query = {
            {"user_id", m_user_id},
            {"user_hash", m_user_hash},
        };
        */

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

#endif // _OPTIONX_PLATFORMS_INTRADERBAR_HTTP_CLIENT_MODULE_HPP_INCLUDED
