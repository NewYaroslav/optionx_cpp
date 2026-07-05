#pragma once
#ifndef OPTIONX_HEADER_DATA_ACCOUNT_TRADING_CONDITION_UPDATE_HPP_INCLUDED
#define OPTIONX_HEADER_DATA_ACCOUNT_TRADING_CONDITION_UPDATE_HPP_INCLUDED

/// \file TradingConditionUpdate.hpp
/// \brief Defines broker trading-condition update payloads.

namespace optionx {

    /// \struct TradingConditionUpdate
    /// \brief Describes changed broker trading conditions for a symbol or account scope.
    ///
    /// Fields are optional because brokers often update payouts, market state,
    /// limits and sessions independently. Empty identity fields mean that the
    /// update applies to the provider/account scope rather than a concrete
    /// symbol or option type.
    struct TradingConditionUpdate {
        std::string symbol; ///< Broker/provider symbol, or empty for account-wide updates.
        PlatformType platform_type = PlatformType::UNKNOWN; ///< Source platform type.
        AccountType account_type = AccountType::UNKNOWN; ///< Account type, if relevant.
        CurrencyType currency = CurrencyType::UNKNOWN; ///< Account currency, if relevant.
        OptionType option_type = OptionType::UNKNOWN; ///< Option type, if relevant.
        std::int64_t timestamp = 0; ///< Update timestamp as Unix seconds; 0 means unknown.

        std::optional<bool> market_open; ///< Whether the symbol/session is open.
        std::optional<bool> tradable; ///< Whether new trades are currently accepted.
        std::optional<double> payout; ///< Current payout ratio, 0.0-1.0.
        std::optional<double> min_amount; ///< Minimum allowed trade amount.
        std::optional<double> max_amount; ///< Maximum allowed trade amount.
        std::optional<double> min_refund; ///< Minimum refund ratio, 0.0-1.0.
        std::optional<double> max_refund; ///< Maximum refund ratio, 0.0-1.0.
        std::optional<std::uint32_t> min_duration; ///< Minimum duration in seconds.
        std::optional<std::uint32_t> max_duration; ///< Maximum duration in seconds.
        std::optional<std::uint32_t> max_open_trades; ///< Maximum concurrent trades.
        std::optional<std::int64_t> session_start; ///< Session start, seconds since midnight.
        std::optional<std::int64_t> session_end; ///< Session end, seconds since midnight.
        std::string message; ///< Optional diagnostic or broker-provided note.

        /// \brief Returns true when no condition field is populated.
        /// \return True if the update contains only identity/context fields.
        bool empty() const noexcept {
            return !market_open &&
                   !tradable &&
                   !payout &&
                   !min_amount &&
                   !max_amount &&
                   !min_refund &&
                   !max_refund &&
                   !min_duration &&
                   !max_duration &&
                   !max_open_trades &&
                   !session_start &&
                   !session_end &&
                   message.empty();
        }
    };

    /// \brief Callback type for broker trading-condition updates.
    using trading_condition_callback_t =
        std::function<void(const TradingConditionUpdate&)>;

} // namespace optionx

#endif // OPTIONX_HEADER_DATA_ACCOUNT_TRADING_CONDITION_UPDATE_HPP_INCLUDED
