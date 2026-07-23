#pragma once
#ifndef OPTIONX_HEADER_BRIDGES_DETAIL_BRIDGE_TRADE_SIGNAL_VALIDATION_HPP_INCLUDED
#define OPTIONX_HEADER_BRIDGES_DETAIL_BRIDGE_TRADE_SIGNAL_VALIDATION_HPP_INCLUDED

/// \file BridgeTradeSignalValidation.hpp
/// \brief Defines shared validation helpers for executable bridge trade signals.

#include "data/trading.hpp"

#include <cmath>
#include <stdexcept>
#include <string>

namespace optionx::bridges::detail {

    /// \brief Validates a ratio-style trade signal field.
    /// \param value Field value.
    /// \param field_name Field name used in diagnostics.
    inline void validate_trade_signal_ratio(
            const double value,
            const char* field_name) {
        if (!std::isfinite(value)) {
            throw std::invalid_argument(std::string(field_name) + " must be finite.");
        }
        if (value < 0.0 || value > 1.0) {
            throw std::invalid_argument(std::string(field_name) + " must be in the 0..1 range.");
        }
    }

    /// \brief Validates a trade signal that a bridge is about to dispatch as executable intent.
    /// \param signal Signal to validate.
    /// \param context Diagnostic prefix, for example `BotBinary signal`.
    /// \throws std::invalid_argument when the signal is not executable.
    inline void validate_executable_trade_signal(
            const TradeSignal& signal,
            const char* context = "Bridge signal",
            const bool require_positive_amount = true) {
        const std::string prefix = context ? context : "Bridge signal";
        if (signal.symbol.empty()) {
            throw std::invalid_argument(prefix + " symbol is required.");
        }
        if (signal.order_type == OrderType::UNKNOWN) {
            throw std::invalid_argument(prefix + " order_type is required.");
        }
        if (signal.option_type == OptionType::UNKNOWN) {
            throw std::invalid_argument(prefix + " option_type is required.");
        }
        if (!std::isfinite(signal.amount)) {
            throw std::invalid_argument(prefix + " amount must be finite.");
        }
        if (require_positive_amount && signal.amount <= 0.0) {
            throw std::invalid_argument(prefix + " amount must be positive.");
        }
        validate_trade_signal_ratio(signal.refund, "refund");
        validate_trade_signal_ratio(signal.min_payout, "min_payout");
        if (signal.duration != 0 && signal.expiry_time > 0) {
            throw std::invalid_argument(prefix + " must not mix duration and expiry_time.");
        }
        if (signal.duration == 0 && signal.expiry_time <= 0) {
            throw std::invalid_argument(prefix + " expiry is required.");
        }
    }

} // namespace optionx::bridges::detail

#endif // OPTIONX_HEADER_BRIDGES_DETAIL_BRIDGE_TRADE_SIGNAL_VALIDATION_HPP_INCLUDED
