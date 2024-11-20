#pragma once
#ifndef _OPTIONX_PLATFORMS_INTRADERBAR_HTTP_CLIENT_MODULE_HPP_INCLUDED
#define _OPTIONX_PLATFORMS_INTRADERBAR_HTTP_CLIENT_MODULE_HPP_INCLUDED

/// \file HttpClientModule.hpp
/// \brief

#include "../../../modules/HttpClientModule.hpp"
#include "../../../storage/ServiceSessionDB.hpp"
#include "../../../utils/PeriodicTask.hpp"
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
        //kurlyk::Headers             m_profile_headers;
        utils::PeriodicTask         m_balance_task;
        utils::PeriodicTask         m_price_task;
        std::unique_ptr<AuthData>   m_auth_data;
        std::string                 m_user_id;
        std::string                 m_user_hash;
        std::string                 m_cookies;
        std::chrono::steady_clock::time_point m_last_trades_time; ///<

        // События и задача ----------------------------------------------------

        /// \brief Handles the account information request event.
        /// \param event The account information request event.
        void handle_event(const modules::AccountInfoRequestEvent& event) override final;

        /// \brief Handles the balance request event.
        /// \param event The balance request event.
        void handle_event(const modules::BalanceRequestEvent& event) override final;

        /// \brief Handles the trade request event.
        /// \param event The trade request event.
        void handle_event(const modules::TradeRequestEvent& event) override final;

        /// \brief Handles the trade status update event.
        /// \param event The trade status update event.
        void handle_event(const modules::TradeStatusEvent& event) override final;

        /// \brief Handles the authorization data event.
        /// \param event The authorization data event.
        void handle_event(const modules::AuthDataEvent& event) override final;

        /// \brief Handles the connection request event by processing authentication details.
        /// \param event The connection request event, including a callback to process the connection result.
        void handle_event(const modules::ConnectRequestEvent& event) override final;

        void process_additional_logic() override final {
            m_balance_task.process();
            m_price_task.process();
        }

        /// \brief Initializes periodic tasks for balance and price updates.
        void initialize_tasks();

        // Инициализация -------------------------------------------------------

        /// \brief Initializes rate limits.
        void initialize_rate_limits() override final;

        /// \brief Initializes HTTP client.
        void initialize_client() override final {
            initialize_database();
            initialize_headers();
            configure_client();
            initialize_tasks();
            m_last_trades_time = std::chrono::steady_clock::now();
        }

        // Результат сделки ----------------------------------------------------

        /// \brief Processes the trade result and updates the event result accordingly.
        /// \param price The retrieved trade close price.
        /// \param profit The retrieved trade profit.
        /// \param balance The current account balance (optional). If not provided (0), the stored account balance is used.
        /// \param request Shared pointer to the trade request containing order details.
        /// \param result Shared pointer to the trade result to be updated.
        void process_trade_status(
                double price,
                double profit,
                double balance,
                std::shared_ptr<TradeRequest> request,
                std::shared_ptr<TradeResult> result);

        // Баланс --------------------------------------------------------------

        /// \brief Handles balance updates.
        void handle_balance_update();

        /// \brief Processes successful balance updates.
        void process_balance_success(double balance, CurrencyType currency, std::shared_ptr<AccountInfoData> account_info);

        /// \brief Processes balance update failure.
        void process_balance_failure(std::shared_ptr<AccountInfoData> account_info);

        /// \brief Restarts the authentication flow.
        void restart_authentication_flow();

        // Цена ----------------------------------------------------------------

        /// \brief Handles price updates.
        void handle_price_update() {
            if (!get_account_info()->connect) return;
            request_price([this](bool success, const std::vector<TickInfo>& ticks) {
                if (!success) return;
                modules::PriceUpdateEvent event(ticks);
                notify(event);
            });
        }

        // Авторизация ---------------------------------------------------------

        void validate_email_pass(
                std::shared_ptr<AuthData> auth_data,
                modules::ConnectRequestEvent::callback_t connect_callback);

        void validate_user_token(
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

        // Запросы -------------------------------------------------------------

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

        void request_price(
                std::function<void(bool success, const std::vector<TickInfo>& ticks)> price_callback);

        /// \brief Requests trade check result and parses the response.
        /// \param deal_id The ID of the deal to check.
        /// \param callback A function that takes the following parameters:
        ///        - success: True if the request succeeded and was parsed successfully.
        ///        - price: The trade price.
        ///        - profit: The trade profit.
        /// \param retry_attempts Number of attempts to retry the request if the response is empty.
        void request_trade_check(
                int64_t deal_id,
                std::function<void(bool success, double price, double profit)> callback,
                int retry_attempts = 10);

        /// \brief Sends a request to open a trade and processes the response.
        /// \param request A shared pointer to the TradeRequest containing trade details.
        /// \param result A shared pointer to the TradeResult to store the result of the trade.
        /// \param request_callback Callback function invoked with the result of the request:
        ///        - success: Indicates if the trade was successfully opened.
        void request_execute_trade(
                std::shared_ptr<TradeRequest> request,
                std::shared_ptr<TradeResult> result,
                std::function<void(bool success)> request_callback);

        // Вспомогательные методы ----------------------------------------------

        /// \brief Initializes the database connection.
        void initialize_database() {
            storage::ServiceSessionDB::get_instance();
        }

        /// \brief Initializes headers for the HTTP client.
        void initialize_headers();

        /// \brief Configures the HTTP client.
        void configure_client();

        /// \brief Notifies about account status updates.
        void notify_account_status(std::shared_ptr<AccountInfoData> account_info, modules::AccountInfoUpdateEvent::Status status);

        /// \brief Retrieves the current account information as a shared pointer.
        /// \return Shared pointer to the account information data.
        /// \throws std::runtime_error if the account information cannot be cast to the expected type.
        std::shared_ptr<AccountInfoData> get_account_info();

    }; // HttpClientModule

} // namespace intrade_bar
} // namespace platforms
} // namespace optionx

#include "HttpClientModule/Utils.hpp"
#include "HttpClientModule/Requests.hpp"
#include "HttpClientModule/EventHandlers.hpp"
#include "HttpClientModule/Authorization.hpp"
#include "HttpClientModule/Balance.hpp"
#include "HttpClientModule/TradeStatus.hpp"

#endif // _OPTIONX_PLATFORMS_INTRADERBAR_HTTP_CLIENT_MODULE_HPP_INCLUDED
