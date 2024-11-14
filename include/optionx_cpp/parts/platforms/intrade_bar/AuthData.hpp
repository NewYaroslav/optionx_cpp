#pragma once
#ifndef _OPTIONX_PLATFORMS_INTRADERBAR_AUTH_DATA_HPP_INCLUDED
#define _OPTIONX_PLATFORMS_INTRADERBAR_AUTH_DATA_HPP_INCLUDED

/// \file AuthData.hpp
/// \brief Contains the AuthData class for Intrade Bar platform authorization.

#include "../../interfaces/IAuthData.hpp"
#include <string>

namespace optionx {
namespace platforms {
namespace intrade_bar {

    /// \class AuthData
    /// \brief Authorization data for Intrade Bar platform.
    class AuthData : public IAuthData {
    public:
        using json = nlohmann::json;

        enum class AuthMethod {
            NONE,           ///< Not specified
            EMAIL_PASSWORD, ///< Authorization via email and password
            USER_TOKEN      ///< Authorization via username and token
        };

        /// \brief Sets authorization data using email and password.
        void set_email_password(const std::string& email, const std::string& password) {
            this->email = email;
            this->password = password;
            auth_method = AuthMethod::EMAIL_PASSWORD;
        }

        /// \brief Sets authorization data using username and token.
        void set_user_token(const std::string& user, const std::string& token) {
            this->user = user;
            this->token = token;
            auth_method = AuthMethod::USER_TOKEN;
        }

        /// \brief Serializes authorization data to a JSON object.
        bool to_json(json &j) override {
            if (auth_method == AuthMethod::EMAIL_PASSWORD) {
                j["email"] = email;
                j["password"] = password;
            } else if (auth_method == AuthMethod::USER_TOKEN) {
                j["user"] = user;
                j["token"] = token;
            } else {
                return false;
            }
            return true;
        }

        /// \brief Deserializes JSON data into authorization data.
        bool from_json(json &j) override {
            if (j.contains("email") && j.contains("password")) {
                email = j["email"];
                password = j["password"];
                auth_method = AuthMethod::EMAIL_PASSWORD;
            } else if (j.contains("user") && j.contains("token")) {
                user = j["user"];
                token = j["token"];
                auth_method = AuthMethod::USER_TOKEN;
            } else {
                auth_method = AuthMethod::NONE;
                return false;
            }
            return true;
        }

        /// \brief Validates the authorization data.
        bool check() const override {
            if (auth_method == AuthMethod::EMAIL_PASSWORD) {
                return !email.empty() && !password.empty();
            } else if (auth_method == AuthMethod::USER_TOKEN) {
                return !user.empty() && !token.empty();
            }
            return false;
        }

        std::unique_ptr<IAuthData> clone_unique() const override {
            return std::make_unique<AuthData>(*this);
        }

        std::shared_ptr<IAuthData> clone_shared() const override {
            return std::make_shared<AuthData>(*this);
        }

        ApiType api_type() const override {
            return ApiType::INTRADE_BAR;
        }

        // Public member variables for direct access if needed
        std::string email;         ///< Email for email/password authentication
        std::string password;      ///< Password for email/password authentication
        std::string user;          ///< Username for user/token authentication
        std::string token;         ///< Token for user/token authentication
        AuthMethod auth_method = AuthMethod::NONE; ///< Authentication method used
    }; // AuthData

} // namespace intrade_bar
} // namespace platforms
} // namespace optionx

#endif // _OPTIONX_PLATFORMS_INTRADERBAR_AUTH_DATA_HPP_INCLUDED
