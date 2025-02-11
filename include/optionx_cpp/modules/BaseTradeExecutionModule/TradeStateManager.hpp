#pragma once
#ifndef _OPTIONX_TRADE_STATE_MANAGER_HPP_INCLUDED
#define _OPTIONX_TRADE_STATE_MANAGER_HPP_INCLUDED

/// \file TradeStateManager.hpp
/// \brief Manages trade state transitions and validations.

namespace optionx::modules {

    /// \class TradeStateManager
    /// \brief Handles validation, state transitions, and finalization of trades.
    ///
    /// This class is responsible for determining the validity of trade requests,
    /// computing trade outcomes, and managing the transitions between different
    /// trade states based on tick data and account information.
    class TradeStateManager {
    public:

        /// \brief Constructs a `TradeStateManager` with an account info provider.
        /// \param account_info Reference to `AccountInfoProvider` for retrieving trade-related data.
        explicit TradeStateManager(AccountInfoProvider& account_info)
            : m_account_info(account_info) {}

        /// \brief Validates a trade request based on account parameters and platform rules.
        /// \param request Shared pointer to the trade request.
        /// \return Corresponding `TradeErrorCode` (SUCCESS if valid).
        TradeErrorCode validate_request(
                const std::shared_ptr<TradeRequest>& request) const;

        /// \brief Determines the trade outcome based on the latest price update.
        /// \param result Shared pointer to the trade result.
        /// \param request Shared pointer to the trade request.
        /// \param tick The latest tick data.
        /// \return The final trade state (WIN, LOSS, STANDOFF).
        TradeState determine_trade_state(
                const std::shared_ptr<TradeResult>& result,
                const std::shared_ptr<TradeRequest>& request,
                const TickData& tick) const;

        /// \brief Checks if the given trade state allows closing.
        /// \param state The current trade state.
        /// \return True if the trade can be closed.
        bool is_closable_state(TradeState state) const;

        /// \brief Determines whether a trade should transition to `WAITING_CLOSE`.
        /// \param trade_state The current state of the trade.
        /// \return True if the trade should transition to `WAITING_CLOSE`.
        bool is_transition_to_waiting_close(TradeState trade_state) const;

        /// \brief Checks whether a trade has reached a terminal state.
        /// \param trade_state The current trade state.
        /// \return True if the trade is finalized and requires no further processing.
        bool is_terminal_state(TradeState trade_state) const;

        /// \brief Calculates the expected close time of a trade based on its type.
        /// \param result The trade result object.
        /// \param request The original trade request.
        /// \return The expected close timestamp in milliseconds.
        int64_t calculate_close_date(
                const std::shared_ptr<TradeResult>& result,
                const std::shared_ptr<TradeRequest>& request) const;

        /// \brief Finalizes a transaction by marking it as an error.
        /// \param transaction The trade transaction event.
        /// \param error_code The error that caused the finalization.
        /// \param state The final trade state.
        /// \param timestamp The timestamp at which the transaction was finalized.
        /// \param error_desc Optional error description (defaults to an empty string).
        void finalize_transaction_with_error(
                const std::shared_ptr<events::TradeTransactionEvent>& transaction,
                TradeErrorCode error_code,
                TradeState state,
                int64_t timestamp,
                const std::string& error_desc = std::string()) const;

    private:
        AccountInfoProvider& m_account_info; ///< Reference to the account info provider.
    };

    inline TradeErrorCode TradeStateManager::validate_request(const std::shared_ptr<TradeRequest>& request) const {
        if (request->symbol.empty()) return TradeErrorCode::INVALID_SYMBOL;
        const int64_t timestamp = time_shield::ms_to_sec(OPTIONX_TIMESTAMP_MS);

        if (!m_account_info.get_info<bool>(AccountInfoType::CONNECTION_STATUS)) return TradeErrorCode::NO_CONNECTION;
        if (!m_account_info.get_by_symbol<bool>(request->symbol, timestamp)) return TradeErrorCode::INVALID_SYMBOL;
        if (!m_account_info.get_by_option<bool>(request->option_type, timestamp)) return TradeErrorCode::INVALID_OPTION;
        if (!m_account_info.get_by_order<bool>(request->order_type, timestamp)) return TradeErrorCode::INVALID_ORDER;
        if (!m_account_info.get_by_account<bool>(request->account_type, timestamp)) return TradeErrorCode::INVALID_ACCOUNT;
        if (!m_account_info.get_by_currency<bool>(request->currency, timestamp)) return TradeErrorCode::INVALID_CURRENCY;
        if (!m_account_info.get_for_trade<bool>(AccountInfoType::TRADE_LIMIT_NOT_EXCEEDED, request, timestamp)) return TradeErrorCode::LIMIT_OPEN_TRADES;
        if (!m_account_info.get_for_trade<bool>(AccountInfoType::AMOUNT_BELOW_MAX, request, timestamp)) return TradeErrorCode::AMOUNT_TOO_HIGH;
        if (!m_account_info.get_for_trade<bool>(AccountInfoType::AMOUNT_ABOVE_MIN, request, timestamp)) return TradeErrorCode::AMOUNT_TOO_LOW;
        if (!m_account_info.get_for_trade<bool>(AccountInfoType::REFUND_BELOW_MAX, request, timestamp)) return TradeErrorCode::REFUND_TOO_HIGH;
        if (!m_account_info.get_for_trade<bool>(AccountInfoType::REFUND_ABOVE_MIN, request, timestamp)) return TradeErrorCode::REFUND_TOO_LOW;
        if (!m_account_info.get_for_trade<bool>(AccountInfoType::DURATION_AVAILABLE, request, timestamp)) return TradeErrorCode::INVALID_DURATION;
        if (!m_account_info.get_for_trade<bool>(AccountInfoType::EXPIRATION_DATE_AVAILABLE, request, timestamp)) return TradeErrorCode::INVALID_EXPIRY_TIME;
        if (!m_account_info.get_for_trade<bool>(AccountInfoType::PAYOUT_ABOVE_MIN, request, timestamp)) return TradeErrorCode::PAYOUT_TOO_LOW;
        if (!m_account_info.get_for_trade<bool>(AccountInfoType::AMOUNT_BELOW_BALANCE, request, timestamp)) return TradeErrorCode::INSUFFICIENT_BALANCE;

        return TradeErrorCode::SUCCESS;
    }

    TradeState TradeStateManager::determine_trade_state(
            const std::shared_ptr<TradeResult>& result,
            const std::shared_ptr<TradeRequest>& request,
            const TickData& tick) const {
        if (!result->open_price) {
            return TradeState::STANDOFF;
        }

        double mid_price = tick.mid_price();
        if (request->order_type == OrderType::BUY) {
            if (mid_price > result->open_price) return TradeState::WIN;
            if (mid_price < result->open_price) return TradeState::LOSS;
            return TradeState::STANDOFF;
        }

        if (request->order_type == OrderType::SELL) {
            if (mid_price < result->open_price) return TradeState::WIN;
            if (mid_price > result->open_price) return TradeState::LOSS;
            return TradeState::STANDOFF;
        }

        return TradeState::STANDOFF;
    }

    bool TradeStateManager::is_closable_state(TradeState state) const {
        return state == TradeState::WAITING_CLOSE ||
               state == TradeState::OPEN_SUCCESS ||
               state == TradeState::IN_PROGRESS;
    }

    bool TradeStateManager::is_transition_to_waiting_close(TradeState trade_state) const {
        return trade_state == TradeState::OPEN_SUCCESS ||
               trade_state == TradeState::IN_PROGRESS;
    }

    bool TradeStateManager::is_terminal_state(TradeState trade_state) const {
        return trade_state == TradeState::OPEN_ERROR ||
               trade_state == TradeState::CHECK_ERROR ||
               trade_state == TradeState::WIN ||
               trade_state == TradeState::LOSS ||
               trade_state == TradeState::STANDOFF ||
               trade_state == TradeState::REFUND;
    }

    int64_t TradeStateManager::calculate_close_date(
            const std::shared_ptr<TradeResult>& result,
            const std::shared_ptr<TradeRequest>& request) const {
        if (result->close_date > 0) return result->close_date;
        if (request->option_type == OptionType::SPRINT) {
            return result->open_date > 0
                ? result->open_date + time_shield::sec_to_ms(request->duration)
                : result->place_date + time_shield::sec_to_ms(request->duration);
        }
        if (request->option_type == OptionType::CLASSIC) {
            return time_shield::sec_to_ms(request->expiry_time);
        }
        return 0;
    }

    void TradeStateManager::finalize_transaction_with_error(
            const std::shared_ptr<events::TradeTransactionEvent>& transaction,
            TradeErrorCode error_code,
            TradeState state,
            int64_t timestamp,
            const std::string& error_desc) const {
        auto& request = transaction->request;
        auto& result = transaction->result;

        result->error_code = error_code;
        result->error_desc = error_desc.empty() ? to_str(result->error_code) : error_desc;
        result->send_date = timestamp;
        result->open_date = timestamp;
        result->close_date = timestamp;
        result->balance = m_account_info.get_for_trade<double>(AccountInfoType::BALANCE, request, timestamp);
        result->payout = m_account_info.get_for_trade<double>(AccountInfoType::PAYOUT, request, timestamp);
        result->trade_state = result->live_state = state;
    }

} // namespace optionx::modules

#endif // _OPTIONX_TRADE_STATE_MANAGER_HPP_INCLUDED
