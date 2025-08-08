#pragma once
#ifndef _OPTIONX_PLATFORMS_INTRADERBAR_AUTH_DATA_HPP_INCLUDED
#define _OPTIONX_PLATFORMS_INTRADERBAR_AUTH_DATA_HPP_INCLUDED

/// \file AuthData.hpp
/// \brief Contains the AuthData class for Intrade Bar platform authorization.

namespace optionx::platforms::intrade_bar {

    /// \class AuthData
    /// \brief Represents authorization data for the Intrade Bar platform.
    class AuthData : public IAuthData {
    public:
        using json = nlohmann::json;

        AuthData() :
            user_agent("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/133.0.0.0 Safari/537.36"),
            accept_language("ru,ru-RU;q=0.9,en;q=0.8,en-US;q=0.7"),
            host("https://intrade.bar"),
            proxy_type(kurlyk::ProxyType::PROXY_HTTP) {
        }

        virtual ~AuthData() = default;

        /// \enum AuthMethod
        /// \brief Specifies the authentication method used.
        enum class AuthMethod {
            NONE,           ///< No authentication method specified.
            EMAIL_PASSWORD, ///< Authorization via email and password.
            USER_TOKEN      ///< Authorization via username and token.
        };

        /// \brief Sets authorization data using email and password.
        /// \param email Email address.
        /// \param password Password.
        void set_email_password(const std::string& email, const std::string& password) {
            this->email = email;
            this->password = password;
            auth_method = AuthMethod::EMAIL_PASSWORD;
        }

        /// \brief Sets authorization data using username and token.
        /// \param user_id User ID.
        /// \param token Token.
        void set_user_token(const std::string& user_id, const std::string& token) {
            this->user_id = user_id;
            this->token = token;
            auth_method = AuthMethod::USER_TOKEN;
        }

        /// \brief Sets the proxy server address.
        /// \param ip Proxy server IP address.
        /// \param port Proxy server port.
        void set_proxy(const std::string& ip, int port) {
            proxy_server = ip + ":" + std::to_string(port);
        }

        /// \brief Sets the proxy server address with authentication details.
        /// \param ip Proxy server IP address.
        /// \param port Proxy server port.
        /// \param username Proxy username.
        /// \param password Proxy password.
        /// \param type Proxy type.
        void set_proxy(
                const std::string& ip,
                int port,
                const std::string& username,
                const std::string& password,
                kurlyk::ProxyType type = kurlyk::ProxyType::PROXY_HTTP) {
            set_proxy(ip, port);
            set_proxy_auth(username, password);
            proxy_type = type;
        }

        /// \brief Sets proxy authentication credentials.
        /// \param username Proxy username.
        /// \param password Proxy password.
        void set_proxy_auth(
                const std::string& username,
                const std::string& password) {
            proxy_auth = username + ":" + password;
        }

        /// \brief Serializes authorization data to a JSON object.
        /// \param j JSON object to populate.
        /// \return True if serialization was successful, false otherwise.
        void to_json(nlohmann::json& j) const override {
            try {
                if (auth_method == AuthMethod::EMAIL_PASSWORD) {
                    j["email"] = email;
                    j["password"] = password;
                } else
                if (auth_method == AuthMethod::USER_TOKEN) {
                    j["user_id"] = user_id;
                    j["token"] = token;
                } else {
                    return;
                }
                j["host"] = host;
                j["user_agent"] = user_agent;
                j["accept_language"] = accept_language;
                j["proxy_server"] = proxy_server;
                j["proxy_auth"] = proxy_auth;
                j["proxy_type"] = optionx::to_str(proxy_type);
                j["account_type"] = optionx::to_str(account_type);
                j["currency"] = optionx::to_str(currency);
                j["auto_find_domain"] = auto_find_domain;
                j["domain_index_min"] = domain_index_min;
                j["domain_index_max"] = domain_index_max;
            } catch (const std::exception& ex) {
                LOGIT_ERROR(ex);
            }
        }

        /// \brief Deserializes JSON data into authorization data.
        /// \param j JSON object to parse.
        /// \return True if deserialization was successful, false otherwise.
        void from_json(const nlohmann::json& j) override {
            try {
                if (j.contains("email") && j.contains("password")) {
                    email = j.at("email").get<std::string>();
                    password = j.at("password").get<std::string>();
                    auth_method = AuthMethod::EMAIL_PASSWORD;
                } else if (j.contains("user_id") && j.contains("token")) {
                    user_id = j.at("user_id").get<std::string>();
                    token = j.at("token").get<std::string>();
                    auth_method = AuthMethod::USER_TOKEN;
                } else {
                    auth_method = AuthMethod::NONE;
                    return;
                }
                host = j.value("host", host);
                user_agent = j.value("user_agent", user_agent);
                accept_language = j.value("accept_language", accept_language);
                proxy_server = j.value("proxy_server", proxy_server);
                proxy_auth   = j.value("proxy_auth", proxy_auth);
                proxy_type   = to_enum<kurlyk::ProxyType>(j.value("proxy_type", "HTTP"));
                account_type = to_enum<AccountType>(j.value("account_type", "UNKNOWN"));
                currency = to_enum<CurrencyType>(j.value("currency", "UNKNOWN"));
                auto_find_domain = j.value("auto_find_domain", auto_find_domain);
                domain_index_min = j.value("domain_index_min", domain_index_min);
                domain_index_max = j.value("domain_index_max", domain_index_max);
            } catch (const std::exception& ex) {
                LOGIT_ERROR(ex);
            }
        }

        /// \brief Validates the authorization data with detailed error message.
        /// \return A pair where the first element is true if data is valid, and the second element contains an error message in case of failure.
        std::pair<bool, std::string> validate() const override {
            // If auto domain discovery is disabled, host must be explicitly specified
            if (!auto_find_domain && host.empty()) {
                return { false, "Host is empty and auto_find_domain is disabled" };
            }

            // If auto domain discovery is enabled, the index range must be valid
            if (auto_find_domain && domain_index_min > domain_index_max) {
                return { false, "Invalid domain index range: min > max" };
            }

            // Validate required fields based on selected authentication method
            if (auth_method == AuthMethod::EMAIL_PASSWORD) {
                if (email.empty())
                    return { false, "Email is empty" };
                if (password.empty())
                    return { false, "Password is empty" };
            } else 
            if (auth_method == AuthMethod::USER_TOKEN) {
                if (user_id.empty())
                    return { false, "User ID is empty" };
                if (token.empty())
                    return { false, "Token is empty" };
            } else {
                return { false, "Authentication method is not set" };
            }
            return { true, std::string() };
        }

        /// \brief Clones the authorization data instance to a unique pointer.
        /// \return Unique pointer to a cloned IAuthData instance.
        std::unique_ptr<IAuthData> clone_unique() const override {
            return std::make_unique<AuthData>(*this);
        }

        /// \brief Clones the authorization data instance to a shared pointer.
        /// \return Shared pointer to a cloned IAuthData instance.
        std::shared_ptr<IAuthData> clone_shared() const override {
            return std::make_shared<AuthData>(*this);
        }

        /// \brief Returns the platform type.
        /// \return Platform type identifier (`PlatformType::INTRADE_BAR`).
        PlatformType platform_type() const override {
            return PlatformType::INTRADE_BAR;
        }

        AccountType account_type = AccountType::UNKNOWN;  ///< Account type, if supported.
        CurrencyType currency    = CurrencyType::UNKNOWN; ///< Account currency, if supported.
        std::string email;                         ///< Email for email/password authentication.
        std::string password;                      ///< Password for email/password authentication.
        std::string user_id;                       ///< Username for user/token authentication.
        std::string token;                         ///< Token for user/token authentication.
        std::string user_agent;                    ///< User agent string for HTTP requests.
        std::string accept_language;               ///< Accepted languages for HTTP requests.
        std::string host;                          ///< Host URL for the Intrade Bar platform.
        std::string proxy_server;                  ///< Proxy address in <ip:port> format.
        std::string proxy_auth;                    ///< Proxy authentication in <username:password> format.
        kurlyk::ProxyType proxy_type;              ///< Proxy type (e.g., HTTP, SOCKS).
        AuthMethod auth_method = AuthMethod::NONE; ///< Authentication method used.
        bool auto_find_domain  = false;            ///< Whether to perform automatic domain discovery.
        int domain_index_min   = 0;                ///< Minimum domain index to scan (0 = intrade.bar).
        int domain_index_max   = 1000;             ///< Maximum domain index to scan (e.g., intrade1000.bar).
    }; // AuthData

} // namespace optionx::platforms::intrade_bar

#endif // _OPTIONX_PLATFORMS_INTRADERBAR_AUTH_DATA_HPP_INCLUDED
