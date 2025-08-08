#pragma once
#ifndef _OPTIONX_PLATFORMS_TRADEUP_AUTH_MANAGER_HPP_INCLUDED
#define _OPTIONX_PLATFORMS_TRADEUP_AUTH_MANAGER_HPP_INCLUDED

/// \file AuthManager.hpp
/// \brief Handles authentication for the TradeUp platform.

#include "optionx_cpp/platforms/TradeUpPlatform/AuthData.hpp"
#include "optionx_cpp/platforms/TradeUpPlatform/HttpClientModule.hpp"
#include "optionx_cpp/platforms/TradeUpPlatform/http_parsers.hpp"
#include "optionx_cpp/platforms/TradeUpPlatform/http_utils.hpp"
#include "optionx_cpp/platforms/TradeUpPlatform/AccountInfoData.hpp"
#include "optionx_cpp/data/events/AuthDataEvent.hpp"
#include "optionx_cpp/data/events/ConnectRequestEvent.hpp"
#include "optionx_cpp/data/events/DisconnectRequestEvent.hpp"
#include "optionx_cpp/data/events/BalanceRequestEvent.hpp"
#include "optionx_cpp/data/events/AccountInfoUpdateEvent.hpp"
#include "optionx_cpp/platforms/common/BaseTradingPlatform.hpp"
#include "optionx_cpp/modules/BaseModule.hpp"
#include "optionx_cpp/utils/tasks/TaskManager.hpp"

namespace optionx::platforms::tradeup {

    /// \class AuthManager
    class AuthManager final : public modules::BaseModule {
    public:
        AuthManager(BaseTradingPlatform& platform,
                    HttpClientModule& http_client,
                    std::shared_ptr<BaseAccountInfoData> account_info)
            : BaseModule(platform.event_bus()),
              m_http_client(http_client),
              m_account_info(std::move(account_info)) {
            subscribe<events::AuthDataEvent>();
            subscribe<events::ConnectRequestEvent>();
            subscribe<events::DisconnectRequestEvent>();
            platform.register_module(this);
        }

        void on_event(const utils::Event* const event) override;
        void process() override {}
        void shutdown() override {}

    private:
        HttpClientModule& m_http_client;
        std::shared_ptr<BaseAccountInfoData> m_account_info;
        std::shared_ptr<AuthData> m_auth_data;

        void handle_event(const events::AuthDataEvent& event);
        void handle_event(const events::ConnectRequestEvent& event);
        void handle_event(const events::DisconnectRequestEvent& event);

        void perform_login(connection_callback_t callback);
        std::shared_ptr<AccountInfoData> get_account_info();
    };

    inline void AuthManager::on_event(const utils::Event* const event) {
        if (const auto* msg = dynamic_cast<const events::AuthDataEvent*>(event)) {
            handle_event(*msg);
        } else if (const auto* msg = dynamic_cast<const events::ConnectRequestEvent*>(event)) {
            handle_event(*msg);
        } else if (const auto* msg = dynamic_cast<const events::DisconnectRequestEvent*>(event)) {
            handle_event(*msg);
        }
    }

    inline void AuthManager::handle_event(const events::AuthDataEvent& event) {
        if (auto data = std::dynamic_pointer_cast<AuthData>(event.auth_data)) {
            auto [ok, msg] = data->validate();
            if (ok) m_auth_data = std::move(data);
            if (m_auth_data) m_auth_data->dispatch_callbacks(ok, msg);
        }
    }

    inline std::shared_ptr<AccountInfoData> AuthManager::get_account_info() {
        return std::dynamic_pointer_cast<AccountInfoData>(m_account_info);
    }

    inline void AuthManager::perform_login(connection_callback_t callback) {
        if (!m_auth_data) {
            if (callback) callback({false, "Auth data not set", nullptr});
            return;
        }

        kurlyk::Headers headers = {
            {"Accept", "application/json"},
            {"Content-Type", "application/json"}
        };
        nlohmann::json body = {
            {"password", m_auth_data->password},
            {"stayLogged", m_auth_data->stay_logged},
            {"login", m_auth_data->login}
        };
        auto future = m_http_client.get_http_client().post(
            "/trade-api/api/signin",
            kurlyk::QueryParams(),
            headers,
            body.dump(),
            m_http_client.get_rate_limit(RateLimitType::AUTH)
        );
        m_http_client.add_http_request_task(std::move(future), [this, callback](kurlyk::HttpResponsePtr response){
            if (!validate_response(response)) {
                if (callback) callback({false, "HTTP error", m_auth_data ? m_auth_data->clone_unique() : nullptr});
                return;
            }
            std::string token, uid;
            if (!parse_signin_response(response->content, token, uid)) {
                if (callback) callback({false, "Parse error", m_auth_data ? m_auth_data->clone_unique() : nullptr});
                return;
            }
            m_http_client.set_auth_token(token);
            std::string cookies = "nip-auth-token=" + token;
            std::string session = extract_cookie(response->headers, "multibrand_session");
            if (!session.empty()) cookies += "; multibrand_session=" + session;
            m_http_client.set_session_cookie(cookies);

            // Request balance after login
            notify(events::BalanceRequestEvent());
            if (callback) callback({true, std::string(), m_auth_data ? m_auth_data->clone_unique() : nullptr});
        });
    }

    inline void AuthManager::handle_event(const events::ConnectRequestEvent& event) {
        perform_login(event.callback);
    }

    inline void AuthManager::handle_event(const events::DisconnectRequestEvent& event) {
        auto info = get_account_info();
        if (info && info->connect) {
            info->connect = false;
            notify(events::AccountInfoUpdateEvent(info, events::AccountInfoUpdateEvent::Status::DISCONNECTED));
        }
        if (event.callback) event.callback({true, std::string(), m_auth_data ? m_auth_data->clone_unique() : nullptr});
    }

} // namespace optionx::platforms::tradeup

#endif // _OPTIONX_PLATFORMS_TRADEUP_AUTH_MANAGER_HPP_INCLUDED
