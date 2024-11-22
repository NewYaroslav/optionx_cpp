namespace optionx {
namespace modules {

    TradeState TradeManagerModule::process_transaction_state(
            const std::shared_ptr<TradeResult>& result,
            const std::shared_ptr<TradeRequest>& request,
            const TickInfo& tick) const {
        if (!result->open_price) {
            return TradeState::STANDOFF;
        }

        double open_close_price = tick.get_average_price();
        if (request->order_type == OrderType::BUY) {
            if (open_close_price > result->open_price) return TradeState::WIN;
            if (open_close_price < result->open_price) return TradeState::LOSS;
            return TradeState::STANDOFF;
        }

        if (request->order_type == OrderType::SELL) {
            if (open_close_price < result->open_price) return TradeState::WIN;
            if (open_close_price > result->open_price) return TradeState::LOSS;
            return TradeState::STANDOFF;
        }

        return TradeState::STANDOFF;
    }

    void TradeManagerModule::process_closing_transactions() {
        const int64_t timestamp = OPTIONX_TIMESTAMP_MS;
        auto it = m_open_transactions.begin();
        while (it != m_open_transactions.end()) {
            auto& transaction = *it;
            auto& request = transaction->request;
            auto& result  = transaction->result;

            if (result->trade_state == TradeState::OPEN_SUCCESS) {
                result->trade_state = result->live_state = TradeState::IN_PROGRESS;
                invoke_callback(transaction);
                ++it;
                continue;
            }

            // Skip transactions not in states for closure or waiting
            if (!utils_is_closable_state(result->trade_state)) {
                ++it;
                continue;
            }

            // Calculate close date based on the option type
            const int64_t close_date = utils_calculate_close_date(result, request);

            // Handle invalid close date
            if (close_date == 0) {
                result->error_code = request->option_type == OptionType::SPRINT ?
                    TradeErrorCode::INVALID_DURATION : TradeErrorCode::INVALID_EXPIRY_TIME;
                process_closing_error(transaction, timestamp);
                it = m_open_transactions.erase(it);
                continue;
            }

            // If it's not time to close the option yet, skip
            if (timestamp < close_date) {
                ++it;
                continue;
            }

            // If the response timeout has been exceeded, finalize with an error
            if (timestamp > (close_date + utils_get_max_response_time())) {
                result->error_code = TradeErrorCode::LONG_RESPONSE_WAIT;
                process_closing_error(transaction, timestamp);
                it = m_open_transactions.erase(it);
                continue;
            }

            // Transition the state to WAITING_CLOSE and notify listeners
            if (result->trade_state == TradeState::OPEN_SUCCESS ||
                result->trade_state == TradeState::IN_PROGRESS) {
                result->trade_state = result->live_state = TradeState::WAITING_CLOSE;
                invoke_callback(transaction);
                TradeStatusEvent trade_status_event(request, result);
                notify(trade_status_event);
            }

            ++it;
        }
    }

    void TradeManagerModule::process_finalizing_transactions() {
        auto it = m_open_transactions.begin();
        while (it != m_open_transactions.end()) {
            auto& transaction = *it;
            auto& request = transaction->request;
            auto& result  = transaction->result;

            // Process transactions in terminal states
            if (result->trade_state == TradeState::OPEN_ERROR ||
                result->trade_state == TradeState::CHECK_ERROR ||
                result->trade_state == TradeState::WIN ||
                result->trade_state == TradeState::LOSS ||
                result->trade_state == TradeState::STANDOFF ||
                result->trade_state == TradeState::REFUND) {
                queue_decrement_open_trades(request, result);
                invoke_callback(transaction);
                it = m_open_transactions.erase(it);
            } else {
                ++it;
            }
        }
    }

    void TradeManagerModule::process_closing_error(
            const std::shared_ptr<TradeTransactionEvent>& transaction,
            int64_t timestamp) {
        auto& result = transaction->result;
        auto& request = transaction->request;
        LOGIT_PRINT_INFO("CHECK_ERROR");
        queue_decrement_open_trades(request, result);
        utils_finalize_transaction_with_error(transaction, result->error_code, TradeState::CHECK_ERROR, timestamp);
    }

    void TradeManagerModule::process_finalize_all_trades() {
        std::list<transaction_t> pending_transactions;

        std::unique_lock<std::mutex> lock(m_pending_transactions_mutex);
        if (!m_pending_transactions.empty()) {
            pending_transactions.swap(m_pending_transactions);
        }
        lock.unlock();

        int64_t timestamp = OPTIONX_TIMESTAMP_MS;

        for (auto& transaction : pending_transactions) {
            utils_finalize_transaction_with_error(transaction, TradeErrorCode::CLIENT_FORCED_CLOSE, TradeState::OPEN_ERROR, timestamp);
        }

        for (auto& transaction : m_open_transactions) {
            queue_decrement_open_trades(transaction->request, transaction->result);
            utils_finalize_transaction_with_error(transaction, TradeErrorCode::CLIENT_FORCED_CLOSE, TradeState::CHECK_ERROR, timestamp);
        }
    }

} // namespace modules
} // namespace optionx
