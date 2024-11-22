#pragma once
#ifndef _OPTIONX_MODULES_TRADE_MANAGER_MODULE_HPP_INCLUDED
#define _OPTIONX_MODULES_TRADE_MANAGER_MODULE_HPP_INCLUDED

/// \file TradeManagerModule.hpp
/// \brief Contains the TradeManagerModule class for managing trade requests and processing pending orders.

#include <time_shield_cpp/time_shield.hpp>
#ifndef OPTIONX_TIMESTAMP_MS
#define OPTIONX_TIMESTAMP_MS time_shield::timestamp_ms()
#endif

#include "../pubsub/EventMediator.hpp"
#include "../interfaces/IAccountInfoData.hpp"
#include "events/AuthDataEvent.hpp"
#include "events/AccountInfoEvent.hpp"
#include "events/TradeTransactionEvent.hpp"
#include "events/TradeRequestEvent.hpp"
#include "events/TradeStatusEvent.hpp"
#include "events/OpenTradesEvent.hpp"
#include "events/PriceUpdateEvent.hpp"
#include "events/DisconnectRequestEvent.hpp"
#include <log-it/LogIt.hpp>
#include <chrono>
#include <list>
#include <mutex>

namespace optionx {
namespace modules {

    /// \class TradeManagerModule
    /// \brief Manages trade requests, checks order constraints, and processes pending orders.
    class TradeManagerModule : public EventMediator {
    public:
        using time_point_t = std::chrono::steady_clock::time_point;
        using transaction_t = std::shared_ptr<TradeTransactionEvent>;
        using trade_result_callback_t = std::function<void(std::unique_ptr<TradeRequest>, std::unique_ptr<TradeResult>)>;

        /// \brief Constructor for the TradeManagerModule class.
        /// \param hub Reference to the EventHub for subscribing to events.
        /// \param account_info Shared pointer to the IAccountInfoData interface for accessing account details.
        explicit TradeManagerModule(EventHub& hub, std::shared_ptr<IAccountInfoData> account_info)
            : EventMediator(hub), m_account_info(std::move(account_info)) {
            m_last_order_time = std::chrono::steady_clock::now();
            subscribe<AccountInfoEvent>(this);
            subscribe<AuthDataEvent>(this);
            subscribe<PriceUpdateEvent>(this);
            subscribe<DisconnectRequestEvent>(this);
        }

        /// \brief Default virtual destructor.
        virtual ~TradeManagerModule() = default;

        /// \brief Handles an event notification received as a shared pointer.
        /// \param event The event received, passed as a shared pointer.
        void on_event(const std::shared_ptr<Event>& event) override;

        /// \brief Handles an event notification received as a raw pointer.
        /// \param event The event received, passed as a raw pointer.
        void on_event(const Event* const event) override;

        /// \brief Sets a callback for trade result events, including open, close, and errors.
        /// \param callback Function to handle trade result events.
        void set_trade_result_callback(trade_result_callback_t callback) {
            std::lock_guard<std::mutex> lock(m_trade_result_mutex);
            m_trade_result_callback = std::move(callback);
        }

        /// \brief Places a trade request into the pending queue after validation.
        /// \param request Unique pointer to a trade request.
        /// \return True if the request is valid and added to the queue; false otherwise.
        bool place_trade(std::unique_ptr<TradeRequest> request) {
            return queue_add_trade(std::move(request));
        }

        /// \brief Processes all pending trades in the queue.
        virtual void process() {
            process_pending_transactions();
            process_closing_transactions();
            process_finalizing_transactions();
        }

        void shutdown() {
            process_finalize_all_trades();
        }

    protected:

        /// \brief Handles the authorization data event.
        /// \param event The authorization data event.
        virtual void handle_event(const AuthDataEvent& event) = 0;

        /// \brief Handles a price update event.
        /// \param event The price update event containing the latest price information.
        virtual void handle_event(const PriceUpdateEvent& event);

        /// \brief Handles a disconnect request event.
        /// \param event The disconnect request event.
        virtual void handle_event(const DisconnectRequestEvent& event);

        /// \brief Processes the current state of the transaction based on order type and prices.
        /// \param result Shared pointer to the transaction result.
        /// \param request Shared pointer to the order request.
        /// \param tick The tick information associated with the symbol.
        /// \return The new order state.
        virtual TradeState process_transaction_state(
                const std::shared_ptr<TradeResult>& result,
                const std::shared_ptr<TradeRequest>& request,
                const TickInfo& tick) const;

        /// \brief Retrieves the API type of the account.
        /// \return ApiType of the account.
        virtual ApiType get_api_type() = 0;

        // Trade Processing

        /// \brief Processes all pending trade transactions.
        void process_pending_transactions();

        /// \brief Processes open trades that are ready to be closed or transitioned into a waiting state.
        void process_closing_transactions();

        /// \brief Finalizes transactions in their terminal states, notifies listeners, and removes them from tracking.
        void process_finalizing_transactions();


        void process_closing_error(
            const transaction_t& transaction,
            int64_t timestamp);

        /// \brief Finalizes all active and pending trades.
        void process_finalize_all_trades();

        /// \brief Invokes the callback for a completed transaction.
        /// \param transaction Shared pointer to the transaction event.
        void invoke_callback(const transaction_t& transaction);

        // Transaction Queue

        /// \brief Adds a trade request to the pending queue.
        /// \param request Unique pointer to the trade request.
        /// \return True if the trade request was successfully added; false otherwise.
        bool queue_add_trade(std::unique_ptr<TradeRequest> request);

        /// \brief Increments the open trades counter and emits an event.
        /// \param request Shared pointer to the trade request details.
        /// \param result Shared pointer to the trade result details.
        void queue_increment_open_trades(
            const std::shared_ptr<TradeRequest>& request,
            const std::shared_ptr<TradeResult>& result);

        /// \brief Decrements the open trades counter and emits an event.
        /// \param request Shared pointer to the trade request details.
        /// \param result Shared pointer to the trade result details.
        void queue_decrement_open_trades(
            const std::shared_ptr<TradeRequest>& request,
            const std::shared_ptr<TradeResult>& result);

        // Utilities

        /// \brief Retrieves account information based on the request.
        template<class T>
        const T get_account_info(const AccountInfoRequest& request) const;

        /// \brief Retrieves account information based on type and optional timestamp.
        template<class T>
        const T get_account_info(AccountInfoType type, int64_t timestamp = 0) const;

        template<class T>
        const T get_account_info(const std::string &symbol, int64_t timestamp) const;

        template<class T>
        const T get_account_info(OptionType option, int64_t timestamp = 0) const;

        template<class T>
        const T get_account_info(OrderType order, int64_t timestamp = 0) const;

        template<class T>
        const T get_account_info(AccountType account, int64_t timestamp = 0) const;

        template<class T>
        const T get_account_info(CurrencyType currency, int64_t timestamp = 0) const;

        template<class T>
        const T get_account_info(AccountInfoType info_type, const std::shared_ptr<TradeRequest>& trade_request, int64_t timestamp = 0) const;

        std::shared_ptr<IAccountInfoData> m_account_info;  ///< Shared pointer to account information.
        std::mutex  m_pending_transactions_mutex;           ///< Mutex for pending transactions list.
        std::list<transaction_t> m_pending_transactions;    ///< List of pending transactions.
        std::list<transaction_t> m_open_transactions;       ///< List of open transactions.
        time_point_t m_last_order_time;                     ///< Timestamp of the last processed order.
        int64_t      m_open_trades = 0;                     ///< Number of currently tracked open trades (confirmed + pending).
        mutable std::mutex      m_trade_result_mutex;       ///< Mutex for the trade result callback.
        trade_result_callback_t m_trade_result_callback;    ///< Callback function for handling trade results.
    private:

        // Transaction Queue

        /// \brief Processes canceled transactions by notifying callbacks.
        /// \param calceled_transactions List of canceled transactions.
        void queue_process_canceled_transactions(std::list<transaction_t>& calceled_transactions);

        /// \brief Removes expired transactions from the pending list.
        /// \param current_time The current timestamp.
        /// \param calceled_transactions List to store canceled transactions.
        void queue_remove_expired_transactions(int64_t current_time, std::list<transaction_t>& calceled_transactions);

        /// \brief Attempts to process the next transaction in the pending queue.
        /// \param pending_transactions Reference to the pending transactions list.
        /// \return A shared pointer to the next TradeTransactionEvent.
        transaction_t queue_get_next_transaction(std::list<transaction_t>& pending_transactions);

        // Validation

        /// \brief Checks if the trade request meets account and order conditions.
        /// \param request Shared pointer to the trade request.
        /// \return A TradeErrorCode indicating the validation result.
        TradeErrorCode check_request(const std::shared_ptr<TradeRequest>& request);

        // Utilities

        /// \brief Finalizes a transaction with an error state.
        /// \param transaction Shared pointer to the trade transaction event to finalize.
        /// \param error_code Error code representing the issue with the transaction.
        /// \param state Final state of the transaction (typically an error state).
        /// \param timestamp Timestamp to use for the error finalization (e.g., current time).
        /// \param error_desc Optional error description. If not provided, the description will be set automatically based on the error code.
        void utils_finalize_transaction_with_error(
                const transaction_t& transaction,
                TradeErrorCode error_code,
                TradeState state,
                int64_t timestamp,
                const std::string &error_desc = std::string());

        bool utils_is_closable_state(TradeState state) const;

        int64_t utils_calculate_close_date(
                const std::shared_ptr<TradeResult>& result,
                const std::shared_ptr<TradeRequest>& request) const;

        int64_t utils_get_max_response_time();
    };

} // namespace modules
} // namespace optionx

#include "TradeManagerModule/Utils.hpp"
#include "TradeManagerModule/RequestValidator.hpp"
#include "TradeManagerModule/EventHandlers.hpp"
#include "TradeManagerModule/TransactionQueue.hpp"
#include "TradeManagerModule/TradeProcessing.hpp"

#endif // _OPTIONX_MODULES_TRADE_MANAGER_MODULE_HPP_INCLUDED
