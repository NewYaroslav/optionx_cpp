#pragma once
#ifndef _OPTIONX_PLATFORMS_TRADEUP_AUTH_DATA_HPP_INCLUDED
#define _OPTIONX_PLATFORMS_TRADEUP_AUTH_DATA_HPP_INCLUDED

/// \file AuthData.hpp
/// \brief Contains the AuthData class for TradeUp platform authorization.

namespace optionx::platforms::tradeup {

    /// \class AuthData
    /// \brief Represents authorization data for the TradeUp platform.
    class AuthData : public IAuthData {
    public:
        AccountType account_type = AccountType::UNKNOWN;  ///< Account type, if supported.
        std::string email;               ///< User login (email)
        std::string password;            ///< Account password
        std::string user_agent;          ///< User agent string
        std::string accept_language;     ///< Accept-Language header
        std::string host;                ///< API host
        std::string proxy_server;                  ///< Proxy address in <ip:port> format.
        std::string proxy_auth;                    ///< Proxy authentication in <username:password> format.
        kurlyk::ProxyType proxy_type;              ///< Proxy type (e.g., HTTP, SOCKS).
        AuthMethod auth_method = AuthMethod::NONE; ///< Authentication method used.

        AuthData()
            : user_agent("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/133.0.0.0 Safari/537.36"),
              accept_language("ru,ru-RU;q=0.9,en;q=0.8,en-US;q=0.7"),
              host("https://tradeup.net"),
              proxy_type(::kurlyk::ProxyType::PROXY_HTTP) {
        }

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

        void to_json(nlohmann::json& j) const override {
            j["email"] = email;
            j["password"] = password;
            j["host"] = host;
            j["user_agent"] = user_agent;
            j["accept_language"] = accept_language;
            j["proxy_server"] = proxy_server;
            j["proxy_auth"] = proxy_auth;
            j["proxy_type"] = optionx::to_str(proxy_type);
            j["account_type"] = optionx::to_str(account_type);
        }

        void from_json(const nlohmann::json& j) override {
            email = j.value("email", "");
            password = j.value("password", "");
            host = j.value("host", host);
            user_agent = j.value("user_agent", user_agent);
            accept_language = j.value("accept_language", accept_language);
            proxy_server = j.value("proxy_server", proxy_server);
            proxy_auth   = j.value("proxy_auth", proxy_auth);
            proxy_type   = to_enum<kurlyk::ProxyType>(j.value("proxy_type", "HTTP"));
            account_type = to_enum<AccountType>(j.value("account_type", "UNKNOWN"));
        }

        std::pair<bool, std::string> validate() const override {
            if (email.empty()) return {false, "Email is empty"};
            if (password.empty()) return {false, "Password is empty"};
            return {true, std::string()};
        }

        std::unique_ptr<IAuthData> clone_unique() const override {
            auto clone = std::make_unique<AuthData>(*this);
            clone->clear_callbacks();
            return clone;
        }

        std::shared_ptr<IAuthData> clone_shared() const override {
            auto clone = std::make_shared<AuthData>(*this);
            clone->clear_callbacks();
            return clone;
        }

        /// \brief Returns the platform type.
        /// \return Platform type identifier (`PlatformType::TRADEUP`).
        PlatformType platform_type() const override {
            return PlatformType::TRADEUP;
        }
    };

} // namespace optionx::platforms::tradeup

#endif // _OPTIONX_PLATFORMS_TRADEUP_AUTH_DATA_HPP_INCLUDED
