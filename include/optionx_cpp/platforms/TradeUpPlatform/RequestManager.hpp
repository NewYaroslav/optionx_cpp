#pragma once
#ifndef OPTIONX_HEADER_PLATFORMS_TRADE_UP_PLATFORM_REQUEST_MANAGER_HPP_INCLUDED
#define OPTIONX_HEADER_PLATFORMS_TRADE_UP_PLATFORM_REQUEST_MANAGER_HPP_INCLUDED

/// \file RequestManager.hpp
/// \brief Handles HTTP requests for user authentication, trade execution, balance updates, and market data retrieval.

namespace optionx::platforms::tradeup {

    /// \class RequestManager
    /// \brief Manages HTTP requests for the TradeUp platform.
    ///
    /// The `RequestManager` class is responsible for handling HTTP-based communication
    /// with the trading platform, including authentication, balance retrieval, price updates,
    /// and trade execution.
    class RequestManager final : public components::BaseComponent {
    public:

        /// \brief Constructs the request manager.
        /// \param platform Reference to the trading platform.
        /// \param client Reference to the HTTP client component.
        explicit RequestManager(
                BaseTradingPlatform& platform,
                HttpClientComponent& client)
                : BaseComponent(platform.event_bus()), m_client(client) {
            platform.register_component(this);
        }

        /// \brief Default destructor.
        virtual ~RequestManager() = default;

        void on_event(const utils::Event* const event) override {}

        /// \brief Cancels all ongoing HTTP requests.
        void cancel_requests() {
            get_http_client().cancel_requests();
        }
        
        /// \brief
        /// \param auth_data Shared pointer to authentication data.
        /// \param token Initial API token.
        bool initialize_session(
                std::shared_ptr<AuthData> auth_data, 
                const std::string &token);
        
        /// \brief Sends login credentials for authentication.
        /// \param auth_data Shared pointer to authentication data.
        /// \param result_callback Callback function to receive login result.
        void request_login(
            std::shared_ptr<AuthData> auth_data,
            std::function<void(
                bool success,
                std::string error_message,
                std::string user_id,
                std::string token,
                std::string affs_id,
                std::string cookies
            )> result_callback);
    
        void request_login_success(
            std::function<void(
                bool success,
                std::string error_message)> result_callback);
                
        void request_session_extension(
            std::function<void(
                bool success,
                std::string error_message,
                std::string ret,
                std::string message,
                std::string token,
                std::string cookies,
                int64_t expire
            )> result_callback);

    private:
        HttpClientComponent& m_client;      ///< Reference to the HTTP client component.
        kurlyk::Headers   m_api_headers; ///< Default API headers.
        std::string       m_host;        ///<
        std::string       m_token;       ///< 

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
    };
    
    // -------------------------------------------
    
    inline bool RequestManager::initialize_session(
            std::shared_ptr<AuthData> auth_data, 
            const std::string &token) {
        if (!token.empty()) {
            LOGIT_DEBUG(utils::redact_secret(token));
            
            auto& client = get_http_client();
            m_host = auth_data->host;
            client.set_host(m_host);
            client.set_origin(m_host);
            client.set_user_agent(auth_data->user_agent);
            client.set_accept_language(auth_data->accept_language);
            client.set_retry_attempts(10, time_shield::MS_PER_SEC);
            client.set_timeout(30);
            client.set_connect_timeout(15);

            const kurlyk::Headers headers = {
                {"Connection", "keep-alive"},
                {"Cache-Control", "no-cache"},
                {"pragma", "no-cache"},
                {"sec-ch-ua", ::kurlyk::utils::convert_user_agent_to_sec_ch_ua(auth_data->user_agent)},
                {"sec-ch-ua-mobile", "?0"},
                {"sec-ch-ua-platform", "Windows"},
                {"sec-fetch-dest", "empty"},
                {"sec-fetch-mode", "cors"},
                {"sec-fetch-site", "same-origin"},
                {"sec-gpc", "1"}
            };
            client.set_headers(headers);
            
            m_api_headers = {
                {"Accept", "application/json, text/plain, */*"},
                {"Content-Type", "application/json"},
                {"ngsw-bypass", "true"}
				{"X-API-TOKEN", token}
            };

            m_token = token;
            return true;
        }
        return false;
    }
    
    inline void RequestManager::request_login(
            std::shared_ptr<AuthData> auth_data,
            std::function<void(
                bool success,
                std::string error_message,
                std::string user_id,
                std::string token,
                std::string affs_id,
                std::string cookies
            )> result_callback) {
        LOGIT_TRACE0();

        // Validate email and password
        if (auth_data->email.empty() ||
            auth_data->password.empty()) {
            LOGIT_ERROR_IF(auth_data->email.empty(), "Email is missing.");
            LOGIT_ERROR_IF(auth_data->password.empty(), "Password is missing.");
            result_callback(false, "Email or password is missing.", {}, {}, {}, {});
            return;
        }

        // Prepare the HTTP POST request
        auto& client = get_http_client();
        m_host = auth_data->host;
        client.set_host(m_host);
        client.set_origin(m_host);
        client.set_user_agent(auth_data->user_agent);
        client.set_accept_language(auth_data->accept_language);
        client.set_retry_attempts(10, time_shield::MS_PER_SEC);
        client.set_timeout(30);
        client.set_connect_timeout(15);

        const kurlyk::Headers headers = {
            {"Connection", "keep-alive"},
            {"Cache-Control", "no-cache"},
            {"pragma", "no-cache"},
            {"sec-ch-ua", ::kurlyk::utils::convert_user_agent_to_sec_ch_ua(auth_data->user_agent)},
            {"sec-ch-ua-mobile", "?0"},
            {"sec-ch-ua-platform", "Windows"},
            {"sec-fetch-dest", "empty"},
            {"sec-fetch-mode", "cors"},
            {"sec-fetch-site", "same-origin"},
            {"sec-gpc", "1"}
        };
        client.set_headers(headers);
        
        const std::string referer(m_host + "/auth/sign-in");
        client.set_referer(referer);
        
        LOGIT_DEBUG(
            auth_data->host,
            auth_data->email, 
            auth_data->user_agent, 
            auth_data->accept_language);
                
        nlohmann::json body = {
            {"password", auth_data->password},
            {"stayLogged", true},
            {"login", auth_data->email}
        };

        auto future = client.post(
            "/trade-api/api/signin",
            kurlyk::QueryParams(),
            m_api_headers,
            body.dump(),
            get_rate_limit(RateLimitType::AUTH)
        );
        
        // Handle the HTTP response
        auto callback = [this, result_callback = std::move(result_callback)](
                kurlyk::HttpResponsePtr response) {
            // Validate the response
            if (!validate_response(response, [&result_callback](std::string error_message){
            result_callback(false, std::move(error_message), {}, {}, {}, {});
                })) {
                return;
            }

            // Parse the response content
            auto parsed_data = parse_signin_response(response->content, response->headers);
            if (!parsed_data) {
                LOGIT_PRINT_ERROR(
                     "Failed to process server response. Status code: ",
                    (response ? response->status_code : -1)
                );
                result_callback(false, "Failed to process server response.", {}, {}, {}, {});
                return;
            }
            // Extract parsed data
            auto [user_id, token, affs_id, cookies] = *parsed_data;

            m_token = token;
            if (!token.empty()) {
                m_api_headers.emplace("X-API-TOKEN", token);
            } else {
                const std::string error_message = "Token is empty — X-API-TOKEN header will not be added.";
                LOGIT_ERROR(error_message);
                result_callback(
                    false, 
                    std::move(error_message), 
                    std::move(user_id), 
                    std::move(token), 
                    std::move(affs_id), 
                    std::move(cookies)
                );
                return;
            }
            
            if (!cookies.empty()) {
                m_api_headers.emplace("Cookie", cookies);
            } else {
                const std::string error_message = "Cookie is empty.";
                LOGIT_ERROR(error_message);
                result_callback(
                    false, 
                    std::move(error_message), 
                    std::move(user_id), 
                    std::move(token), 
                    std::move(affs_id), 
                    std::move(cookies)
                );
                return;
            }

            LOGIT_DEBUG(user_id, utils::redact_secret(token), affs_id, utils::redact_secret(cookies));
            result_callback(
                true, 
                {}, 
                std::move(user_id), 
                std::move(token), 
                std::move(affs_id), 
                std::move(cookies)
            );
        };

        add_http_request_task(std::move(future), std::move(callback));
    }

    inline void RequestManager::request_login_success(
            std::function<void(
                bool success,
                std::string error_message
            )> result_callback) {
        LOGIT_TRACE0();

        // Prepare the HTTP GET request
        auto& client = get_http_client();
        const std::string referer(m_host + "/option/EUR-USD_OTC"); // /auth/sign-in
        client.set_referer(referer);

        auto future = client.get(
            "/trade-api/api/loginSuccess",
            kurlyk::QueryParams(),
            m_api_headers,
            get_rate_limit(RateLimitType::AUTH)
        );

        auto callback = [this, result_callback = std::move(result_callback)](kurlyk::HttpResponsePtr response) {
            if (!validate_response(response, [&result_callback](std::string error_message){
                    result_callback(false, std::move(error_message));
                })) {
                return;
            }
            if (!parse_success_response(response->content)) {
                LOGIT_PRINT_ERROR(
                     "Login success check returned failure. Status code: ",
                    (response ? response->status_code : -1)
                );
                result_callback(false, "Login success check returned failure.");
                return;
            }
            result_callback(true, {});
        };

        add_http_request_task(std::move(future), std::move(callback));
    }
    
    inline void RequestManager::request_session_extension(
            std::function<void(
                bool success,
                std::string error_message,
                std::string ret,
                std::string message,
                std::string token,
                std::string cookies,
                int64_t expire
            )> result_callback) {
        LOGIT_TRACE0();

        // Prepare the HTTP GET request
        auto& client = get_http_client();
        const std::string referer(m_host + "/option/EUR-USD_OTC");
        client.set_referer(referer);

        auto future = client.post(
            "/api/v1/session/extension",
            kurlyk::QueryParams(),
            m_api_headers,
            "{}",
            get_rate_limit(RateLimitType::AUTH)
        );

        auto callback = [this, result_callback = std::move(result_callback)](kurlyk::HttpResponsePtr response) {
            if (!validate_response(response, [&result_callback](std::string error_message){
                    result_callback(false, std::move(error_message), {}, {}, m_token, {}, 0);
                })) {
                return;
            }

            // Parse the response content
            auto parsed_data = parse_session_extension_response(
                m_token, 
                response->content, 
                response->headers);
            if (!parsed_data) {
                LOGIT_PRINT_ERROR(
                     "Failed to process server response. Status code: ",
                    (response ? response->status_code : -1)
                );
                result_callback(false, "Failed to process server response.", {}, {}, m_token, {}, 0);
                return;
            }
            // Extract parsed data
            auto [success, ret, message, cookies, expire] = *parsed_data;
            
            if (success && !cookies.empty()) {
                m_api_headers.insert_or_assign("Cookie", cookies);
            }

            LOGIT_DEBUG(success, ret, message, utils::redact_secret(cookies), expire);

            result_callback(success, {}, std::move(ret), std::move(message), m_token, std::move(cookies), expire);
        };

        add_http_request_task(std::move(future), std::move(callback));
    }

} // namespace optionx::platforms::tradeup

#endif // OPTIONX_HEADER_PLATFORMS_TRADE_UP_PLATFORM_REQUEST_MANAGER_HPP_INCLUDED
