namespace optionx {
namespace modules {

    TradeErrorCode TradeManagerModule::check_request(const std::shared_ptr<TradeRequest>& request) {
        if (request->symbol.empty()) return TradeErrorCode::INVALID_SYMBOL;
        const int64_t timestamp = time_shield::ms_to_sec(OPTIONX_TIMESTAMP_MS);

        if (!m_account_info) return TradeErrorCode::NO_CONNECTION;
        if (!get_account_info<bool>(AccountInfoType::CONNECTION_STATUS)) return TradeErrorCode::NO_CONNECTION;
        if (!get_account_info<bool>(request->symbol, timestamp)) return TradeErrorCode::INVALID_SYMBOL;
        if (!get_account_info<bool>(request->option_type, timestamp)) return TradeErrorCode::INVALID_OPTION;
        if (!get_account_info<bool>(request->order_type, timestamp)) return TradeErrorCode::INVALID_ORDER;
        if (!get_account_info<bool>(request->account_type, timestamp)) return TradeErrorCode::INVALID_ACCOUNT;
        if (!get_account_info<bool>(request->currency, timestamp)) return TradeErrorCode::INVALID_CURRENCY;
        if (!get_account_info<bool>(AccountInfoType::TRADE_LIMIT_NOT_EXCEEDED, request, timestamp)) return TradeErrorCode::LIMIT_OPEN_TRADES;
        if (!get_account_info<bool>(AccountInfoType::AMOUNT_BELOW_MAX, request, timestamp)) return TradeErrorCode::AMOUNT_TOO_HIGH;
        if (!get_account_info<bool>(AccountInfoType::AMOUNT_ABOVE_MIN, request, timestamp)) return TradeErrorCode::AMOUNT_TOO_LOW;
        if (!get_account_info<bool>(AccountInfoType::REFUND_BELOW_MAX, request, timestamp)) return TradeErrorCode::REFUND_TOO_HIGH;
        if (!get_account_info<bool>(AccountInfoType::REFUND_ABOVE_MIN, request, timestamp)) return TradeErrorCode::REFUND_TOO_LOW;
        if (!get_account_info<bool>(AccountInfoType::DURATION_AVAILABLE, request, timestamp)) return TradeErrorCode::INVALID_DURATION;
        if (!get_account_info<bool>(AccountInfoType::EXPIRATION_DATE_AVAILABLE, request, timestamp)) return TradeErrorCode::INVALID_EXPIRY_TIME;
        if (!get_account_info<bool>(AccountInfoType::PAYOUT_ABOVE_MIN, request, timestamp)) return TradeErrorCode::PAYOUT_TOO_LOW;
        if (!get_account_info<bool>(AccountInfoType::AMOUNT_BELOW_BALANCE, request, timestamp)) return TradeErrorCode::INSUFFICIENT_BALANCE;

        return TradeErrorCode::SUCCESS;
    }

} // namespace modules
} // namespace optionx
