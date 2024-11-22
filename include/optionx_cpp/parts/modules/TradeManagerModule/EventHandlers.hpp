namespace optionx {
namespace modules {

    void TradeManagerModule::on_event(const std::shared_ptr<Event>& event) {
        if (auto msg = std::dynamic_pointer_cast<AuthDataEvent>(event)) {
            handle_event(*msg);
        } else
        if (auto msg = std::dynamic_pointer_cast<PriceUpdateEvent>(event)) {
            handle_event(*msg);
        } else
        if (auto msg = std::dynamic_pointer_cast<DisconnectRequestEvent>(event)) {
            handle_event(*msg);
        }
    }

    void TradeManagerModule::on_event(const Event* const event) {
        if (const auto* msg = dynamic_cast<const AuthDataEvent*>(event)) {
            handle_event(*msg);
        } else
        if (const auto* msg = dynamic_cast<const PriceUpdateEvent*>(event)) {
            handle_event(*msg);
        } else
        if (const auto* msg = dynamic_cast<const DisconnectRequestEvent*>(event)) {
            handle_event(*msg);
        }
    }

    void TradeManagerModule::handle_event(const PriceUpdateEvent& event) {
        for (auto& transaction : m_open_transactions) {
            auto& request = transaction->request;
            auto& result = transaction->result;

            if (result->trade_state != TradeState::OPEN_SUCCESS &&
                result->trade_state != TradeState::IN_PROGRESS) continue;

            if (result->trade_state == TradeState::OPEN_SUCCESS) {
                result->live_state = TradeState::IN_PROGRESS;
                result->trade_state = TradeState::IN_PROGRESS;
            }

            auto tick = event.get_tick_by_symbol(request->symbol);
            if (!tick.is_initialized()) continue;

            result->close_price = tick.get_average_price();
            result->live_state = process_transaction_state(result, request, tick);
            invoke_callback(transaction);
        }
    }

    void TradeManagerModule::handle_event(const DisconnectRequestEvent& event) {
        process_finalize_all_trades();
    }

    void TradeManagerModule::invoke_callback(const transaction_t& transaction) {
        auto &request = transaction->request;
        auto &result  = transaction->result;

        notify(transaction.get());
        request->invoke_callbacks(request, result);

        std::lock_guard<std::mutex> lock(m_trade_result_mutex);
        if (m_trade_result_callback) {
            m_trade_result_callback(
                std::move(request->clone_unique()),
                std::move(result->clone_unique()));
        }
    }

} // namespace modules
} // namespace optionx
