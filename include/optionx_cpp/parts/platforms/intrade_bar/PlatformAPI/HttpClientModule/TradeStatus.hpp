namespace optionx {
namespace platforms {
namespace intrade_bar {

    void HttpClientModule::process_trade_status(
            double price,
            double profit,
            double balance,
            std::shared_ptr<TradeRequest> request,
            std::shared_ptr<TradeResult> result) {
        if (!request ||!result) {
            return;
        }
        auto account_info = get_account_info();
        if (balance <= 0.0) {
            result->balance = account_info->balance;
        } else {
            result->balance = balance;
        }
        result->close_price = price;
        if (std::abs(profit - result->amount) < 0.01) {
            result->trade_state = result->live_state = TradeState::STANDOFF;
            result->payout = account_info->get_account_info<int64_t>(AccountInfoType::PAYOUT, request, time_shield::ms_to_sec(result->open_date));
            result->profit = 0;
        } else
        if (profit <= 0.01) {
            result->trade_state = result->live_state = TradeState::LOSS;
            result->payout = account_info->get_account_info<int64_t>(AccountInfoType::PAYOUT, request, time_shield::ms_to_sec(result->open_date));
            result->profit = -result->amount;
        } else {
            result->trade_state = result->live_state = TradeState::WIN;
            result->profit = profit;
            result->payout = profit / result->amount;
        }
    }

} // namespace intrade_bar
} // namespace platforms
} // namespace optionx
