#pragma once
#ifndef _OPTIONX_MODULES_ORDER_MANAGER_HPP_INCLUDED
#define _OPTIONX_MODULES_ORDER_MANAGER_HPP_INCLUDED

/// \file OrderManager.hpp
/// \brief Contains the OrderManager class for managing trade requests and processing pending orders.

#include "events/TradeTransactionEvent.hpp"
#include "events/AccountInfoEvent.hpp"
#include "../pubsub/EventMediator.hpp"
#include <chrono>
#include <list>
#include <mutex>

namespace optionx {
namespace modules {

    /// \class OrderManager
    /// \brief Manages trade requests, checks order constraints, and processes pending orders.
    class OrderManager : public EventMediator {
    public:

        /// \brief Constructor initializes the last order time.
        OrderManager() {
            m_last_order_time = std::chrono::steady_clock::now();
        }

        /// \brief Places a trade request into the pending queue after validation.
        /// \param request Unique pointer to a trade request.
        /// \return True if the request is valid and added to the queue; false otherwise.
        bool place_trade(std::unique_ptr<TradeRequest> request);

        /// \brief Processes all pending orders in the queue.
        virtual void process() {
            process_pending_orders();
        }

    protected:
        using time_point_t = std::chrono::steady_clock::time_point;
        using transaction_t = std::shared_ptr<TradeTransactionEvent>;

        std::mutex                          m_pending_transactions_mutex; ///< Mutex for pending transactions list
        std::list<transaction_t>            m_pending_transactions;       ///< List of pending transactions

        std::shared_ptr<AccountInfoEvent>   m_account_info;               ///< Shared pointer to account information
        time_point_t                        m_last_order_time;            ///< Timestamp of the last processed order

        /// \brief Handles AccountInfoEvent updates.
        /// \param event Pointer to the event.
        void on_event(const Event* const event) override {}

        /// \brief Handles events received as shared pointers.
        /// \param event Shared pointer to the event.
        void on_event(const std::shared_ptr<Event>& event) override {
            if (auto msg = std::dynamic_pointer_cast<AccountInfoEvent>(event)) {
                m_account_info = msg;
            }
        }

        /// \brief Processes all pending trade transactions.
        void process_pending_orders();

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

        /// \brief Sends an order to be processed. Must be implemented by derived classes.
        /// \param transaction Trade transaction event to be processed.
        virtual void send_order(TradeTransactionEvent& transaction) = 0;

        /// \brief Sets account info data.
        /// \param type The type of account information.
        /// \param value The value to set.
        virtual void set_account_info(AccountInfoType type, int64_t value) = 0;

        /// \brief Retrieves the API type of the account.
        /// \return ApiType of the account.
        virtual ApiType get_api_type() = 0;

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

    }; // OrderManager

    bool OrderManager::place_trade(std::unique_ptr<TradeRequest> request) {
        if (!request) return false;
        if (request->account == AccountType::UNKNOWN) {
            request->account = get_account_info<AccountType>(AccountInfoType::ACCOUNT_TYPE);
        }
        if (request->currency == CurrencyType::UNKNOWN) {
            request->currency = get_account_info<CurrencyType>(AccountInfoType::CURRENCY);
        }

        auto event = std::make_shared<TradeTransactionEvent>(request, get_api_type());
        std::lock_guard<std::mutex> lock(m_pending_transactions_mutex);
        m_pending_transactions.push_back(std::move(event));
        return true;
    }

    void OrderManager::process_pending_orders() {
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
                result->payout      = get_account_info(AccountInfoType::PAYOUT, request, result->send_date);

                send_order(transaction);
                notify(transaction);
                request->invoke_callback(request, result);
            } else {
                result->error_desc = to_str(error_code);
                result->error_code = error_code;
                result->send_date = result->open_date = result->close_date = OPTIONX_TIMESTAMP_MS;
                result->balance = get_account_info(AccountInfoType::BALANCE, request);
                result->payout = get_account_info(AccountInfoType::PAYOUT, request, result->send_date);
                result->state = result->current_state = OrderState::OPEN_ERROR;

                notify(transaction);
                request->invoke_callback(request, result);
            }
            process_calceled_transactions(calceled_transactions);
            return;
        }
        lock.unlock();

        process_calceled_transactions(calceled_transactions);
    }

    void OrderManager::process_calceled_transactions(std::list<transaction_t>& calceled_transactions) {
        for (auto &transaction : calceled_transactions) {
            auto &request = transaction->request;
            auto &result = transaction->result;
            result->error_desc = to_str(OrderErrorCode::LONG_QUEUE_WAIT);
            result->error_code = OrderErrorCode::LONG_QUEUE_WAIT;
            result->send_date = result->open_date = result->close_date = OPTIONX_TIMESTAMP_MS;
            result->balance = get_account_info(AccountInfoType::BALANCE, request);
            result->payout = get_account_info(AccountInfoType::PAYOUT, request, result->send_date);
            result->state = result->current_state = OrderState::OPEN_ERROR;

            notify(transaction);
            request->invoke_callback(request, result);
        }
    }

    void OrderManager::remove_expired_transactions(int64_t current_time, std::list<transaction_t>& calceled_transactions) {
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

    OrderErrorCode OrderManager::check_transaction(const std::shared_ptr<TradeRequest>& request) {
        if (request->symbol.empty()) return OrderErrorCode::INVALID_SYMBOL;
        const int64_t timestamp = OPTIONX_TIMESTAMP_MS;

        if (!get_account_info<bool>(request->symbol, timestamp)) return OrderErrorCode::INVALID_SYMBOL;
        if (!get_account_info<bool>(request->option, timestamp)) return OrderErrorCode::INVALID_OPTION;
        if (!get_account_info<bool>(request->order, timestamp)) return OrderErrorCode::INVALID_ORDER;
        if (!get_account_info<bool>(request->account, timestamp)) return OrderErrorCode::INVALID_ACCOUNT;
        if (!get_account_info<bool>(request->currency, timestamp)) return OrderErrorCode::INVALID_CURRENCY;
        if (!get_account_info<bool>(AccountInfoType::ORDER_LIMIT_NOT_EXCEEDED, request, timestamp)) return OrderErrorCode::LIMIT_OPEN_ORDERS;
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

    std::shared_ptr<TradeTransactionEvent> OrderManager::get_next_transaction() {
        const int64_t order_interval = get_account_info<int64_t>(AccountInfoType::ORDER_INTERVAL_MS);
        auto now = std::chrono::steady_clock::now();
        auto elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last_order_time);

        if (elapsed_time.count() >= order_interval) {
            int64_t open_orders = get_account_info<int64_t>(AccountInfoType::OPEN_ORDERS);
            if (open_orders < get_account_info<int64_t>(AccountInfoType::MAX_ORDERS)) {
                auto it = m_pending_transactions.begin();
                std::shared_ptr<TradeTransactionEvent> transaction = std::make_shared<TradeTransactionEvent>(*it);
                m_pending_transactions.erase(it);
                return transaction;
            }
        }
        return nullptr;
    }

    template<class T>
    const T OrderManager::get_account_info(const AccountInfoRequest& request) {
        m_account_info->get_account_info<T>(request);
    }

    template<class T>
    const T OrderManager::get_account_info(AccountInfoType type, int64_t timestamp) {
        m_account_info->get_account_info<T>(type, timestamp);
    }

    template<class T>
    const T OrderManager::get_account_info(const std::string &symbol, int64_t timestamp) {
        m_account_info->get_account_info<T>(symbol, timestamp);
    }

    template<class T>
    const T OrderManager::get_account_info(OptionType option, int64_t timestamp) {
        m_account_info->get_account_info<T>(option, timestamp);
    }

    template<class T>
    const T OrderManager::get_account_info(OrderType order, int64_t timestamp) {
        m_account_info->get_account_info<T>(order, timestamp);
    }

    template<class T>
    const T OrderManager::get_account_info(AccountType account, int64_t timestamp) {
        m_account_info->get_account_info<T>(account, timestamp);
    }

    template<class T>
    const T OrderManager::get_account_info(CurrencyType currency, int64_t timestamp) {
        m_account_info->get_account_info<T>(currency, timestamp);
    }

    template<class T>
    const T OrderManager::get_account_info(AccountInfoType info_type, std::shared_ptr<TradeRequest>& trade_request, int64_t timestamp) {
        m_account_info->get_account_info<T>(info_type, trade_request, timestamp);
    }

} // namespace modules
} // namespace optionx

#endif // _OPTIONX_MODULES_ORDER_MANAGER_HPP_INCLUDED
