namespace optionx {
namespace modules {

    bool TradeManagerModule::utils_is_closable_state(TradeState state) const {
        return state == TradeState::WAITING_CLOSE ||
               state == TradeState::OPEN_SUCCESS ||
               state == TradeState::IN_PROGRESS;
    }

    int64_t TradeManagerModule::utils_calculate_close_date(
            const std::shared_ptr<TradeResult>& result,
            const std::shared_ptr<TradeRequest>& request) const {
        if (result->close_date > 0) return result->close_date;
        if (request->option_type == OptionType::SPRINT) {
            return result->open_date > 0
                ? result->open_date + time_shield::sec_to_ms(request->duration)
                ? result->send_date
                : result->send_date + time_shield::sec_to_ms(request->duration)
                ? result->place_date
                : result->place_date + time_shield::sec_to_ms(request->duration)
                : 0;
        }
        if (request->option_type == OptionType::CLASSIC) {
            return time_shield::sec_to_ms(request->expiry_time);
        }
        return 0;
    }

    int64_t TradeManagerModule::utils_get_max_response_time() {
        return time_shield::sec_to_ms(get_account_info<int64_t>(AccountInfoType::RESPONSE_TIMEOUT));
    }

    void TradeManagerModule::utils_finalize_transaction_with_error(
            const std::shared_ptr<TradeTransactionEvent>& transaction,
            TradeErrorCode error_code,
            TradeState state,
            int64_t timestamp,
            const std::string &error_desc) {
        auto& request = transaction->request;
        auto& result = transaction->result;

        result->error_code = error_code;
        if (error_desc.empty()) result->error_desc = to_str(result->error_code);
        if (!result->send_date) result->send_date = timestamp;
        if (!result->open_date) result->open_date = timestamp;
        if (!result->close_date) result->close_date = timestamp;
        result->balance = get_account_info<double>(AccountInfoType::BALANCE, request, result->send_date);
        if (!result->payout) result->payout = get_account_info<double>(AccountInfoType::PAYOUT, request, result->send_date);
        result->trade_state = result->live_state = state;

        invoke_callback(transaction);
    }

    template<class T>
    const T TradeManagerModule::get_account_info(const AccountInfoRequest& request) const {
        return m_account_info->get_account_info<T>(request);
    }

    template<class T>
    const T TradeManagerModule::get_account_info(AccountInfoType type, int64_t timestamp) const {
        return m_account_info->get_account_info<T>(type, timestamp);
    }

    template<class T>
    const T TradeManagerModule::get_account_info(const std::string &symbol, int64_t timestamp) const {
        return m_account_info->get_account_info<T>(symbol, timestamp);
    }

    template<class T>
    const T TradeManagerModule::get_account_info(OptionType option, int64_t timestamp) const {
        return m_account_info->get_account_info<T>(option, timestamp);
    }

    template<class T>
    const T TradeManagerModule::get_account_info(OrderType order, int64_t timestamp) const {
        return m_account_info->get_account_info<T>(order, timestamp);
    }

    template<class T>
    const T TradeManagerModule::get_account_info(AccountType account, int64_t timestamp) const {
        return m_account_info->get_account_info<T>(account, timestamp);
    }

    template<class T>
    const T TradeManagerModule::get_account_info(CurrencyType currency, int64_t timestamp) const {
        return m_account_info->get_account_info<T>(currency, timestamp);
    }

    template<class T>
    const T TradeManagerModule::get_account_info(AccountInfoType info_type, const std::shared_ptr<TradeRequest>& trade_request, int64_t timestamp) const {
        return m_account_info->get_account_info<T>(info_type, trade_request, timestamp);
    }

} // namespace modules
} // namespace optionx
