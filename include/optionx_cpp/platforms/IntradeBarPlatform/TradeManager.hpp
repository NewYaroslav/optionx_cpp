#pragma once
#ifndef _OPTIONX_PLATFORMS_INTRADERBAR_TRADE_MANAGER_HPP_INCLUDED
#define _OPTIONX_PLATFORMS_INTRADERBAR_TRADE_MANAGER_HPP_INCLUDED

/// \file TradeManager.hpp
/// \brief Implements trade execution, trade status tracking, and balance updates.

namespace optionx::platforms::intrade_bar {

    /// \class TradeManager
    /// \brief Manages trade execution, trade status updates, and account balance tracking.
    ///
    /// This module is responsible for handling trade requests, monitoring trade status,
    /// and ensuring the account balance is correctly updated after each trade.
    class TradeManager final : public modules::BaseModule {
    public:

        /// \brief Constructs the trade manager.
        /// \param platform Reference to the trading platform.
        /// \param request_manager Reference to the request manager handling HTTP requests.
        /// \param account_info Shared pointer to account information structure.
        explicit TradeManager(
                BaseTradingPlatform& platform,
                RequestManager& request_manager,
                std::shared_ptr<BaseAccountInfoData> account_info)
                : BaseModule(platform.event_hub()),
                  m_request_manager(request_manager),
                  m_account_info(std::move(account_info))  {
            subscribe<events::TradeRequestEvent>(this);
            subscribe<events::TradeStatusEvent>(this);
            subscribe<events::OpenTradesEvent>(this);
            platform.register_module(this);
        }

        /// \brief Default destructor.
        virtual ~TradeManager() = default;

        /// \brief Processes incoming events and dispatches them to appropriate handlers.
        /// \param event The received event.
        void on_event(const utils::Event* const event) override;

        /// \brief Executes periodic tasks related to trade management.
        void process() override;

        /// \brief Shuts down the trade manager and cleans up resources.
        void shutdown() override;

    private:
        RequestManager&    m_request_manager; ///< Reference to the request manager.
        utils::TaskManager m_task_manager;    ///< Task manager for handling asynchronous tasks.
        std::shared_ptr<BaseAccountInfoData> m_account_info; ///< Shared pointer to account information.

        /// \brief Handles a trade request event.
        /// \param event The trade request event.
        void handle_event(const events::TradeRequestEvent& event);

        /// \brief Handles a trade status update event.
        /// \param event The trade status event.
        void handle_event(const events::TradeStatusEvent& event);

        /// \brief Handles an OpenTradesEvent.
        /// \param event The OpenTradesEvent containing the number of open trades.
        void handle_event(const events::OpenTradesEvent& event);

        /// \brief Processes trade status based on retrieved price and profit.
        /// \param success Indicates whether the trade check was successful.
        /// \param price The closing price of the trade.
        /// \param profit The profit or loss from the trade.
        /// \param balance The updated account balance after the trade.
        /// \param request The original trade request.
        /// \param result The trade result containing updated trade details.
        void process_trade_status(
                bool success,
                double price,
                double profit,
                double balance,
                std::shared_ptr<TradeRequest> request,
                std::shared_ptr<TradeResult> result);

        /// \brief Retrieves account information as an `AccountInfoData` instance.
        /// \return A shared pointer to `AccountInfoData` containing account details.
        std::shared_ptr<AccountInfoData> get_account_info();
    };

    void TradeManager::on_event(const utils::Event* const event) {
        if (const auto* msg = dynamic_cast<const events::TradeRequestEvent*>(event)) {
            handle_event(*msg);
        } else
        if (const auto* msg = dynamic_cast<const events::TradeStatusEvent*>(event)) {
            handle_event(*msg);
        } else
        if (const auto* msg = dynamic_cast<const events::OpenTradesEvent*>(event)) {
            handle_event(*msg);
        }
    };

    void TradeManager::process() {
        m_task_manager.process();
    }

    void TradeManager::shutdown() {
        m_task_manager.shutdown();
    }

    void TradeManager::handle_event(const events::TradeRequestEvent& event) {
        auto request = event.request;
        auto result  = event.result;

        m_request_manager.request_execute_trade(
                request, [this, request, result] (
                    bool success,
                    int64_t option_id,
                    int64_t open_date,
                    double open_price,
                    const std::string& error_desc) {
            const int64_t timestamp = OPTIONX_TIMESTAMP_MS;
            if (!success) {
                result->trade_state = result->live_state = TradeState::OPEN_ERROR;
                result->error_code = TradeErrorCode::PARSING_ERROR;
                result->error_desc = error_desc;
                result->delay = timestamp - result->send_date;
                result->ping = result->delay / 2;
                result->open_date = timestamp;
                result->close_date = request->option_type == OptionType::SPRINT
                                     ? timestamp + time_shield::sec_to_ms(request->duration)
                                     : time_shield::sec_to_ms(request->expiry_time);
                return;
            }

            result->option_id = option_id;
            result->open_date = open_date;
            result->close_date = request->option_type == OptionType::SPRINT
                                     ? open_date + time_shield::sec_to_ms(request->duration)
                                     : time_shield::sec_to_ms(request->expiry_time);
            result->delay = timestamp - result->open_date;
            result->ping = result->delay / 2;
            result->open_price = result->close_price = open_price;
            result->live_state = TradeState::STANDOFF;
            result->error_code = TradeErrorCode::SUCCESS;
            auto account_info  = get_account_info();
            result->payout     = account_info->get_for_trade<double>(AccountInfoType::PAYOUT, request, time_shield::ms_to_sec(result->open_date));

            m_request_manager.request_balance(
                    [this, request, result](
                    bool success,
                    double balance,
                    CurrencyType currency) {
                auto account_info = get_account_info();
                if (success) {
                    result->balance = balance;
                    result->trade_state = TradeState::OPEN_SUCCESS;

                    using Status = events::AccountInfoUpdateEvent::Status;
                    const double previous_balance = account_info->balance;

                    if (!account_info->connect) {
                        account_info->connect = true;
                        notify(events::AccountInfoUpdateEvent(account_info, Status::CONNECTED));
                    }

                    const double balance_tolerance = 0.01;
                    if (std::abs(previous_balance - balance) > balance_tolerance) {
                        account_info->balance = balance;
                        notify(events::AccountInfoUpdateEvent(account_info, Status::BALANCE_UPDATED));
                    }
                } else {
                    result->balance = account_info->balance;
                    result->trade_state = TradeState::OPEN_SUCCESS;
                }
            });
        });
    }

    void TradeManager::handle_event(const events::TradeStatusEvent& event) {
        auto request = event.request;
        auto result  = event.result;
        if (!request || !result) {
            LOGIT_ERROR("TradeStatusEvent received with null request or result.");
            return;
        }

        LOGIT_0TRACE();
        const int64_t delay_ms = 500;
        m_task_manager.add_delayed_task(
				"event(TradeStatusEvent)-500ms",
				delay_ms, 
				[this, request, result](
					std::shared_ptr<utils::Task> task) {
            LOGIT_0TRACE();
            if (task->is_shutdown()) {
                LOGIT_INFO("Task was shut down unexpectedly for option ID: ", result->option_id);
                auto account_info = get_account_info();
                result->payout = account_info->get_for_trade<double>(
                    AccountInfoType::PAYOUT,
                    request,
                    time_shield::ms_to_sec(result->open_date));
                result->profit =
                    result->live_state == TradeState::STANDOFF ? 0 :
                    (result->live_state == TradeState::WIN ?
                     result->payout * result->amount : -result->amount);
                result->trade_state = result->live_state = TradeState::CHECK_ERROR;
                result->error_code = TradeErrorCode::CLIENT_FORCED_CLOSE;
                result->error_desc = to_str(TradeErrorCode::CLIENT_FORCED_CLOSE);
                return;
            }
            const int retry_attempts = 15;
            m_request_manager.request_trade_check(
                    result->option_id,
                    retry_attempts,
                    [this, request, result](
                    bool success,
                    double price,
                    double profit) {
                if (!success) {
                    LOGIT_ERROR("Failed to retrieve trade result for option ID: ", result->option_id);
                    auto account_info = get_account_info();
                    result->payout = account_info->get_for_trade<double>(
                        AccountInfoType::PAYOUT,
                        request,
                        time_shield::ms_to_sec(result->open_date));
                    result->profit =
                        result->live_state == TradeState::STANDOFF ? 0 :
                        (result->live_state == TradeState::WIN ?
                         result->payout * result->amount : -result->amount);
                    result->trade_state = result->live_state = TradeState::CHECK_ERROR;
                    result->error_code = TradeErrorCode::PARSING_ERROR;
                    result->error_desc = "Failed to retrieve trade result.";
                    return;
                }
                LOGIT_0TRACE();
                m_request_manager.request_balance(
                        [this, request, result, price, profit](
                        bool success,
                        double balance,
                        CurrencyType currency) {
                    process_trade_status(
                        success,
                        price,
                        profit,
                        balance,
                        request,
                        result);
                });
            });
        });
    }

    void TradeManager::handle_event(const events::OpenTradesEvent& event) {
        using Status = events::AccountInfoUpdateEvent::Status;
        auto account_info = get_account_info();
        account_info->open_trades = event.open_trades;
        notify(events::AccountInfoUpdateEvent(account_info, Status::OPEN_TRADES_CHANGED));
    }

    void TradeManager::process_trade_status(
            bool success,
            double price,
            double profit,
            double balance,
            std::shared_ptr<TradeRequest> request,
            std::shared_ptr<TradeResult> result) {
        if (!request ||!result) return;
        auto account_info = get_account_info();
        if (!success) {
            result->balance = account_info->balance;
        } else {
            result->balance = balance;
        }
        result->close_price = price;

        const double balance_tolerance = 0.01;
        if (std::abs(profit - result->amount) < balance_tolerance) {
            result->trade_state = result->live_state = TradeState::STANDOFF;
            result->payout = account_info->get_for_trade<double>(
                AccountInfoType::PAYOUT,
                request,
                time_shield::ms_to_sec(result->open_date));
            result->profit = 0;
        } else
        if (profit <= balance_tolerance) {
            result->trade_state = result->live_state = TradeState::LOSS;
            result->payout = account_info->get_for_trade<double>(
                AccountInfoType::PAYOUT,
                request,
                time_shield::ms_to_sec(result->open_date));
            result->profit = -result->amount;
        } else {
            result->trade_state = result->live_state = TradeState::WIN;
            result->profit = profit - result->amount;
            result->payout = result->amount <= 0.0 ? 0.0 : utils::normalize_double((profit - result->amount) / result->amount, 2);
        }

        if (success) {
            using Status = events::AccountInfoUpdateEvent::Status;
            const double previous_balance = account_info->balance;

            if (!account_info->connect) {
                account_info->connect = true;
                notify(events::AccountInfoUpdateEvent(account_info, Status::CONNECTED));
            }

            if (std::abs(previous_balance - balance) > balance_tolerance) {
                account_info->balance = balance;
                notify(events::AccountInfoUpdateEvent(account_info, Status::BALANCE_UPDATED));
            }
        }
    }

    std::shared_ptr<AccountInfoData> TradeManager::get_account_info() {
        if (auto account_info = std::dynamic_pointer_cast<AccountInfoData>(m_account_info)) {
            return account_info;
        }
        LOGIT_FATAL("Failed to cast IAccountInfoData to AccountInfoData");
        throw std::runtime_error("Invalid account information type");
    }

} // namespace optionx::platforms::intrade_bar

#endif // _OPTIONX_PLATFORMS_INTRADERBAR_TRADE_MANAGER_HPP_INCLUDED
