#pragma once
#ifndef _OPTIONX_MODULES_ORDER_MANAGER_MODULE_HPP_INCLUDED
#define _OPTIONX_MODULES_ORDER_MANAGER_MODULE_HPP_INCLUDED

/// \file OrderManagerModule.hpp
/// \brief Contains the OrderManagerModule class for managing trade requests and processing pending orders.

#include "../pubsub/EventMediator.hpp"
#include "../interfaces/IAccountInfoData.hpp"
#include "events/TradeTransactionEvent.hpp"
#include "events/AccountInfoEvent.hpp"
#include "events/BalanceRequestEvent.hpp"
#include "events/TradeStatusEvent.hpp"
#include "events/PriceUpdateEvent.hpp"
#include <chrono>
#include <list>
#include <mutex>

namespace optionx {
namespace modules {

    /// \class OrderManagerModule
    /// \brief Manages trade requests, checks order constraints, and processes pending orders.
    class OrderManagerModule : public EventMediator {
    public:
        using trade_result_callback_t = std::function<void(std::unique_ptr<TradeRequest>, std::unique_ptr<TradeResult>)>;

        explicit OrderManagerModule(EventHub& hub, std::shared_ptr<IAccountInfoData> account_info)
            : EventMediator(hub), m_account_info(std::move(account_info)) {
            m_last_order_time = std::chrono::steady_clock::now();
            subscribe<AccountInfoEvent>(this);
        }

        /// \brief Default virtual destructor.
        virtual ~OrderManagerModule() = default;

        /// \brief Handles an event notification received as a shared pointer.
        /// \param event The event received, passed as a shared pointer.
        void on_event(const std::shared_ptr<Event>& event) override {
            if (auto msg = std::dynamic_pointer_cast<AuthDataEvent>(event)) {
                handle_event(*msg);
            } else
            if (auto msg = std::dynamic_pointer_cast<PriceUpdateEvent>(event)) {
                handle_event(*msg);
            }
        }

        /// \brief Handles an event notification received as a raw pointer.
        /// \param event The event received, passed as a raw pointer.
        void on_event(const Event* const event) override {
            if (const auto* msg = dynamic_cast<const AuthDataEvent*>(event)) {
                handle_event(*msg);
            } else
            if (const auto* msg = dynamic_cast<const PriceUpdateEvent*>(event)) {
                handle_event(*msg);
            }
        }

        /// \brief Sets a callback for trade result events, including open, close, and errors.
        /// \param callback Function to handle trade result events.
        void set_trade_result_callback(trade_result_callback_t callback) {
            std::lock_guard<std::mutex> lock(m_trade_result_mutex);
            m_trade_result_callback = std::move(callback);
        }

        /// \brief Places a trade request into the pending queue after validation.
        /// \param request Unique pointer to a trade request.
        /// \return True if the request is valid and added to the queue; false otherwise.
        bool place_trade(std::unique_ptr<TradeRequest> request);

        /// \brief Finalizes all active and pending trades.
        void finalize_all_trades();

        /// \brief Processes all pending orders in the queue.
        virtual void process() {
            process_pending_orders();
            process_closing_orders();
            process_finalizing_orders();
        }

    protected:

        /// \brief Handles the authorization data event.
        /// \param event The authorization data event.
        virtual void handle_event(const AuthDataEvent& event) = 0;

        /// \brief Processes the current state of the transaction based on order type and prices.
        /// \param result Shared pointer to the transaction result.
        /// \param request Shared pointer to the order request.
        /// \param tick The tick information associated with the symbol.
        /// \return The new order state.
        virtual OrderState process_current_state(const std::shared_ptr<OrderResult>& result,
                                                 const std::shared_ptr<OrderRequest>& request,
                                                 const TickInfo& tick) const {
            if (!result->open_price) {
                return OrderState::STANDOFF;
            }

            double open_close_price = tick.get_average_price();
            if (request->order_type == OrderType::BUY) {
                if (open_close_price > result->open_price) return OrderState::WIN;
                if (open_close_price < result->open_price) return OrderState::LOSS;
                return OrderState::STANDOFF;
            }

            if (request->order_type == OrderType::SELL) {
                if (open_close_price < result->open_price) return OrderState::WIN;
                if (open_close_price > result->open_price) return OrderState::LOSS;
                return OrderState::STANDOFF;
            }

            return OrderState::STANDOFF;
        }

        /// \brief Retrieves the API type of the account.
        /// \return ApiType of the account.
        virtual ApiType get_api_type() = 0;

        using time_point_t = std::chrono::steady_clock::time_point;
        using transaction_t = std::shared_ptr<TradeTransactionEvent>;

        std::shared_ptr<AccountInfoEvent>   m_account_info;                 ///< Shared pointer to account information

        std::mutex                          m_pending_transactions_mutex;   ///< Mutex for pending transactions list
        std::list<transaction_t>            m_pending_transactions;         ///< List of pending transactions
        std::list<transaction_t>            m_open_transactions;            ///<

        time_point_t                        m_last_order_time;              ///< Timestamp of the last processed order
        int64_t                             m_open_trades = 0;              ///< Number of currently tracked open trades (confirmed + pending).
        mutable std::mutex                  m_trade_result_mutex;           ///<
        trade_result_callback_t             m_trade_result_callback;        ///<

        void handle_event(const PriceUpdateEvent& event) {
            for (auto& transaction : m_open_transactions) {
                auto& request = transaction->request;
                auto& result = transaction->result;
                if (result->state != OrderState::OPEN_SUCCESS) continue;

                auto tick = event.get_tick_by_symbol(request->symbol);
                if (!tick.is_initialized()) continue;

                result->open_close = tick.get_average_price();
                result->current_state = process_current_state(result, request, tick);
                invoke_callback(transaction);
            }
        }

        /// \brief Processes all pending trade transactions.
        void process_pending_orders();

        /// \brief Processes open orders that are ready to be closed or transitioned into a waiting state.
        void process_closing_orders();

        /// \brief Finalizes transactions in their terminal states, notifies listeners, and removes them from tracking.
        void process_finalizing_orders();

        /// \brief Increments the open trades counter and emits an event.
        /// \param request Shared pointer to the trade request details.
        /// \param result Shared pointer to the trade result details.
        void increment_open_trades(
            const std::shared_ptr<TradeRequest>& request,
            const std::shared_ptr<TradeResult>& result);

        /// \brief Decrements the open trades counter and emits an event.
        /// \param request Shared pointer to the trade request details.
        /// \param result Shared pointer to the trade result details.
        void decrement_open_trades(
            const std::shared_ptr<TradeRequest>& request,
            const std::shared_ptr<TradeResult>& result);

        /// \brief Retrieves account information based on the request.
        template<class T>
        const T get_account_info(const AccountInfoRequest& request);

        /// \brief Retrieves account information based on type and optional timestamp.
        template<class T>
        const T get_account_info(AccountInfoType type, int64_t timestamp = 0);

        template<class T>
        const T get_account_info(const std::string &symbol, int64_t timestamp);

        template<class T>
        const T get_account_info(OptionType option, int64_t timestamp = 0);

        template<class T>
        const T get_account_info(OrderType order, int64_t timestamp = 0);

        template<class T>
        const T get_account_info(AccountType account, int64_t timestamp = 0);

        template<class T>
        const T get_account_info(CurrencyType currency, int64_t timestamp = 0);

        template<class T>
        const T get_account_info(AccountInfoType info_type, std::shared_ptr<TradeRequest>& trade_request, int64_t timestamp = 0);

        void invoke_callback(const transaction_t& transaction) {
            auto &request = transaction->request;
            auto &result  = transaction->result;

            notify(transaction.get());
            request->invoke_callback(request, result);

            std::lock_guard<std::mutex> lock(m_trade_result_mutex);
            if (m_trade_result_callback) {
                m_trade_result_callback(
                    std::move(request->clone_unique()),
                    std::move(result->clone_unique()));
            }
        }

    private:

        /// \brief Processes canceled transactions by notifying callbacks.
        void process_calceled_transactions(std::list<transaction_t>& calceled_transactions);

        /// \brief Removes expired transactions from the pending list.
        /// \param current_time The current timestamp.
        /// \param calceled_transactions List to store canceled transactions.
        void remove_expired_transactions(int64_t current_time, std::list<transaction_t>& calceled_transactions);

        /// \brief Attempts to process the next transaction in the pending queue.
        /// \return A shared pointer to the next TradeTransactionEvent.
        std::shared_ptr<TradeTransactionEvent> get_next_transaction();

        /// \brief Checks if the trade request meets account and order conditions.
        /// \param request Shared pointer to the trade request.
        /// \return An OrderErrorCode indicating the validation result.
        OrderErrorCode check_transaction(const std::shared_ptr<TradeRequest>& request);

        /// \brief Finalizes a transaction with an error state.
        /// \param transaction Shared pointer to the trade transaction event to finalize.
        /// \param error_code Error code representing the issue with the transaction.
        /// \param state Final state of the transaction (typically an error state).
        /// \param timestamp Timestamp to use for the error finalization (e.g., current time).
        /// \param error_desc Optional error description. If not provided, the description will be set automatically based on the error code.
        void finalize_transaction_with_error(
                std::shared_ptr<TradeTransactionEvent>& transaction,
                OrderErrorCode error_code,
                OrderState state,
                int64_t timestamp,
                const std::string &error_desc = std::string());

    }; // OrderManagerModule

    bool OrderManagerModule::place_trade(std::unique_ptr<TradeRequest> request) {
        if (!request) return false;
        if (request->account == AccountType::UNKNOWN) {
            request->account = get_account_info<AccountType>(AccountInfoType::ACCOUNT_TYPE);
        }
        if (request->currency == CurrencyType::UNKNOWN) {
            request->currency = get_account_info<CurrencyType>(AccountInfoType::CURRENCY);
        }
        request->place_date = OPTIONX_TIMESTAMP_MS;

        auto trade_event = std::make_shared<TradeTransactionEvent>(request, get_api_type());
        std::lock_guard<std::mutex> lock(m_pending_transactions_mutex);
        m_pending_transactions.push_back(std::move(trade_event));
        return true;
    }

    void OrderManagerModule::finalize_all_trades() {
        std::unique_lock<std::mutex> lock(m_pending_transactions_mutex);
        auto pending_transactions = m_pending_transactions;
        m_pending_transactions.clear();
        lock.unlock();

        int64_t timestamp = OPTIONX_TIMESTAMP_MS;

        for (auto& transaction : pending_transactions) {
            finalize_transaction_with_error(transaction, OrderErrorCode::CLIENT_FORCED_CLOSE, OrderState::OPEN_ERROR, timestamp);
        }

        for (auto& transaction : m_open_transactions) {
            finalize_transaction_with_error(transaction, OrderErrorCode::CLIENT_FORCED_CLOSE, OrderState::CHECK_ERROR, timestamp);
            decrement_open_trades(transaction->request, transaction->result);
        }
    }

    void OrderManagerModule::process_pending_orders() {
        std::unique_lock<std::mutex> lock(m_pending_transactions_mutex);
        if (m_pending_transactions.empty()) return;

        std::list<transaction_t> calceled_transactions;
        const int64_t timestamp = OPTIONX_TIMESTAMP_MS;

        remove_expired_transactions(timestamp, calceled_transactions);

        for (;;) {
            if (m_pending_transactions.empty()) break;
            std::shared_ptr<TradeTransactionEvent> transaction = get_next_transaction();
            if (!transaction) break;
            lock.unlock();

            OrderErrorCode error_code = check_transaction(transaction);
            auto &request = transaction->request;
            auto &result  = transaction->result;
            if (error_code == OrderErrorCode::SUCCESS) {
                result->state       = result->current_state = OrderState::WAITING_OPEN;
                result->send_date   = OPTIONX_TIMESTAMP_MS;
                result->balance     = get_account_info(AccountInfoType::BALANCE, request);
                result->payout      = get_account_info(AccountInfoType::PAYOUT, request, time_shield::ms_to_sec(result->open_date));

                TradeRequestEvent trade_request_event(request, result);
                notify(trade_request_event);

                increment_open_trades(request, result);
                invoke_callback(transaction);

                m_open_transactions.push_back(std::move(transaction));
            } else {
                finalize_transaction_with_error(transaction, error_code, OrderState::OPEN_ERROR, timestamp);
            }

            process_calceled_transactions(calceled_transactions);
            return;
        }
        lock.unlock();
        process_calceled_transactions(calceled_transactions);
    }

    void OrderManagerModule::process_closing_orders() {
        const int64_t timestamp = OPTIONX_TIMESTAMP_MS;
        auto it = m_open_transactions.begin();
        while (it != m_open_transactions.end()) {
            auto& transaction = *it;
            auto& request = transaction->request;
            auto& result  = transaction->result;

            // Skip transactions not in states for closure or waiting
            if (result->state != OrderState::WAITING_CLOSE &&
                result->state != OrderState::OPEN_SUCCESS) {
                ++it;
                continue;
            }

            // Calculate close date based on the option type
            const int64_t close_date = result->close_date > 0 ?
                result->close_date : (request->option == OptionType::SPRINT ?
                (result->open_date > 0 ? (result->open_date + time_shield::sec_to_ms(request->duration)) :
                (result->send_date + time_shield::sec_to_ms(request->duration))) : (request->option == OptionType::CLASSIC ?
                time_shield::sec_to_ms(request->expiry_time) : 0));

            // Handle invalid close date
            if (close_date == 0) {
                OrderErrorCode error_code = request->option == OptionType::SPRINT ?
                    OrderErrorCode::INVALID_DURATION : OrderErrorCode::INVALID_EXPIRY_TIME;

                finalize_transaction_with_error(transaction, error_code, OrderState::CHECK_ERROR, timestamp);
                decrement_open_trades(request, result);
                it = m_open_transactions.erase(it);
                continue;
            }

            // Get the maximum allowed time for response
            const int64_t max_date = time_shield::sec_to_ms(get_account_info<int64_t>(AccountInfoType::RESPONSE_TIMEOUT)) + close_date;

            // If it's not time to close the option yet, skip
            if (timestamp < close_date) {
                ++it;
                continue;
            }

            // If the response timeout has been exceeded, finalize with an error
            if (timestamp > max_date) {
                finalize_transaction_with_error(transaction, OrderErrorCode::LONG_RESPONSE_WAIT, OrderState::CHECK_ERROR, timestamp);
                decrement_open_trades(request, result);
                it = m_open_transactions.erase(it);
                continue;
            }

            // Transition the state to WAITING_CLOSE and notify listeners
            if (result->state == OrderState::OPEN_SUCCESS) {
                result->state = OrderState::WAITING_CLOSE;
                TradeStatusEvent trade_status_event(request, result);
                notify(trade_status_event);
                invoke_callback(transaction);
            }

            ++it;
        }
    }

    void OrderManagerModule::process_finalizing_orders() {
        auto it = m_open_transactions.begin();
        while (it != m_open_transactions.end()) {
            auto& transaction = *it;
            auto& request = transaction->request;
            auto& result  = transaction->result;

            // Process transactions in terminal states
            if (result->state == OrderState::OPEN_ERROR ||
                result->state == OrderState::CHECK_ERROR ||
                result->state == OrderState::WIN ||
                result->state == OrderState::LOSS ||
                result->state == OrderState::STANDOFF ||
                result->state == OrderState::REFUND) {
                decrement_open_trades(request, result);
                invoke_callback(transaction);
                it = m_open_transactions.erase(it);
            } else {
                ++it;
            }
        }
    }

    void OrderManagerModule::process_calceled_transactions(std::list<transaction_t>& calceled_transactions) {
        const int64_t timestamp = OPTIONX_TIMESTAMP_MS;
        for (auto &transaction : calceled_transactions) {
            finalize_transaction_with_error(transaction, OrderErrorCode::LONG_QUEUE_WAIT, OrderState::OPEN_ERROR, timestamp);
        }
    }

    void OrderManagerModule::remove_expired_transactions(int64_t current_time, std::list<transaction_t>& calceled_transactions) {
        const int64_t order_queue_timeout = get_account_info<int64_t>(AccountInfoType::ORDER_QUEUE_TIMEOUT);

        auto it = m_pending_transactions.begin();
        while (it != m_pending_transactions.end()) {
            auto& transaction = *it;
            const int64_t delay = current_time - transaction->result->place_date;
            if (delay >= order_queue_timeout) {
                calceled_transactions.push_back(std::move(transaction));
                it = m_pending_transactions.erase(it);
            } else {
                ++it;
            }
        }
    }

    std::shared_ptr<TradeTransactionEvent> OrderManagerModule::get_next_transaction() {
        const int64_t order_interval = get_account_info<int64_t>(AccountInfoType::ORDER_INTERVAL_MS);
        auto now = std::chrono::steady_clock::now();
        auto elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last_order_time);

        if (elapsed_time.count() >= order_interval) {
            int64_t open_trades = get_account_info<int64_t>(AccountInfoType::OPEN_TRADES);
            if (open_trades < get_account_info<int64_t>(AccountInfoType::MAX_TRADES)) {
                auto it = m_pending_transactions.begin();
                std::shared_ptr<TradeTransactionEvent> transaction = std::make_shared<TradeTransactionEvent>(*it);
                m_pending_transactions.erase(it);
                return transaction;
            }
        }
        return nullptr;
    }

    OrderErrorCode OrderManagerModule::check_transaction(const std::shared_ptr<TradeRequest>& request) {
        if (request->symbol.empty()) return OrderErrorCode::INVALID_SYMBOL;
        const int64_t timestamp = time_shield::ms_to_sec(OPTIONX_TIMESTAMP_MS);

        if (!m_account_info) return OrderErrorCode::NO_CONNECTION;
        if (!get_account_info<bool>(AccountInfoType::CONNECTION_STATUS)) return OrderErrorCode::NO_CONNECTION;
        if (!get_account_info<bool>(request->symbol, timestamp)) return OrderErrorCode::INVALID_SYMBOL;
        if (!get_account_info<bool>(request->option, timestamp)) return OrderErrorCode::INVALID_OPTION;
        if (!get_account_info<bool>(request->order, timestamp)) return OrderErrorCode::INVALID_ORDER;
        if (!get_account_info<bool>(request->account, timestamp)) return OrderErrorCode::INVALID_ACCOUNT;
        if (!get_account_info<bool>(request->currency, timestamp)) return OrderErrorCode::INVALID_CURRENCY;
        if (!get_account_info<bool>(AccountInfoType::TRADE_LIMIT_NOT_EXCEEDED, request, timestamp)) return OrderErrorCode::LIMIT_OPEN_TRADES;
        if (!get_account_info<bool>(AccountInfoType::AMOUNT_BELOW_MAX, request, timestamp)) return OrderErrorCode::AMOUNT_TOO_HIGH;
        if (!get_account_info<bool>(AccountInfoType::AMOUNT_ABOVE_MIN, request, timestamp)) return OrderErrorCode::AMOUNT_TOO_LOW;
        if (!get_account_info<bool>(AccountInfoType::REFUND_BELOW_MAX, request, timestamp)) return OrderErrorCode::REFUND_TOO_HIGH;
        if (!get_account_info<bool>(AccountInfoType::REFUND_ABOVE_MIN, request, timestamp)) return OrderErrorCode::REFUND_TOO_LOW;
        if (!get_account_info<bool>(AccountInfoType::DURATION_AVAILABLE, request, timestamp)) return OrderErrorCode::INVALID_DURATION;
        if (!get_account_info<bool>(AccountInfoType::EXPIRATION_DATE_AVAILABLE, request, timestamp)) return OrderErrorCode::INVALID_EXPIRY_TIME;
        if (!get_account_info<bool>(AccountInfoType::PAYOUT_ABOVE_MIN, request, timestamp)) return OrderErrorCode::PAYOUT_TOO_LOW;
        if (!get_account_info<bool>(AccountInfoType::AMOUNT_BELOW_BALANCE, request, timestamp)) return OrderErrorCode::INSUFFICIENT_BALANCE;

        return OrderErrorCode::SUCCESS;
    }

    void OrderManagerModule::finalize_transaction_with_error(
            std::shared_ptr<TradeTransactionEvent>& transaction,
            OrderErrorCode error_code,
            OrderState state,
            int64_t timestamp,
            const std::string &error_desc = std::string()) {
        auto& request = transaction->request;
        auto& result = transaction->result;

        result->error_code = error_code;
        if (error_desc.empty()) result->error_desc = to_str(result->error_code);
        if (!result->send_date) result->send_date = timestamp;
        if (!result->open_date) result->open_date = timestamp;
        if (!result->close_date) result->close_date = timestamp;
        result->balance = get_account_info(AccountInfoType::BALANCE, request);
        if (!result->payout) result->payout = get_account_info(AccountInfoType::PAYOUT, request, result->send_date);
        result->state = result->current_state = state;

        invoke_callback(transaction);
    }

    /// \brief Increments the open trades counter and emits an event.
    /// \param request Shared pointer to the trade request details.
    /// \param result Shared pointer to the trade result details.
    void OrderManagerModule::increment_open_trades(
        const std::shared_ptr<TradeRequest>& request,
        const std::shared_ptr<TradeResult>& result) {
        m_open_trades++;
        OpenTradesEvent event(m_open_trades, request, result);
        notify(event);
    }

    /// \brief Decrements the open trades counter and emits an event.
    /// \param request Shared pointer to the trade request details.
    /// \param result Shared pointer to the trade result details.
    void OrderManagerModule::decrement_open_trades(
        const std::shared_ptr<TradeRequest>& request,
        const std::shared_ptr<TradeResult>& result) {
        if (m_open_trades > 0) {
            m_open_trades--;
            OpenTradesEvent event(m_open_trades, request, result);
            notify(event);
        }
    }

    template<class T>
    const T OrderManagerModule::get_account_info(const AccountInfoRequest& request) {
        m_account_info->get_account_info<T>(request);
    }

    template<class T>
    const T OrderManagerModule::get_account_info(AccountInfoType type, int64_t timestamp) {
        m_account_info->get_account_info<T>(type, timestamp);
    }

    template<class T>
    const T OrderManagerModule::get_account_info(const std::string &symbol, int64_t timestamp) {
        m_account_info->get_account_info<T>(symbol, timestamp);
    }

    template<class T>
    const T OrderManagerModule::get_account_info(OptionType option, int64_t timestamp) {
        m_account_info->get_account_info<T>(option, timestamp);
    }

    template<class T>
    const T OrderManagerModule::get_account_info(OrderType order, int64_t timestamp) {
        m_account_info->get_account_info<T>(order, timestamp);
    }

    template<class T>
    const T OrderManagerModule::get_account_info(AccountType account, int64_t timestamp) {
        m_account_info->get_account_info<T>(account, timestamp);
    }

    template<class T>
    const T OrderManagerModule::get_account_info(CurrencyType currency, int64_t timestamp) {
        m_account_info->get_account_info<T>(currency, timestamp);
    }

    template<class T>
    const T OrderManagerModule::get_account_info(AccountInfoType info_type, std::shared_ptr<TradeRequest>& trade_request, int64_t timestamp) {
        m_account_info->get_account_info<T>(info_type, trade_request, timestamp);
    }

} // namespace modules
} // namespace optionx

#endif // _OPTIONX_MODULES_ORDER_MANAGER_HPP_INCLUDED
