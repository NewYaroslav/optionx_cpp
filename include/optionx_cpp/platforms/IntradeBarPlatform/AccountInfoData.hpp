#pragma once
#ifndef _OPTIONX_PLATFORMS_INTRADERBAR_ACCOUNT_INFO_DATA_HPP_INCLUDED
#define _OPTIONX_PLATFORMS_INTRADERBAR_ACCOUNT_INFO_DATA_HPP_INCLUDED

/// \file AccountInfoData.hpp
/// \brief Contains the AccountInfoData class for Intrade Bar platform account information.

namespace optionx::platforms::intrade_bar {

    /// \class AccountInfoData
    /// \brief Account information data for Intrade Bar platform.
    class AccountInfoData : public BaseAccountInfoData {
    public:
        // Public member variables
        int64_t         user_id         = 0;                        ///< User ID
        double          balance         = 0.0;                      ///< Account balance
        CurrencyType    currency        = CurrencyType::UNKNOWN;    ///< Account currency
        AccountType     account_type    = AccountType::UNKNOWN;     ///< Account type (DEMO or REAL)
        bool            connect         = false;                    ///< Connection status
        int64_t         open_trades     = 0;                        ///< Number of open trades for the account

        // --- Limits ----------------------------------------------------------

        double min_usd_amount           = 1.0;                      ///< Minimum trade amount for USD accounts
        double max_usd_amount           = 700.0;                    ///< Maximum trade amount for USD accounts
        double max_usd_limit_amount     = 50.0;                     ///< Maximum trade amount during restricted periods for USD
        double high_payout_usd_amount   = 80.0;                     ///< Trade amount threshold for increased payout percentage in USD
        double min_rub_amount           = 100.0;                    ///< Minimum trade amount for RUB accounts
        double max_rub_amount           = 50000.0;                  ///< Maximum trade amount for RUB accounts
        double max_rub_limit_amount     = 3500.0;                   ///< Maximum trade amount during restricted periods for RUB
        double high_payout_rub_amount   = 5000.0;                   ///< Trade amount threshold for increased payout percentage in RUB

        int64_t min_duration            = 60;                       ///< Minimum binary option duration (in seconds)
        int64_t min_btc_duration        = 300;                      ///< Minimum duration for BTCUSDT (in seconds)
        int64_t max_duration            = 30000;                    ///< Maximum duration (in seconds)
        int64_t max_trades              = 5;                        ///< Maximum number of trades
        int64_t max_limit_trades        = 2;                        ///< Maximum number of trades during restricted periods
        int64_t order_queue_timeout     = 10;                       ///< Timeout for pending orders in the queue
        int64_t responce_timeout        = 10;                       ///< Timeout for server response related to opening or closing a trade
        int64_t order_interval_ms       = 1000;                     ///< Minimum time interval required between consecutive orders

        // Trading time for BTCUSDT
        int64_t start_btc_time          = 0;                        ///< Start of trading time for BTCUSDT
        int64_t end_btc_time            = 86400;                    ///< End of trading time for BTCUSDT
        // Trading time for standard pairs
        int64_t start_time              = 3600;                     ///< Start of trading time for standard pairs
        int64_t end_time                = 75600;                    ///< End of trading time for standard pairs

        /// \brief Retrieves the API type associated with this account data.
        /// \return The type of API used.
        const PlatformType platform_type() const override final {
            return PlatformType::INTRADE_BAR;
        }

        /// \brief Creates a unique pointer to a clone of this account info data instance.
        /// \return Unique pointer to a cloned `BaseAccountInfoData` instance.
        std::unique_ptr<BaseAccountInfoData> clone_unique() const override final {
            return std::make_unique<AccountInfoData>(*this);
        }

        /// \brief Creates a shared pointer to a clone of this account info data instance.
        /// \return Shared pointer to a cloned `BaseAccountInfoData` instance.
        std::shared_ptr<BaseAccountInfoData> clone_shared() const override final {
            return std::make_shared<AccountInfoData>(*this);
        }

    protected:

        /// \brief Retrieves a boolean account information based on the request type.
        /// \param request Specifies the type of account information requested.
        /// \return Boolean account information.
        bool get_info_bool(const AccountInfoRequest& request) const override final {
            switch (request.type) {
            case AccountInfoType::CONNECTION_STATUS:
                return connect;
            case AccountInfoType::SYMBOL_AVAILABILITY: {
                static const std::set<std::string> symbols = {
                    "AUDCAD","AUDCHF","AUDJPY",
                    "AUDNZD","AUDUSD","CADJPY",
                    "EURAUD","EURCAD","EURCHF",
                    "EURGBP","EURJPY","EURUSD",
                    "GBPAUD","GBPCHF","GBPJPY",
                    "GBPNZD","NZDJPY","NZDUSD",
                    "USDCAD","USDCHF","USDJPY",
                    "BTCUSDT"
                };
                return (symbols.find(request.symbol) != symbols.end());
            }
            case AccountInfoType::OPTION_TYPE_AVAILABILITY:
                if (request.option_type == OptionType::CLASSIC &&
                    (request.symbol == "BTCUSDT" || request.symbol == "BTCUSD")) return false;
                return (request.option_type == OptionType::CLASSIC || request.option_type == OptionType::SPRINT);
            case AccountInfoType::ORDER_TYPE_AVAILABILITY:
                return (request.order_type == OrderType::BUY || request.order_type == OrderType::SELL);
            case AccountInfoType::ACCOUNT_TYPE_AVAILABILITY:
                return (account_type != AccountType::UNKNOWN && request.account_type == account_type);
            case AccountInfoType::CURRENCY_AVAILABILITY:
                return (currency != CurrencyType::UNKNOWN && request.currency == currency);
            case AccountInfoType::TRADE_LIMIT_NOT_EXCEEDED:
                return (open_trades < max_trades);
            case AccountInfoType::AMOUNT_BELOW_MAX:
                return request.amount <= get_max_amount(request);
            case AccountInfoType::AMOUNT_ABOVE_MIN:
                return request.amount >= get_min_amount(request);
            case AccountInfoType::REFUND_BELOW_MAX:
                return true;
            case AccountInfoType::REFUND_ABOVE_MIN:
                return true;
            case AccountInfoType::DURATION_AVAILABLE: {
                if (request.option_type == OptionType::CLASSIC) return true;
                const int64_t req_min_duration = (request.symbol == "BTCUSD" || request.symbol == "BTCUSDT") ?
                    min_btc_duration : min_duration;
                const int64_t req_max_duration = (request.symbol == "BTCUSD" || request.symbol == "BTCUSDT") ?
                    max_duration : std::min(time_shield::start_of_min(end_time - time_shield::sec_of_day(request.timestamp)), max_duration);
                return (request.duration >= req_min_duration && request.duration <= req_max_duration);
            }
            case AccountInfoType::EXPIRATION_DATE_AVAILABLE: {
                if (request.option_type == OptionType::SPRINT) return true;
                const int64_t sec_of_day = time_shield::sec_of_day(request.expiry_time);
                if (sec_of_day < start_time || sec_of_day > end_time) return false;
                if (sec_of_day % (5 * time_shield::SEC_PER_MIN) != 0) return false;
                const int64_t sec_close = request.expiry_time - request.timestamp;
                const int64_t min_sec = 300;
                return (sec_close > min_sec);
            }
            case AccountInfoType::PAYOUT_ABOVE_MIN:
                if (request.min_payout == 0.0) return true;
                return (request.min_payout >= get_payout(request));
            case AccountInfoType::AMOUNT_BELOW_BALANCE:
                return (request.amount <= balance);
            default:
                break;
            }
            return false;
        }

        /// \brief Retrieves integer account information based on the request type.
        /// \param request Specifies the type of account information requested.
        /// \return Integer account information.
        int64_t get_info_int64(const AccountInfoRequest& request) const override final {
            switch (request.type) {
                case AccountInfoType::USER_ID: return user_id;
                case AccountInfoType::CONNECTION_STATUS: return static_cast<int64_t>(connect);
                case AccountInfoType::BALANCE: return static_cast<int64_t>(balance);
                case AccountInfoType::PLATFORM_TYPE: return static_cast<int64_t>(platform_type());
                case AccountInfoType::ACCOUNT_TYPE: return static_cast<int64_t>(account_type);
                case AccountInfoType::CURRENCY: return static_cast<int64_t>(currency);
                case AccountInfoType::OPEN_TRADES: return open_trades;
                case AccountInfoType::MAX_TRADES:
                    return check_amount_limits(time_shield::sec_of_day(request.timestamp)) ? max_limit_trades : max_trades;
                case AccountInfoType::PAYOUT:
                    return static_cast<int64_t>(get_payout(request) * 100.0);
                case AccountInfoType::MIN_AMOUNT: return static_cast<int64_t>(get_min_amount(request));
                case AccountInfoType::MAX_AMOUNT: return static_cast<int64_t>(get_max_amount(request));
                case AccountInfoType::MIN_DURATION:
                    return (request.symbol == "BTCUSD" || request.symbol == "BTCUSDT") ? min_btc_duration : min_duration;
                case AccountInfoType::MAX_DURATION:
                    return std::min(time_shield::start_of_min(end_time - time_shield::sec_of_day(request.timestamp)), max_duration);
                case AccountInfoType::START_TIME: return time_shield::start_of_day(request.timestamp) + start_time;
                case AccountInfoType::END_TIME: return time_shield::start_of_day(request.timestamp) + end_time;
                case AccountInfoType::ORDER_QUEUE_TIMEOUT: return order_queue_timeout;
                case AccountInfoType::RESPONSE_TIMEOUT: return responce_timeout;
                case AccountInfoType::ORDER_INTERVAL_MS: return order_interval_ms;
                default: break;
            }
            return 0;
        }

        /// \brief Retrieves floating-point account information based on the request type.
        /// \param request Specifies the type of account information requested.
        /// \return Floating-point account information.
        double get_info_f64(const AccountInfoRequest& request) const override final {
            switch (request.type) {
                case AccountInfoType::BALANCE: return balance;
                case AccountInfoType::PAYOUT: return get_payout(request);
                case AccountInfoType::MIN_AMOUNT: return get_min_amount(request);
                case AccountInfoType::MAX_AMOUNT: return get_max_amount(request);
                default: break;
            }
            return 0;
        }

        /// \brief Retrieves string account information based on the request type.
        /// \param request Specifies the type of account information requested.
        /// \return String account information.
        std::string get_info_str(const AccountInfoRequest& request) const override final {
            switch (request.type) {
                case AccountInfoType::USER_ID: return std::to_string(user_id);
                case AccountInfoType::BALANCE: return utils::format("%.2f", balance);
                case AccountInfoType::PLATFORM_TYPE: return to_str(platform_type());
                case AccountInfoType::ACCOUNT_TYPE: return to_str(account_type);
                case AccountInfoType::CURRENCY: return to_str(currency);
                default: break;
            }
            return std::string();
        }

        /// \brief Retrieves the account type.
        /// \param request The account information request.
        /// \return The account's type (DEMO or REAL).
        AccountType get_info_account_type(const AccountInfoRequest& request) const override final {
            return account_type;
        }

        /// \brief Retrieves the account currency.
        /// \param request The account information request.
        /// \return The account's currency type.
        CurrencyType get_info_currency(const AccountInfoRequest& request) const override final {
            return currency;
        }

        /// \brief Gets the minimum trade amount based on the currency type.
        /// \param request The account information request.
        /// \return The minimum trade amount.
        double get_min_amount(const AccountInfoRequest& request) const {
            return request.currency == CurrencyType::USD ?
                min_usd_amount : (request.currency == CurrencyType::RUB ?
                min_rub_amount : (currency == CurrencyType::USD ?
                min_usd_amount : (currency == CurrencyType::RUB ? min_rub_amount : 0.0)));
        }

        /// \brief Gets the maximum trade amount based on the currency type.
        /// \param request The account information request.
        /// \return The maximum trade amount.
        double get_max_amount(const AccountInfoRequest& request) const {
            if (check_amount_limits(time_shield::sec_of_day(request.timestamp))) {
                return request.currency == CurrencyType::USD ?
                    max_usd_limit_amount : (request.currency == CurrencyType::RUB ?
                    max_rub_limit_amount : (currency == CurrencyType::USD ?
                    max_usd_limit_amount : (currency == CurrencyType::RUB ? max_rub_limit_amount : 0.0)));
            }
            return request.currency == CurrencyType::USD ?
                    max_usd_amount : (request.currency == CurrencyType::RUB ?
                    max_rub_amount : (currency == CurrencyType::USD ?
                    max_usd_amount : (currency == CurrencyType::RUB ? max_rub_amount : 0.0)));
        }

        /// \brief Checks if the amount limits apply based on the time of day.
        /// \param second_day The time in seconds since the start of the day.
        /// \return True if amount limits are in effect, false otherwise.
        bool check_amount_limits(const int second_day) const {
            // Check if the given time of day falls within specified high-risk periods
            return ((second_day >= 50100 && second_day < 50700) || // 17:00
                    (second_day >= 53700 && second_day < 54300) || // 18:00
                    (second_day >= 57300 && second_day < 57900) || // 19:00
                    (second_day >= 60900 && second_day < 61500) || // 20:00
                    (second_day >= 64500 && second_day < 65100) || // 21:00
                    (second_day >= 68100 && second_day < 68700) || // 22:00
                    (second_day >= 71700 && second_day < 72300) || // 23:00
                    (second_day >= 73500 && second_day < 74100) || // 23:30
                    (second_day >= 75300));
        }

        /// \brief Checks if payout limits apply based on the time of day.
        /// \param second_day The time in seconds since the start of the day.
        /// \return True if payout limits are in effect, false otherwise.
        bool check_payout_limits(const int second_day) const {
            // Check if the time of day falls within higher payout periods
            return ((second_day < 3780) ||                          // 4:00
                    (second_day >= 7020 && second_day < 7380) ||    // 5:00
                    (second_day >= 10620 && second_day < 10980) ||  // 6:00
                    (second_day >= 14220 && second_day < 14580) ||  // 7:00
                    (second_day >= 17820 && second_day < 18180) ||  // 8:00
                    (second_day >= 21420 && second_day < 21780) ||  // 9:00
                    (second_day >= 50100 && second_day < 50700) || // 17:00
                    (second_day >= 53700 && second_day < 54300) || // 18:00
                    (second_day >= 57300 && second_day < 57900) || // 19:00
                    (second_day >= 60900 && second_day < 61500) || // 20:00
                    (second_day >= 64500 && second_day < 65100) || // 21:00
                    (second_day >= 68100 && second_day < 68700) || // 22:00
                    (second_day >= 71700 && second_day < 72300) || // 23:00
                    (second_day >= 75300));
        }

        /// \brief Gets the payout percentage based on the trade parameters.
        /// \param request The account information request.
        /// \return The payout percentage (as a decimal value).
        double get_payout(const AccountInfoRequest& request) const {

            if ((request.currency == CurrencyType::USD && request.amount < min_usd_amount) ||
                (request.currency == CurrencyType::RUB && request.amount < min_rub_amount)) {
                return 0.0;
            }
            if ((currency == CurrencyType::USD && request.amount < min_usd_amount)||
                (currency == CurrencyType::RUB && request.amount < min_rub_amount)) {
                return 0.0;
            }

            const int64_t sec_of_day = time_shield::sec_of_day(request.timestamp);
            if (request.symbol == "BTCUSDT" || request.symbol == "BTCUSD") {
                if (request.option_type == OptionType::CLASSIC ||
                    request.duration < min_btc_duration ||
                    request.duration > max_duration) {
                    return 0.0;
                }
                if (!check_payout_limits(sec_of_day)) {
                    if ((request.currency == CurrencyType::USD && request.amount >= high_payout_usd_amount)||
                        (request.currency == CurrencyType::RUB && request.amount >= high_payout_rub_amount)) {
                        return 0.85;
                    }
                    if ((currency == CurrencyType::USD && request.amount >= high_payout_usd_amount)||
                        (currency == CurrencyType::RUB && request.amount >= high_payout_rub_amount)) {
                        return 0.85;
                    }
                    return 0.79;
                }
                return 0.6;
            }


            if (time_shield::is_day_off(request.timestamp) ||
                sec_of_day < start_time ||
                sec_of_day >= end_time) {
                return 0.0;
            }

            if (request.option_type == OptionType::SPRINT) {
                if (request.duration < time_shield::SEC_PER_MIN ||
                    request.duration == (2 * time_shield::SEC_PER_MIN) ||
                    (request.duration % time_shield::SEC_PER_MIN) != 0 ||
                    request.duration > std::min(time_shield::start_of_min(end_time - time_shield::sec_of_day(request.timestamp)), max_duration)) {
                    return 0.0;
                }
                if (!check_payout_limits(sec_of_day)) {
                    if ((request.currency == CurrencyType::USD && request.amount >= high_payout_usd_amount)||
                        (request.currency == CurrencyType::RUB && request.amount >= high_payout_rub_amount)) {
                        return 0.85;
                    }
                    if ((currency == CurrencyType::USD && request.amount >= high_payout_usd_amount)||
                        (currency == CurrencyType::RUB && request.amount >= high_payout_rub_amount)) {
                        return 0.85;
                    }
                    if (request.duration == 180) return 0.82;
                    if (request.duration == 60) return 0.82;
                    return 0.82;
                }
                return 0.6;
            } else
            if (request.option_type == OptionType::CLASSIC) {
                if (request.duration > time_shield::SEC_PER_YEAR) {
                    const int64_t expiration = get_classic_bo_expiration(request.timestamp, request.duration);
                    if (expiration == 0) return 0.0;
                } else {
                    if ((request.duration % (5 * time_shield::SEC_PER_MIN)) != 0) return 0.0;
                    const int64_t timestamp = get_classic_bo_closing_timestamp(request.timestamp, request.duration / time_shield::SEC_PER_MIN);
                    if (timestamp == 0) return 0.0;
                    if (timestamp > (time_shield::start_of_day(timestamp) + end_time)) return 0.0;
                }
                if (sec_of_day < start_time || sec_of_day >= end_time) return 0.0;
                if (!check_payout_limits(sec_of_day)) {
                    if ((request.currency == CurrencyType::USD && request.amount >= high_payout_usd_amount)||
                        (request.currency == CurrencyType::RUB && request.amount >= high_payout_rub_amount)) {
                        return 0.85;
                    }
                    if ((currency == CurrencyType::USD && request.amount >= high_payout_usd_amount)||
                        (currency == CurrencyType::RUB && request.amount >= high_payout_rub_amount)) {
                        return 0.85;
                    }
                    return 0.79;
                }
            }
            return 0.0;
        }

        /// \brief Gets the expiration time for a classic binary option.
        /// \param timestamp The current timestamp in seconds.
        /// \param closing_timestamp The intended closing timestamp.
        /// \return Expiration time in seconds, or 0 if invalid.
        int64_t get_classic_bo_expiration(int64_t timestamp, int64_t closing_timestamp) const {
            const int64_t min_exp = 5 * time_shield::SEC_PER_MIN;
            if ((closing_timestamp % min_exp) != 0) return 0;
            const int64_t diff = closing_timestamp - timestamp;
            const int64_t min_diff = 3 * time_shield::SEC_PER_MIN;
            if (diff <= min_diff) return 0;
            return ((((diff - 1) / time_shield::SEC_PER_MIN - 3) / 5) * 5 + 5) * time_shield::SEC_PER_MIN;
        }

        /// \brief Gets the closing timestamp for a classic binary option.
        /// \param timestamp The initial timestamp in seconds.
        /// \param expiration Expiration time in minutes.
        /// \return Closing timestamp, or 0 if invalid.
        int64_t get_classic_bo_closing_timestamp(int64_t timestamp, int64_t expiration) const {
            if ((expiration % 5) != 0 || expiration < 5) return 0;
            const int64_t timestamp_future = timestamp + (expiration + 3) * time_shield::SEC_PER_MIN;
            return (timestamp_future - timestamp_future % (5 * time_shield::SEC_PER_MIN));
        }
    }; // AccountInfoData

} // namespace optionx::platforms::intrade_bar

#endif // _OPTIONX_PLATFORMS_INTRADERBAR_ACCOUNT_INFO_DATA_HPP_INCLUDED
