#pragma once
#ifndef _OPTIONX_PLATFORMS_TRADEUP_BALANCE_MANAGER_HPP_INCLUDED
#define _OPTIONX_PLATFORMS_TRADEUP_BALANCE_MANAGER_HPP_INCLUDED

/// \file BalanceManager.hpp
/// \brief Handles balance updates for the TradeUp platform.

#include "optionx_cpp/platforms/TradeUpPlatform/HttpClientModule.hpp"
#include "optionx_cpp/platforms/TradeUpPlatform/http_parsers.hpp"
#include "optionx_cpp/platforms/TradeUpPlatform/http_utils.hpp"
#include "optionx_cpp/platforms/TradeUpPlatform/AccountInfoData.hpp"
#include "optionx_cpp/data/events/BalanceRequestEvent.hpp"
#include "optionx_cpp/data/events/AccountInfoUpdateEvent.hpp"
#include "optionx_cpp/modules/BaseModule.hpp"
#include "optionx_cpp/platforms/common/BaseTradingPlatform.hpp"

namespace optionx::platforms::tradeup {

    /// \class BalanceManager
    class BalanceManager final : public modules::BaseModule {
    public:
        BalanceManager(BaseTradingPlatform& platform,
                       HttpClientModule& http_client,
                       std::shared_ptr<BaseAccountInfoData> account_info)
            : BaseModule(platform.event_bus()),
              m_http_client(http_client),
              m_account_info(std::move(account_info)) {
            subscribe<events::BalanceRequestEvent>();
            platform.register_module(this);
        }

        void on_event(const utils::Event* const event) override;
        void process() override {}
        void shutdown() override {}

    private:
        HttpClientModule& m_http_client;
        std::shared_ptr<BaseAccountInfoData> m_account_info;

        void handle_event(const events::BalanceRequestEvent& event);
        void request_balance();
        std::shared_ptr<AccountInfoData> get_account_info();
    };

    inline void BalanceManager::on_event(const utils::Event* const event) {
        if (const auto* msg = dynamic_cast<const events::BalanceRequestEvent*>(event)) {
            handle_event(*msg);
        }
    }

    inline std::shared_ptr<AccountInfoData> BalanceManager::get_account_info() {
        return std::dynamic_pointer_cast<AccountInfoData>(m_account_info);
    }

    inline void BalanceManager::handle_event(const events::BalanceRequestEvent& event) {
        (void)event;
        request_balance();
    }

    inline void BalanceManager::request_balance() {
        if (m_http_client.auth_token().empty()) return;
        kurlyk::Headers headers = {
            {"Accept", "application/json"},
            {"Content-Type", "application/json"},
            {"Cookie", m_http_client.session_cookie()},
            {"X-API-TOKEN", m_http_client.auth_token()}
        };
        auto future = m_http_client.get_http_client().post(
            "/api/v1/info",
            kurlyk::QueryParams(),
            headers,
            "{}",
            m_http_client.get_rate_limit(RateLimitType::BALANCE)
        );
        m_http_client.add_http_request_task(std::move(future), [this](kurlyk::HttpResponsePtr response){
            if (!validate_response(response)) return;
            double bal = 0.0; CurrencyType cur = CurrencyType::UNKNOWN;
            if (parse_info_response(response->content, bal, cur)) {
                auto info = get_account_info();
                if (!info) return;
                bool was_connected = info->connect;
                info->balance = bal;
                info->currency = cur;
                info->connect = true;
                auto status = was_connected ?
                    events::AccountInfoUpdateEvent::Status::BALANCE_UPDATED :
                    events::AccountInfoUpdateEvent::Status::CONNECTED;
                notify(events::AccountInfoUpdateEvent(info, status));
                std::string session = extract_cookie(response->headers, "multibrand_session");
                if (!session.empty()) {
                    m_http_client.set_session_cookie("nip-auth-token=" + m_http_client.auth_token() + "; multibrand_session=" + session);
                }
            }
        });
    }

} // namespace optionx::platforms::tradeup

#endif // _OPTIONX_PLATFORMS_TRADEUP_BALANCE_MANAGER_HPP_INCLUDED
