#pragma once
#ifndef _OPTIONX_PLATFORMS_INTRADERBAR_AUTH_DATA_HPP_INCLUDED
#define _OPTIONX_PLATFORMS_INTRADERBAR_AUTH_DATA_HPP_INCLUDED

/// \file AuthData.hpp
/// \brief Contains the AuthData class for Intrade Bar platform authorization.

#include "../../interfaces/IAuthData.hpp"
#include "../../utils/Enums.hpp"
#include <log-it/LogIt.hpp>

namespace optionx {
namespace platforms {
namespace intrade_bar {

    /// \class AuthData
    /// \brief Represents authorization data for the Intrade Bar platform.
    class AuthData : public IAuthData {
    public:
        using json = nlohmann::json;

        AuthData() :
            user_agent("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/129.0.0.0 Safari/537.36"),
            accept_language("ru,ru-RU;q=0.9,en;q=0.8,en-US;q=0.7"),
            host("https://intrade.bar"),
            proxy_type(kurlyk::ProxyType::HTTP) {
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
        /// \param user Username.
        /// \param token Token.
        void set_user_token(const std::string& user, const std::string& token) {
            this->user = user;
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
                kurlyk::ProxyType type = kurlyk::ProxyType::HTTP) {
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
        bool to_json(json &j) override {
            try {
                if (auth_method == AuthMethod::EMAIL_PASSWORD) {
                    j["email"] = email;
                    j["password"] = password;
                } else if (auth_method == AuthMethod::USER_TOKEN) {
                    j["user"] = user;
                    j["token"] = token;
                } else {
                    return false;
                }
                j["host"] = host;
                j["user_agent"] = user_agent;
                j["accept_language"] = accept_language;
                j["proxy_server"] = proxy_server;
                j["proxy_auth"] = proxy_auth;
                j["proxy_type"] = to_str(proxy_type);
                j["account_type"] = to_str(account_type);
                j["currency"] = to_str(currency);
                return true;
            } catch (const std::exception& ex) {
                LOGIT_ERROR(ex);
                return false;
            }
        }

        /// \brief Deserializes JSON data into authorization data.
        /// \param j JSON object to parse.
        /// \return True if deserialization was successful, false otherwise.
        bool from_json(json &j) override {
            try {
                if (j.contains("email") && j.contains("password")) {
                    email = j.at("email").get<std::string>();
                    password = j.at("password").get<std::string>();
                    auth_method = AuthMethod::EMAIL_PASSWORD;
                } else if (j.contains("user") && j.contains("token")) {
                    user = j.at("user").get<std::string>();
                    token = j.at("token").get<std::string>();
                    auth_method = AuthMethod::USER_TOKEN;
                } else {
                    auth_method = AuthMethod::NONE;
                    return false;
                }
                host = j.value("host", host);
                user_agent = j.value("user_agent", user_agent);
                accept_language = j.value("accept_language", accept_language);
                proxy_server = j.value("proxy_server", proxy_server);
                proxy_auth = j.value("proxy_auth", proxy_auth);

                account_type = to_enum<AccountType>(j.value("account_type", "UNKNOWN"));
                currency = to_enum<CurrencyType>(j.value("currency", "UNKNOWN"));
                return true;
            } catch (const std::exception& ex) {
                LOGIT_ERROR(ex);
                return false;
            }
        }

        /// \brief Validates the authorization data.
        /// \return True if the authorization data is valid, false otherwise.
        bool check() const override {
            if (auth_method == AuthMethod::EMAIL_PASSWORD) {
                return !email.empty() && !password.empty();
            } else if (auth_method == AuthMethod::USER_TOKEN) {
                return !user.empty() && !token.empty();
            }
            return false;
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

        /// \brief Retrieves the API type associated with this authorization data.
        /// \return The type of API used.
        ApiType api_type() const override {
            return ApiType::INTRADE_BAR;
        }

        AccountType account_type = AccountType::UNKNOWN;    ///< Account type, if supported.
        CurrencyType currency    = CurrencyType::UNKNOWN;   ///< Account currency, if supported.
        std::string email;          ///< Email for email/password authentication.
        std::string password;       ///< Password for email/password authentication.
        std::string user;           ///< Username for user/token authentication.
        std::string token;          ///< Token for user/token authentication.
        std::string user_agent;     ///<
        std::string accept_language;///<
        std::string host;           ///<
        std::string proxy_server;   ///< Proxy address in <ip:port> format.
        std::string proxy_auth;     ///< Proxy authentication in <username:password> format.
        kurlyk::ProxyType proxy_type; ///<
        AuthMethod auth_method = AuthMethod::NONE; ///< Authentication method used.
    }; // AuthData

} // namespace intrade_bar
} // namespace platforms
} // namespace optionx

#endif // _OPTIONX_PLATFORMS_INTRADERBAR_AUTH_DATA_HPP_INCLUDED
