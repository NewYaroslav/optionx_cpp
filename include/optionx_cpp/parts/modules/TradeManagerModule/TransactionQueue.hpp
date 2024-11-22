namespace optionx {
namespace modules {

    bool TradeManagerModule::queue_add_trade(std::unique_ptr<TradeRequest> request) {
        if (!request) return false;
        if (request->account_type == AccountType::UNKNOWN) {
            request->account_type = get_account_info<AccountType>(AccountInfoType::ACCOUNT_TYPE);
        }
        if (request->currency == CurrencyType::UNKNOWN) {
            request->currency = get_account_info<CurrencyType>(AccountInfoType::CURRENCY);
        }

        auto trade_event = std::make_shared<TradeTransactionEvent>(request, get_api_type());
        std::lock_guard<std::mutex> lock(m_pending_transactions_mutex);
        m_pending_transactions.push_back(std::move(trade_event));
        return true;
    }

    std::shared_ptr<TradeTransactionEvent> TradeManagerModule::queue_get_next_transaction(std::list<transaction_t>& pending_transactions) {
        const int64_t order_interval_ms = get_account_info<int64_t>(AccountInfoType::ORDER_INTERVAL_MS);
        auto now = std::chrono::steady_clock::now();
        auto elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last_order_time);

        if (elapsed_time.count() >= order_interval_ms) {
            auto it = pending_transactions.begin();
            auto transaction = *it;
            auto& request = transaction->request;
            int64_t open_trades = get_account_info<int64_t>(AccountInfoType::OPEN_TRADES, request);
            if (open_trades < get_account_info<int64_t>(AccountInfoType::MAX_TRADES, request)) {
                pending_transactions.erase(it);
                return transaction;
            }
        }
        return nullptr;
    }

    void TradeManagerModule::queue_process_canceled_transactions(std::list<transaction_t>& calceled_transactions) {
        const int64_t timestamp = OPTIONX_TIMESTAMP_MS;
        for (auto &transaction : calceled_transactions) {
            utils_finalize_transaction_with_error(transaction, TradeErrorCode::LONG_QUEUE_WAIT, TradeState::OPEN_ERROR, timestamp);
        }
    }

    void TradeManagerModule::queue_remove_expired_transactions(int64_t current_time_ms, std::list<transaction_t>& calceled_transactions) {
        const int64_t order_queue_timeout_ms = time_shield::sec_to_ms(get_account_info<int64_t>(AccountInfoType::ORDER_QUEUE_TIMEOUT));
        auto it = m_pending_transactions.begin();
        while (it != m_pending_transactions.end()) {
            auto& transaction = *it;
            const int64_t delay_ms = current_time_ms - transaction->result->place_date;
            if (delay_ms >= order_queue_timeout_ms) {
                calceled_transactions.push_back(std::move(transaction));
                it = m_pending_transactions.erase(it);
            } else {
                ++it;
            }
        }
    }

    void TradeManagerModule::process_pending_transactions() {
        std::unique_lock<std::mutex> lock(m_pending_transactions_mutex);
        if (m_pending_transactions.empty()) return;

        std::list<transaction_t> calceled_transactions;
        const int64_t timestamp = OPTIONX_TIMESTAMP_MS;

        queue_remove_expired_transactions(timestamp, calceled_transactions);

        for (;;) {
            if (m_pending_transactions.empty()) break;
            std::shared_ptr<TradeTransactionEvent> transaction = queue_get_next_transaction(m_pending_transactions);
            if (!transaction) break;
            lock.unlock();

            auto &request = transaction->request;
            auto &result  = transaction->result;
            result->error_code = check_request(request);
            if (result->error_code == TradeErrorCode::SUCCESS) {
                result->trade_state       = result->live_state = TradeState::WAITING_OPEN;
                result->send_date   = OPTIONX_TIMESTAMP_MS;
                result->balance     = get_account_info<double>(AccountInfoType::BALANCE, request);
                result->payout      = get_account_info<double>(AccountInfoType::PAYOUT, request, time_shield::ms_to_sec(result->open_date));

                queue_increment_open_trades(request, result);
                invoke_callback(transaction);

                TradeRequestEvent trade_request_event(request, result);
                notify(trade_request_event);

                m_open_transactions.push_back(std::move(transaction));
            } else {
                utils_finalize_transaction_with_error(transaction, result->error_code, TradeState::OPEN_ERROR, timestamp);
            }

            queue_process_canceled_transactions(calceled_transactions);
            return;
        }
        lock.unlock();
        queue_process_canceled_transactions(calceled_transactions);
    }

    void TradeManagerModule::queue_increment_open_trades(
        const std::shared_ptr<TradeRequest>& request,
        const std::shared_ptr<TradeResult>& result) {
        m_open_trades++;
        OpenTradesEvent event(m_open_trades, request, result);
        notify(event);
    }

    void TradeManagerModule::queue_decrement_open_trades(
        const std::shared_ptr<TradeRequest>& request,
        const std::shared_ptr<TradeResult>& result) {
        if (m_open_trades > 0) {
            m_open_trades--;
            OpenTradesEvent event(m_open_trades, request, result);
            notify(event);
        }
    }

} // namespace modules
} // namespace optionx
