#pragma once
#ifndef _OPTIONX_PLATFORMS_INTRADERBAR_BALANCE_MANAGER_HPP_INCLUDED
#define _OPTIONX_PLATFORMS_INTRADERBAR_BALANCE_MANAGER_HPP_INCLUDED

/// \file BalanceManager.hpp
/// \brief Defines the BalanceManager class responsible for handling balance updates and account-related events.

namespace optionx::platforms::intrade_bar {

    /// \class BalanceManager
    /// \brief Manages balance updates and account-related events.
    ///
    /// This class is responsible for retrieving balance information, monitoring account activity,
    /// and reacting to trade events to ensure account consistency. It subscribes to various events
    /// related to trading activity and account status changes.
    class BalanceManager final : public modules::BaseModule {
    public:

        /// \brief Constructs the BalanceManager.
        /// \param platform Reference to the trading platform.
        /// \param request_manager Reference to the request manager for making HTTP requests.
        /// \param account_info Shared pointer to the account information data.
        explicit BalanceManager(
            BaseTradingPlatform& platform,
            RequestManager& request_manager,
            std::shared_ptr<BaseAccountInfoData> account_info)
            : BaseModule(platform.event_bus()), m_request_manager(request_manager),
              m_account_info(std::move(account_info))  {
            subscribe<events::ConnectRequestEvent>();
            subscribe<events::DisconnectRequestEvent>();
            subscribe<events::BalanceRequestEvent>();
            subscribe<events::TradeRequestEvent>();
            subscribe<events::AccountInfoUpdateEvent>();
            m_request_time = m_last_trades_time =
                time_shield::ms_to_sec(OPTIONX_TIMESTAMP_MS);
            platform.register_module(this);
        }

        /// \brief Default destructor.
        virtual ~BalanceManager() = default;

        /// \brief Processes incoming events and dispatches them to the appropriate handlers.
        /// \param event The received event.
        void on_event(const utils::Event* const event) override;

        /// \brief Processes periodic balance updates and account-related tasks.
        void process() override;

        /// \brief Shuts down all active balance-related operations.
        void shutdown() override;

    private:
        RequestManager&    m_request_manager; ///< Reference to the request manager.
        utils::TaskManager m_task_manager;    ///< Task manager for handling async tasks.
        std::shared_ptr<BaseAccountInfoData> m_account_info; ///< Shared pointer to account information.
        int64_t m_last_trades_time;           ///< Timestamp of the last trade activity.
        int64_t m_request_time;               ///< Timestamp of the last balance request.
        bool m_has_balance_update = false;    ///< Flag indicating if a balance update is in progress.
        bool m_check_host_in_progress = false;

         /// \brief Initiates a balance update request.
        void handle_balance_update();

        /// \brief Processes a successful balance update.
        /// \param balance The updated balance value.
        /// \param currency The account currency type.
        /// \param account_info Shared pointer to the account information.
        void process_balance_success(
            double balance,
            CurrencyType currency,
            std::shared_ptr<AccountInfoData>& account_info);

        /// \brief Handles a failed balance update.
        /// \param account_info Shared pointer to the account information.
        void process_balance_failure(
            std::shared_ptr<AccountInfoData>& account_info);

        /// \brief Handles an event triggered when a connection request is received.
        /// \param event The connection request event.
        void handle_event(const events::ConnectRequestEvent& event);

        /// \brief Handles an event triggered when a disconnection request is received.
        /// \param event The disconnection request event.
        void handle_event(const events::DisconnectRequestEvent& event);

        /// \brief Handles an event triggered when a balance request is received.
        /// \param event The balance request event.
        void handle_event(const events::BalanceRequestEvent& event);

        /// \brief Handles an event triggered when a trade request is received.
        /// \param event The trade request event.
        void handle_event(const events::TradeRequestEvent& event);

        /// \brief Handles an account information update event.
        /// \param event The account info update event.
        void handle_event(const events::AccountInfoUpdateEvent& event);

        /// \brief Handles account connection events.
        void handle_connected();

        /// \brief Handles account disconnection events.
        void handle_disconnected();

        /// \brief Retrieves the account information as an `AccountInfoData` instance.
        /// \return A shared pointer to `AccountInfoData`.
        std::shared_ptr<AccountInfoData> get_account_info();
    };

    void BalanceManager::on_event(const utils::Event* const event) {
        if (const auto* msg = dynamic_cast<const events::ConnectRequestEvent*>(event)) {
            handle_event(*msg);
        } else
        if (const auto* msg = dynamic_cast<const events::DisconnectRequestEvent*>(event)) {
            handle_event(*msg);
        } else
        if (const auto* msg = dynamic_cast<const events::TradeRequestEvent*>(event)) {
            handle_event(*msg);
        } else
        if (const auto* msg = dynamic_cast<const events::BalanceRequestEvent*>(event)) {
            handle_event(*msg);
        } else
        if (const auto* msg = dynamic_cast<const events::AccountInfoUpdateEvent*>(event)) {
            handle_event(*msg);
        }
    };

    void BalanceManager::process() {
        m_task_manager.process();
    }

    void BalanceManager::shutdown() {
        m_task_manager.shutdown();
    }

    // Handles balance updates.
    void BalanceManager::handle_balance_update() {
        if (m_has_balance_update) return;
        m_has_balance_update = true;
        m_request_manager.request_balance([this](
                bool success,
                double balance,
                CurrencyType currency) {
            m_has_balance_update = false;
            m_check_host_in_progress = false;
            auto account_info = get_account_info();
            if (!account_info) {
                LOGIT_ERROR("Failed to get account info.");
                return;
            }

            if (success) {
                process_balance_success(balance, currency, account_info);
            } else {
                process_balance_failure(account_info);
            }
        });
    }

    // Processes successful balance updates.
    void BalanceManager::process_balance_success(
            double balance,
            CurrencyType currency,
            std::shared_ptr<AccountInfoData>& account_info) {
        using Status = events::AccountInfoUpdateEvent::Status;
        const double previous_balance = account_info->balance;

        if (!account_info->connect) {
            account_info->connect = true;
            notify(events::AccountInfoUpdateEvent(account_info, Status::CONNECTED));
            handle_connected();
        }

        int64_t elapsed = time_shield::ms_to_sec(OPTIONX_TIMESTAMP_MS) - m_last_trades_time;

        const double balance_tolerance = 0.01;
        const int64_t max_elapsed = 3 * time_shield::SEC_PER_MIN;
        if ((account_info->currency != currency) ||
            (std::abs(previous_balance - balance) > balance_tolerance &&
            elapsed >= max_elapsed)) {
            notify(events::RestartAuthEvent());
        } else
        if (std::abs(previous_balance - balance) > balance_tolerance) {
            account_info->balance = balance;
            notify(events::AccountInfoUpdateEvent(account_info, Status::BALANCE_UPDATED));
        }
    }

    // Processes balance update failure.
    void BalanceManager::process_balance_failure(
            std::shared_ptr<AccountInfoData>& account_info) {
        if (account_info->connect) {
            account_info->connect = false;
            using Status = events::AccountInfoUpdateEvent::Status;
            const std::string error_text("Failed to retrieve balance.");
            LOGIT_ERROR(error_text);
            notify(events::AccountInfoUpdateEvent(account_info, Status::DISCONNECTED, error_text));
            handle_disconnected();
        }
    }

    void BalanceManager::handle_event(const events::BalanceRequestEvent& event) {
        m_last_trades_time = time_shield::ms_to_sec(OPTIONX_TIMESTAMP_MS);
        handle_balance_update();
    }

    void BalanceManager::handle_event(const events::TradeRequestEvent& event) {
        m_last_trades_time = time_shield::ms_to_sec(OPTIONX_TIMESTAMP_MS);
        auto request = event.request;
        //auto result  = event.result;
        if (request->option_type == OptionType::SPRINT) {
            m_last_trades_time += request->duration;
        } else {
            if (request->duration > 0) {
                m_last_trades_time += request->duration;
            } else {
                m_last_trades_time = request->expiry_time;
            }
        }
        const int64_t max_elapsed = 3 * time_shield::SEC_PER_MIN;
        m_last_trades_time += max_elapsed;
    }

    void BalanceManager::handle_event(const events::ConnectRequestEvent& event) {
        LOGIT_TRACE0();
        m_task_manager.shutdown();
    }

    void BalanceManager::handle_event(const events::DisconnectRequestEvent& event) {
        LOGIT_TRACE0();
        m_task_manager.shutdown();
    }

    void BalanceManager::handle_event(const events::AccountInfoUpdateEvent& event) {
        using Status = events::AccountInfoUpdateEvent::Status;
        if (event.status == Status::CONNECTED) {
            handle_connected();
        } else
        if (event.status == Status::DISCONNECTED) {
            handle_disconnected();
        }
    }

    /// \brief Handles account connection event.
    void BalanceManager::handle_connected() {
        LOGIT_TRACE0();
        
        m_task_manager.add_periodic_task(
                "connected-15min",
                time_shield::MS_PER_15_MIN,
                [this](std::shared_ptr<utils::Task> task){
            if (task->is_shutdown()) return;
            LOGIT_TRACE0();
            handle_balance_update();
        });
        
        m_task_manager.add_periodic_task(
                "connected-15sec",
                time_shield::MS_PER_15_SEC,
                [this](std::shared_ptr<utils::Task> task){
            if (task->is_shutdown()) return;
            if (m_check_host_in_progress) return;
            m_check_host_in_progress = true;
            
            LOGIT_TRACE0();
            m_request_manager.request_check_current_host_available([this](
                    bool success) {
                LOGIT_TRACE0();
                m_check_host_in_progress = false;
                if (!success) {
                    auto account_info = get_account_info();
                    if (account_info->connect) {
                        account_info->connect = false;
                        using Status = events::AccountInfoUpdateEvent::Status;
                        const std::string error_text("Ping to current host failed.");
                        LOGIT_ERROR(error_text);
                        notify(events::AccountInfoUpdateEvent(account_info, Status::DISCONNECTED, error_text));
                        handle_disconnected();
                    }
                }
            });
        });
    }

    /// \brief Handles account disconnection event.
    void BalanceManager::handle_disconnected() {
        LOGIT_TRACE0();

        m_task_manager.shutdown();
        m_task_manager.add_periodic_task(
                "disconnected-15sec",
                time_shield::MS_PER_15_SEC,
                [this](std::shared_ptr<utils::Task> task){
            LOGIT_TRACE0();
            if (task->is_shutdown()) return;
            if (m_check_host_in_progress) return;
            m_check_host_in_progress = true;
            
            LOGIT_TRACE0();
            m_request_manager.request_check_current_host_available([this](
                    bool success) {
                if (success) {
                    handle_balance_update();
                    return;
                }
                
                m_request_manager.request_find_working_domain(
                        [this](bool success, std::string& host) {
                    if (!success) {
                        m_check_host_in_progress = false;
                        return;
                    }

                    notify(events::AutoDomainSelectedEvent(
                        success,
                        host));
                                
                    handle_balance_update();
                });
            });
        });
    }

    std::shared_ptr<AccountInfoData> BalanceManager::get_account_info() {
        if (auto account_info = std::dynamic_pointer_cast<AccountInfoData>(m_account_info)) {
            return account_info;
        }
        LOGIT_FATAL("Failed to cast IAccountInfoData to AccountInfoData");
        throw std::runtime_error("Invalid account information type");
    }

} // namespace optionx::platforms::intrade_bar

#endif // _OPTIONX_PLATFORMS_INTRADERBAR_BALANCE_MANAGER_HPP_INCLUDED
