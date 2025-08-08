#pragma once
#ifndef _OPTIONX_MODULES_TRADE_QUEUE_MANAGER_HPP_INCLUDED
#define _OPTIONX_MODULES_TRADE_QUEUE_MANAGER_HPP_INCLUDED

/// \file TradeQueueManager.hpp
/// \brief Manages the trade request queue, processes transactions, and handles trade events.

namespace optionx::modules {

    /// \class TradeQueueManager
    /// \brief Manages the queue of trade transactions and processes them accordingly.
    ///
    /// ### Subscribed events:
    /// - `PriceUpdateEvent`: Updates trade states based on market price movements.
    /// - `DisconnectRequestEvent`: Handles connection loss and finalizes active trades.
    ///
    /// ### Emitted events:
    /// - `TradeRequestEvent`: Notifies when a trade request is sent.
    /// - `TradeStatusEvent`: Reports trade state changes.
    /// - `OpenTradesEvent`: Notifies about open trade count updates.
    class TradeQueueManager : public utils::EventMediator {
    public:
        using transaction_t = std::shared_ptr<events::TradeTransactionEvent>;
        using trade_result_callback_t = std::function<void(std::unique_ptr<TradeRequest>, std::unique_ptr<TradeResult>)>;

        /// \brief Constructs a TradeQueueManager instance.
        /// \param bus Event bus for event-based communication.
        /// \param account_info Account information provider.
        /// \param trade_state_manager Manages trade state transitions.
        explicit TradeQueueManager(
                utils::EventBus& bus,
                AccountInfoProvider& account_info,
                TradeStateManager& trade_state_manager)
            : EventMediator(bus), m_account_info(account_info),
              m_trade_state_manager(trade_state_manager) {
            m_last_order_time = std::chrono::steady_clock::now();
            subscribe<events::PriceUpdateEvent>();
            subscribe<events::DisconnectRequestEvent>();
        }

        /// \brief Virtual destructor.
        virtual ~TradeQueueManager() = default;

        /// \brief Handles an event notification received as a raw pointer.
        /// \param event The received event.
        void on_event(const utils::Event* const event) override {
            if (const auto* msg = dynamic_cast<const events::PriceUpdateEvent*>(event)) {
                handle_event(*msg);
            } else
            if (const auto* msg = dynamic_cast<const events::DisconnectRequestEvent*>(event)) {
                handle_event(*msg);
            }
        };

        /// \brief Adds a trade request to the processing queue.
        /// \tparam PreprocessFunctor Type of the preprocessing functor.
        /// \param request Unique pointer to a trade request.
        /// \param platform_type The platform type associated with the request.
        /// \param preprocess Functor to preprocess the trade request and result.
        /// \return True if the trade request was successfully added; false otherwise.
        template<typename PreprocessFunction>
        bool add_trade(
            std::unique_ptr<TradeRequest> request,
            PlatformType platform_type,
            PreprocessFunction preprocess);

        /// \brief Sets a callback for trade result events.
        /// \param callback Function to handle trade result events.
        void set_trade_result_callback(trade_result_callback_t callback);

        /// \brief
        /// \return
        trade_result_callback_t& on_trade_result() {
            return m_trade_result_callback;
        }

        /// \brief Processes all pending, closing, and finalizing transactions.
        virtual void process() {
            process_pending_transactions();
            process_closing_transactions();
            process_finalizing_transactions();
        }

         /// \brief Finalizes all active and pending trades.
        void finalize_all_trades();

    protected:

        /// \brief Retrieves and removes the next transaction from the pending queue.
        /// \return The next transaction to process, or nullptr if none available.
        transaction_t pop_next_transaction();

        /// \brief Handles all canceled transactions.
        void handle_canceled_transactions(std::list<transaction_t>& canceled_transactions);

        /// \brief Handles an error when closing a transaction.
        void handle_closing_error(
                const transaction_t& transaction,
                int64_t timestamp);

        /// \brief Processes all pending transactions.
        void process_pending_transactions();

        /// \brief Processes transactions that need to be closed.
        void process_closing_transactions();

        /// \brief Processes transactions that need to be finalized.
        void process_finalizing_transactions();

        /// \brief Handles an error while closing a trade.
        /// \param transaction The transaction that encountered an error.
        /// \param timestamp Current timestamp.
        void clean_expired_transactions(
                int64_t current_time_ms,
                std::list<transaction_t>& canceled_transactions);

        /// \brief Increments the open trades counter and emits an OpenTradesEvent.
        void increment_open_trades(
                const std::shared_ptr<TradeRequest>& request,
                const std::shared_ptr<TradeResult>& result);

        /// \brief Decrements the open trades counter and emits an OpenTradesEvent.
        void decrement_open_trades(
                const std::shared_ptr<TradeRequest>& request,
                const std::shared_ptr<TradeResult>& result);

    private:
        AccountInfoProvider&     m_account_info;        ///< Reference to account information provider.
        TradeStateManager&       m_trade_state_manager; ///< Manages trade states and transitions.
        mutable std::mutex       m_trade_result_mutex;  ///< Mutex for the trade result callback.
        trade_result_callback_t  m_trade_result_callback; ///< Callback for handling trade results.
        std::mutex               m_pending_mutex;      ///< Mutex for pending transactions list.
        std::list<transaction_t> m_pending_transactions; ///< List of pending transactions.
        std::list<transaction_t> m_open_transactions;  ///< List of open transactions.
        std::chrono::steady_clock::time_point m_last_order_time; ///< Last processed order timestamp.
        int64_t                  m_open_trades = 0;   ///< Number of currently tracked open trades

        /// \brief Dispatches a trade event notification.
        /// \param transaction The trade transaction event to be dispatched.
        void dispatch_trade_event(const transaction_t& transaction);

        /// \brief Handles incoming price updates.
        /// \param event The received price update event.
        void handle_event(const events::PriceUpdateEvent& event);

        /// \brief Handles incoming disconnect requests.
        /// \param event The received disconnect request event.
        void handle_event(const events::DisconnectRequestEvent& event);
    };

    template<typename PreprocessFunction>
    bool TradeQueueManager::add_trade(
            std::unique_ptr<TradeRequest> request,
            PlatformType platform_type,
            PreprocessFunction preprocess) {
        LOGIT_0TRACE();
        if (!request) return false;
        if (request->account_type == AccountType::UNKNOWN) {
            request->account_type = m_account_info.get_info<AccountType>(AccountInfoType::ACCOUNT_TYPE);
        }
        if (request->currency == CurrencyType::UNKNOWN) {
            request->currency = m_account_info.get_info<CurrencyType>(AccountInfoType::CURRENCY);
        }
        auto result = request->create_trade_result_unique();
        result->trade_id = utils::TradeIdGenerator::instance().generate_id();
        result->place_date = OPTIONX_TIMESTAMP_MS;
        result->platform_type = platform_type;
        if (!preprocess(request, result)) return false;

        LOGIT_0TRACE();
        auto trade_event = std::make_shared<events::TradeTransactionEvent>(request, result);
        std::lock_guard<std::mutex> lock(m_pending_mutex);
        m_pending_transactions.push_back(std::move(trade_event));
        return true;
    }

    void TradeQueueManager::set_trade_result_callback(trade_result_callback_t callback) {
        std::lock_guard<std::mutex> lock(m_trade_result_mutex);
        m_trade_result_callback = std::move(callback);
    }

    TradeQueueManager::transaction_t TradeQueueManager::pop_next_transaction() {
        const int64_t order_interval_ms = m_account_info.get_info<int64_t>(AccountInfoType::ORDER_INTERVAL_MS);
        auto now = std::chrono::steady_clock::now();
        auto elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last_order_time);

        if (elapsed_time.count() >= order_interval_ms) {
            auto it = m_pending_transactions.begin();
            auto transaction = *it;
            auto& request = transaction->request;
            int64_t open_trades = m_account_info.get_for_trade<int64_t>(AccountInfoType::OPEN_TRADES, request);
            if (open_trades < m_account_info.get_for_trade<int64_t>(AccountInfoType::MAX_TRADES, request)) {
                m_pending_transactions.erase(it);
                return transaction;
            }
        }
        return nullptr;
    }

    void TradeQueueManager::handle_canceled_transactions(std::list<transaction_t>& calceled_transactions) {
        const int64_t timestamp = OPTIONX_TIMESTAMP_MS;
        for (const auto &transaction : calceled_transactions) {
            m_trade_state_manager.finalize_transaction_with_error(
                transaction,
                TradeErrorCode::LONG_QUEUE_WAIT,
                TradeState::OPEN_ERROR,
                timestamp);
        }
    }

    void TradeQueueManager::clean_expired_transactions(
            int64_t current_time_ms,
            std::list<transaction_t>& calceled_transactions) {
        const int64_t timeout_ms = time_shield::sec_to_ms(m_account_info.get_info<int64_t>(AccountInfoType::ORDER_QUEUE_TIMEOUT));
        auto it = m_pending_transactions.begin();
        while (it != m_pending_transactions.end()) {
            auto& transaction = *it;
            const int64_t delay_ms = current_time_ms - transaction->result->place_date;
            if (delay_ms >= timeout_ms) {
                calceled_transactions.push_back(std::move(transaction));
                it = m_pending_transactions.erase(it);
            } else {
                ++it;
            }
        }
    }

    void TradeQueueManager::handle_closing_error(
            const transaction_t& transaction,
            int64_t timestamp) {
        auto& result = transaction->result;
        auto& request = transaction->request;
        LOGIT_0FATAL();
        decrement_open_trades(request, result);
        m_trade_state_manager.finalize_transaction_with_error(transaction, result->error_code, TradeState::CHECK_ERROR, timestamp);
    }

    void TradeQueueManager::process_pending_transactions() {
        std::unique_lock<std::mutex> lock(m_pending_mutex);
        if (m_pending_transactions.empty()) return;

        std::list<transaction_t> calceled_transactions;
        const int64_t timestamp = OPTIONX_TIMESTAMP_MS;

        clean_expired_transactions(timestamp, calceled_transactions);

        for (;;) {
            if (m_pending_transactions.empty()) break;
            std::shared_ptr<events::TradeTransactionEvent> transaction = pop_next_transaction();
            if (!transaction) break;
            lock.unlock();

            auto &request = transaction->request;
            auto &result  = transaction->result;
            result->error_code = m_trade_state_manager.validate_request(request);
            if (result->error_code == TradeErrorCode::SUCCESS) {
                LOGIT_0TRACE();
                result->trade_state = result->live_state = TradeState::WAITING_OPEN;
                result->send_date   = OPTIONX_TIMESTAMP_MS;
                result->balance     = m_account_info.get_for_trade<double>(AccountInfoType::BALANCE, request);
                result->payout      = m_account_info.get_for_trade<double>(AccountInfoType::PAYOUT, request, time_shield::ms_to_sec(result->send_date));

                increment_open_trades(request, result);
                dispatch_trade_event(transaction);

                events::TradeRequestEvent trade_request_event(request, result);
                notify(trade_request_event);

                m_open_transactions.push_back(std::move(transaction));
            } else {
                LOGIT_0TRACE();
                m_trade_state_manager.finalize_transaction_with_error(transaction, result->error_code, TradeState::OPEN_ERROR, timestamp);
                dispatch_trade_event(transaction);
            }

            handle_canceled_transactions(calceled_transactions);
            return;
        }
        lock.unlock();
        handle_canceled_transactions(calceled_transactions);
    }

    void TradeQueueManager::process_closing_transactions() {
        if (m_open_transactions.empty()) return;
        const int64_t timestamp = OPTIONX_TIMESTAMP_MS;
        auto it = m_open_transactions.begin();
        while (it != m_open_transactions.end()) {
            auto& transaction = *it;
            auto& request = transaction->request;
            auto& result  = transaction->result;

            if (result->trade_state == TradeState::OPEN_SUCCESS) {
                LOGIT_0TRACE();
                dispatch_trade_event(transaction);
                result->trade_state = result->live_state = TradeState::IN_PROGRESS;
                ++it;
                continue;
            }

            // Skip transactions not in states for closure or waiting
            if (!m_trade_state_manager.is_closable_state(result->trade_state)) {
                ++it;
                continue;
            }

            // Calculate close date based on the option type
            const int64_t close_date = m_trade_state_manager.calculate_close_date(result, request);

            // Handle invalid close date
            if (close_date == 0) {
                LOGIT_0TRACE();
                result->error_code = request->option_type == OptionType::SPRINT ?
                    TradeErrorCode::INVALID_DURATION : TradeErrorCode::INVALID_EXPIRY_TIME;
                handle_closing_error(transaction, timestamp);
                it = m_open_transactions.erase(it);
                continue;
            }

            // If it's not time to close the option yet, skip
            if (timestamp < close_date) {
                ++it;
                continue;
            }

            // If the response timeout has been exceeded, finalize with an error
            if (timestamp > (close_date + m_account_info.get_response_timeout())) {
                LOGIT_0TRACE();
                result->error_code = TradeErrorCode::LONG_RESPONSE_WAIT;
                handle_closing_error(transaction, timestamp);
                it = m_open_transactions.erase(it);
                continue;
            }

            // Transition the state to WAITING_CLOSE and notify listeners
            if (m_trade_state_manager.is_transition_to_waiting_close(result->trade_state)) {
                LOGIT_0TRACE();
                result->trade_state = result->live_state = TradeState::WAITING_CLOSE;
                dispatch_trade_event(transaction);
                events::TradeStatusEvent trade_status_event(request, result);
                notify(trade_status_event);
            }

            ++it;
        }
    }

    void TradeQueueManager::process_finalizing_transactions() {
        auto it = m_open_transactions.begin();
        while (it != m_open_transactions.end()) {
            auto& transaction = *it;
            auto& request = transaction->request;
            auto& result  = transaction->result;

            // Process transactions in terminal states
            if (m_trade_state_manager.is_terminal_state(result->trade_state)) {
                LOGIT_0TRACE();
                decrement_open_trades(request, result);
                dispatch_trade_event(transaction);
                it = m_open_transactions.erase(it);
            } else {
                ++it;
            }
        }
    }

    void TradeQueueManager::finalize_all_trades() {
        LOGIT_0TRACE();

        std::list<transaction_t> pending_transactions;
        std::unique_lock<std::mutex> lock(m_pending_mutex);
        if (!m_pending_transactions.empty()) {
            pending_transactions.swap(m_pending_transactions);
        }
        lock.unlock();

        int64_t timestamp = OPTIONX_TIMESTAMP_MS;
        for (auto& transaction : pending_transactions) {
            m_trade_state_manager.finalize_transaction_with_error(transaction, TradeErrorCode::CLIENT_FORCED_CLOSE, TradeState::OPEN_ERROR, timestamp);
            dispatch_trade_event(transaction);
        }

        for (auto& transaction : m_open_transactions) {
            m_trade_state_manager.finalize_transaction_with_error(transaction, TradeErrorCode::CLIENT_FORCED_CLOSE, TradeState::CHECK_ERROR, timestamp);
            decrement_open_trades(transaction->request, transaction->result);
            dispatch_trade_event(transaction);
        }
        m_open_transactions.clear();
    }

    void TradeQueueManager::increment_open_trades(
        const std::shared_ptr<TradeRequest>& request,
        const std::shared_ptr<TradeResult>& result) {
        m_open_trades++;
        events::OpenTradesEvent event(m_open_trades, request, result);
        notify(event);
    }

    void TradeQueueManager::decrement_open_trades(
        const std::shared_ptr<TradeRequest>& request,
        const std::shared_ptr<TradeResult>& result) {
        if (m_open_trades > 0) {
            m_open_trades--;
            events::OpenTradesEvent event(m_open_trades, request, result);
            notify(event);
        }
    }

    void TradeQueueManager::dispatch_trade_event(const transaction_t& transaction) {
        auto &request = transaction->request;
        auto &result  = transaction->result;

        notify(transaction.get());
        request->dispatch_callbacks(request, result);

        std::lock_guard<std::mutex> lock(m_trade_result_mutex);
        if (m_trade_result_callback) {
            m_trade_result_callback(
                std::move(request->clone_unique()),
                std::move(result->clone_unique()));
        }
    }

    void TradeQueueManager::handle_event(const events::PriceUpdateEvent& event) {
        for (auto& transaction : m_open_transactions) {
            auto& request = transaction->request;
            auto& result = transaction->result;

            if (result->trade_state != TradeState::OPEN_SUCCESS &&
                result->trade_state != TradeState::IN_PROGRESS) continue;

            if (result->trade_state == TradeState::OPEN_SUCCESS) {
                LOGIT_0TRACE();
                dispatch_trade_event(transaction);
                result->live_state  = TradeState::IN_PROGRESS;
                result->trade_state = TradeState::IN_PROGRESS;
            }

            auto tick = event.get_tick_by_symbol(request->symbol);
            if (!tick.has_flag(TickStatusFlags::INITIALIZED)) continue;

            result->close_price = tick.mid_price();
            result->live_state = m_trade_state_manager.determine_trade_state(result, request, tick);
            dispatch_trade_event(transaction);
        }
    }

    void TradeQueueManager::handle_event(const events::DisconnectRequestEvent& event) {
        finalize_all_trades();
    }

} // namespace optionx::modules

#endif // _OPTIONX_TRADE_QUEUE_MANAGER_HPP_INCLUDED
