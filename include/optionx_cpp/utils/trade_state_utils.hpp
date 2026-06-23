#pragma once
#ifndef _OPTIONX_TRADE_STATE_UTILS_HPP_INCLUDED
#define _OPTIONX_TRADE_STATE_UTILS_HPP_INCLUDED

/// \file trade_state_utils.hpp
/// \brief Terminal-state and outcome predicates for TradeState.

#include "data/trading/enums.hpp"

namespace optionx {

    /// \brief Returns true if the state represents a winning trade.
    inline bool is_win(TradeState state) noexcept {
        return state == TradeState::WIN;
    }

    /// \brief Returns true if the state represents a losing trade.
    inline bool is_loss(TradeState state) noexcept {
        return state == TradeState::LOSS;
    }

    /// \brief Returns true if the state represents a standoff (no outcome).
    inline bool is_standoff(TradeState state) noexcept {
        return state == TradeState::STANDOFF;
    }

    /// \brief Returns true if the state represents a refunded trade.
    inline bool is_refund(TradeState state) noexcept {
        return state == TradeState::REFUND;
    }

    /// \brief Returns true if the state is terminal (no further lifecycle changes expected).
    inline bool is_terminal_trade_state(TradeState state) noexcept {
        return state == TradeState::WIN ||
               state == TradeState::LOSS ||
               state == TradeState::STANDOFF ||
               state == TradeState::REFUND ||
               state == TradeState::OPEN_ERROR ||
               state == TradeState::CHECK_ERROR ||
               state == TradeState::CANCELED_TRADE;
    }

    /// \brief Returns true if the state represents an error outcome.
    inline bool is_error_trade_state(TradeState state) noexcept {
        return state == TradeState::OPEN_ERROR ||
               state == TradeState::CHECK_ERROR ||
               state == TradeState::CANCELED_TRADE;
    }

    /// \brief Returns true if the state carries a monetary result (win/loss/standoff/refund).
    inline bool is_result_state(TradeState state) noexcept {
        return state == TradeState::WIN ||
               state == TradeState::LOSS ||
               state == TradeState::STANDOFF ||
               state == TradeState::REFUND;
    }

} // namespace optionx

#endif // _OPTIONX_TRADE_STATE_UTILS_HPP_INCLUDED
