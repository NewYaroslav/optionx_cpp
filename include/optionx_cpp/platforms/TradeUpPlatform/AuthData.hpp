#pragma once
#ifndef _OPTIONX_PLATFORMS_TRADEUP_AUTH_DATA_HPP_INCLUDED
#define _OPTIONX_PLATFORMS_TRADEUP_AUTH_DATA_HPP_INCLUDED

/// \file AuthData.hpp
/// \brief Contains the AuthData class for TradeUp platform authorization.

#include <nlohmann/json.hpp>
#include "optionx_cpp/data/account/IAuthData.hpp"
#include "optionx_cpp/data/trading/enums.hpp"

namespace optionx::platforms::tradeup {

    /// \class AuthData
    /// \brief Represents authorization data for the TradeUp platform.
    class AuthData : public IAuthData {
    public:
        std::string login;               ///< User login (email)
        std::string password;            ///< Account password
        bool        stay_logged = true;  ///< Flag to keep session logged in
        std::string user_agent;          ///< User agent string
        std::string accept_language;     ///< Accept-Language header
        std::string host;                ///< API host

        AuthData()
            : user_agent("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/133.0.0.0 Safari/537.36"),
              accept_language("ru,ru-RU;q=0.9,en;q=0.8,en-US;q=0.7"),
              host("https://tradeup.net") {}

        /// \brief Sets login and password for authorization.
        void set_login_password(const std::string& l, const std::string& p) {
            login = l;
            password = p;
        }

        void to_json(nlohmann::json& j) const override {
            j = nlohmann::json{
                {"login", login},
                {"password", password},
                {"stay_logged", stay_logged},
                {"host", host},
                {"user_agent", user_agent},
                {"accept_language", accept_language}
            };
        }

        void from_json(const nlohmann::json& j) override {
            login = j.value("login", "");
            password = j.value("password", "");
            stay_logged = j.value("stay_logged", true);
            host = j.value("host", host);
            user_agent = j.value("user_agent", user_agent);
            accept_language = j.value("accept_language", accept_language);
        }

        std::pair<bool, std::string> validate() const override {
            if (login.empty()) return {false, "Login is empty"};
            if (password.empty()) return {false, "Password is empty"};
            return {true, std::string()};
        }

        std::unique_ptr<IAuthData> clone_unique() const override {
            return std::make_unique<AuthData>(*this);
        }

        std::shared_ptr<IAuthData> clone_shared() const override {
            return std::make_shared<AuthData>(*this);
        }

        PlatformType platform_type() const override {
            return PlatformType::TRADEUP;
        }
    };

} // namespace optionx::platforms::tradeup

#endif // _OPTIONX_PLATFORMS_TRADEUP_AUTH_DATA_HPP_INCLUDED
