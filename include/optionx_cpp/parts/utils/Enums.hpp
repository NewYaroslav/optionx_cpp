#pragma once
#ifndef _OPTIONX_ENUMS_HPP_INCLUDED
#define _OPTIONX_ENUMS_HPP_INCLUDED

/// \file Enums.hpp
/// \brief

#include <kurlyk.hpp>
#include "StringUtils.hpp"
#include <stdexcept>

namespace optionx {

    enum class ApiType {
        UNKNOWN = 0,
        SIMULATOR,
        CLICKER,
        INTRADE_BAR,
        REST_API,
    };

    enum class OptionType {
        UNKNOWN = 0,
        SPRINT  = 1,
        CLASSIC = 2,
    };

    /** \brief Тип ордера
     */
    enum class OrderType {
        UNKNOWN = 0,
        BUY     = 1,
        SELL    = 2,
    };

    /** \brief Тип счета
     */
    enum class AccountType {
        UNKNOWN = 0,
        DEMO    = 1,
        REAL    = 2,
    };

    /** \brief Валюта
     */
    enum class CurrencyType {
        UNKNOWN = 0,
        USD,
        EUR,
        GBP,
        BTC,
        ETH,
        USDT,
        USDC,
        RUB,
        UAH,
        KZT
    };

    /** \brief Тип результата авторизации
     */
    enum class AuthResultType {
        Account,
        QuotesStream
    };

    enum class AuthSetupState {
        UNKNOWN = 0,
        CONFIGURED,
        CONNECTING,
        CONNECTED,
        CONNECTION_FAILED,
        DISCONNECTED,
        CONFIG_ERROR,
        API_INITIALIZED,        ///< Завершение инициализации API
        API_INIT_ERROR          ///< Ошибка инициализации API
    };

    /** \brief Статус ордера
     */

    /// \enum OrderState
    /// \brief Represents the state of a trade order during its lifecycle.
    enum class OrderState {
        UNKNOWN = 0,    ///< The state of the trade order is unknown.
        CANCELED_TRADE, ///< The trade was canceled.
        WAITING_OPEN,   ///< The trade is waiting to be opened.
        OPEN_SUCCESS,   ///< The trade was successfully opened.
        OPEN_ERROR,     ///< An error occurred while opening the trade.
        WAITING_CLOSE,  ///< The trade is waiting to be closed.
        CHECK_ERROR,    ///< An error occurred while checking the trade result.
        WIN,            ///< The trade ended with a win.
        LOSS,           ///< The trade ended with a loss.
        STANDOFF,       ///< The trade ended in a standoff (draw).
        REFUND          ///< The trade was refunded (partial or full).
    };

    /** \brief Тип информации об аккаунте
     */

    /// \enum AccountInfoType
    /// \brief Defines the types of account information that can be requested.
    enum class AccountInfoType {
        UNKNOWN = 0,              ///< Unknown type
        PARTNER_ID,               ///< Partner ID (string)
        USER_ID,                  ///< User ID (string)
        BALANCE,                  ///< Account balance
        BONUS,                    ///< Account bonus amount
        CONNECTION_STATUS,        ///< Connection status (connected or disconnected)
        VIP_STATUS,               ///< VIP status of the account
        API_TYPE,                 ///< Broker type
        ACCOUNT_TYPE,             ///< Account type (e.g., DEMO, REAL)
        CURRENCY,                 ///< Account currency (e.g., USD, EUR)
        OPEN_TRADES,              ///< Number of currently open trades
        MAX_TRADES,               ///< Maximum allowable open trades
        PAYOUT,                   ///< Payout percentage
        MIN_AMOUNT,               ///< Minimum trade amount
        MAX_AMOUNT,               ///< Maximum trade amount
        MAX_REFUND,               ///< Maximum refund percentage
        MIN_DURATION,             ///< Minimum option duration
        MAX_DURATION,             ///< Maximum option duration
        START_TIME,               ///< Start time of the trading day (in seconds from midnight)
        END_TIME,                 ///< End time of the trading day (in seconds from midnight)
        ORDER_QUEUE_TIMEOUT,      ///< Timeout for pending orders in the queue
        RESPONSE_TIMEOUT,         ///< Timeout for server response related to opening or closing a trade
        ORDER_INTERVAL_MS,        ///< Minimum time interval between consecutive orders, in milliseconds
        SYMBOL_AVAILABILITY,      ///< Availability of a symbol for trading
        OPTION_TYPE_AVAILABILITY, ///< Availability of an OptionType for trading
        ORDER_TYPE_AVAILABILITY,  ///< Availability of an OrderType for trading
        ACCOUNT_TYPE_AVAILABILITY,///< Availability of an AccountType
        CURRENCY_AVAILABILITY,    ///< Availability of a CurrencyType for the account
        TRADE_LIMIT_NOT_EXCEEDED, ///< Check if the number of open trades is below the maximum limit
        AMOUNT_BELOW_MAX,         ///< Check if the trade amount does not exceed the maximum allowed amount
        AMOUNT_ABOVE_MIN,         ///< Check if the trade amount meets the minimum required amount
        REFUND_BELOW_MAX,         ///< Check if the refund percentage is within the maximum limit
        REFUND_ABOVE_MIN,         ///< Check if the refund percentage meets the minimum required
        DURATION_AVAILABLE,       ///< Check if the expiration duration is within available limits
        EXPIRATION_DATE_AVAILABLE,///< Check if the expiration date is within allowable range
        PAYOUT_ABOVE_MIN,         ///< Check if the payout percentage meets the minimum threshold required
        AMOUNT_BELOW_BALANCE      ///< Check if the trade amount is below the account balance
    };

    /// \enum OrderErrorCode
    /// \brief Represents error codes for order validation and processing.
    enum class OrderErrorCode {
        SUCCESS = 0,                  ///< No error, operation successful.
        INVALID_SYMBOL,               ///< Invalid symbol for trading.
        INVALID_OPTION,               ///< Invalid option type.
        INVALID_ORDER,                ///< Invalid order type.
        INVALID_ACCOUNT,              ///< Invalid account type.
        INVALID_CURRENCY,             ///< Invalid currency type.
        AMOUNT_TOO_LOW,               ///< Trade amount is below the minimum allowed.
        AMOUNT_TOO_HIGH,              ///< Trade amount exceeds the maximum allowed.
        REFUND_TOO_LOW,               ///< Refund percentage is below the minimum required.
        REFUND_TOO_HIGH,              ///< Refund percentage exceeds the maximum allowed.
        PAYOUT_TOO_LOW,               ///< Payout percentage is below the minimum threshold.
        INVALID_DURATION,             ///< Duration is invalid or not supported.
        INVALID_EXPIRY_TIME,          ///< Expiry time is invalid or out of range.
        LIMIT_OPEN_TRADES,            ///< Open trades limit has been reached.
        INVALID_REQUEST,              ///< General invalid request.
        LONG_QUEUE_WAIT,              ///< Order waited too long in the queue.
        LONG_RESPONSE_WAIT,           ///< Long wait for server response.
        NO_CONNECTION,                ///< No network connection.
        CLIENT_FORCED_CLOSE,          ///< Client closed forcibly.
        PARSING_ERROR,                ///< Parsing error occurred.
        CANCELED_TRADE,               ///< Trade was canceled by user or system.
        INSUFFICIENT_BALANCE          ///< Trade amount exceeds available account balance.
    };

    /** \brief Тип системы мани-менеджмента
     */
    enum class MmSystemType {
        NONE,
        FIXED,
        PERCENT,
        KELLY_CRITERION,
        MARTINGALE_SIGNAL,
        MARTINGALE_SYMBOL,
        MARTINGALE_BAR,
        ANTI_MARTINGALE_SIGNAL,
        ANTI_MARTINGALE_SYMBOL,
        ANTI_MARTINGALE_BAR,
        LABOUCHERE_SIGNAL,
        LABOUCHERE_SYMBOL,
        LABOUCHERE_BAR,
        SKU_SIGNAL,
        SKU_SYMBOL,
        SKU_BAR
    };

    inline const std::string &to_str(const ApiType value, const int mode = 0) noexcept {
        static const std::vector<std::string> data_mode_0 = {
            "UNKNOWN",
            "Simulator",
            "Clicker",
            "Intrade Bar",
            "REST API"
        };
        static const std::vector<std::string> data_mode_1 = {
            "UNKNOWN",
            "Simulator",
            "Clicker",
            "intrade.bar",
            "Rest API"
        };
        switch (mode) {
        case 0:
            return data_mode_0[static_cast<size_t>(value)];
        case 1:
            return data_mode_1[static_cast<size_t>(value)];
        };
        return data_mode_0[static_cast<size_t>(value)];
    };

    inline const std::string &to_str(const OptionType value, const int mode = 0) noexcept {
        static const std::vector<std::string> data_mode_0 = {
            "UNKNOWN",
            "SPRINT",
            "CLASSIC",
        };
        static const std::vector<std::string> data_mode_1 = {
            "UNKNOWN",
            "Sprint",
            "Classic",
        };
        switch (mode) {
        case 0:
            return data_mode_0[static_cast<size_t>(value)];
        case 1:
            return data_mode_1[static_cast<size_t>(value)];
        };
        return data_mode_0[static_cast<size_t>(value)];
    };

    inline const std::string &to_str(const OrderType value, const int mode = 0) noexcept {
        static const std::vector<std::string> data_mode_0 = {
            "UNKNOWN",
            "BUY",
            "SELL",
        };
        static const std::vector<std::string> data_mode_1 = {
            "UNKNOWN",
            "Buy",
            "Sell",
        };
        static const std::vector<std::string> data_mode_2 = {
            "UNKNOWN",
            "PUT",
            "CALL",
        };
        static const std::vector<std::string> data_mode_3 = {
            "UNKNOWN",
            "Put",
            "Call",
        };
        switch (mode) {
        case 0:
            return data_mode_0[static_cast<size_t>(value)];
        case 1:
            return data_mode_1[static_cast<size_t>(value)];
        case 2:
            return data_mode_2[static_cast<size_t>(value)];
        case 3:
            return data_mode_3[static_cast<size_t>(value)];
        };
        return data_mode_0[static_cast<size_t>(value)];
    };

    inline const std::string &to_str(const AccountType value, const int mode = 0) noexcept {
        static const std::vector<std::string> data_mode_0 = {
            "UNKNOWN",
            "DEMO",
            "REAL",
        };
        static const std::vector<std::string> data_mode_1 = {
            "UNKNOWN",
            "Demo",
            "Real",
        };
        switch (mode) {
        case 0:
            return data_mode_0[static_cast<size_t>(value)];
        case 1:
            return data_mode_1[static_cast<size_t>(value)];
        };
        return data_mode_0[static_cast<size_t>(value)];
    };

    inline const std::string &to_str(const CurrencyType value) noexcept {
        static const std::vector<std::string> data_mode_0 = {
            "UNKNOWN",
            "USD",
            "EUR",
            "GBP",
            "BTC",
            "ETH",
            "USDT",
            "USDC",
            "RUB",
            "UAH",
            "KZT"
        };
        return data_mode_0[static_cast<size_t>(value)];
    };

    inline const std::string &to_str(const OrderState value) noexcept {
        static const std::vector<std::string> data_mode_0 = {
            "UNKNOWN",
            "CANCELED_TRADE",
            "WAITING_OPEN",
            "OPEN_SUCCESS",
            "OPEN_ERROR",
            "WAITING_CLOSE",
            "CHECK_ERROR",
            "WIN",
            "LOSS",
            "STANDOFF",
            "REFUND"
        };
        return data_mode_0[static_cast<size_t>(value)];
    };

    /// \brief Converts an OrderErrorCode value to its corresponding string representation.
    /// \param value The error code to convert.
    /// \return A string representation of the provided error code.
    inline const std::string &to_str(const OrderErrorCode value) noexcept {
        static const std::vector<std::string> str_data = {
            "Success",
            "Invalid symbol",
            "Invalid option type",
            "Invalid order type",
            "Invalid account type",
            "Invalid currency",
            "Amount below minimum",
            "Amount above maximum",
            "Refund below minimum",
            "Refund above maximum",
            "Low payout percentage",
            "Invalid duration",
            "Invalid expiry time",
            "Reached open trades limit",
            "Invalid request",
            "Long wait in the order queue",
            "Long wait for server response",
            "No network connection",
            "Forced client shutdown",
            "Parser error",
            "Canceled",
            "Insufficient balance",
            "Server responded with an error",
        };
        return str_data[static_cast<size_t>(value)];
    };

    inline const std::string &to_str(const MmSystemType value) noexcept {
        static const std::vector<std::string> data_mode_0 = {
            "NONE",
            "FIXED",
            "PERCENT",
            "KELLY_CRITERION",
            "MARTINGALE_SIGNAL",
            "MARTINGALE_SYMBOL",
            "MARTINGALE_BAR",
            "ANTI_MARTINGALE_SIGNAL",
            "ANTI_MARTINGALE_SYMBOL",
            "ANTI_MARTINGALE_BAR",
            "LABOUCHERE_SIGNAL",
            "LABOUCHERE_SYMBOL",
            "LABOUCHERE_BAR",
            "SKU_SIGNAL",
            "SKU_SYMBOL",
            "SKU_BAR"
        };
        return data_mode_0[static_cast<size_t>(value)];
    };

    inline const std::string &to_str(const AuthSetupState& value) noexcept {
        static const std::vector<std::string> data_mode_0 = {
            "UNKNOWN",
            "CONFIGURED",
            "CONNECTING",
            "CONNECTED",
            "CONNECTION_FAILED",
            "DISCONNECTED",
            "CONFIG_ERROR"
        };
        return data_mode_0[static_cast<size_t>(value)];
    };

    /// \brief Converts a ProxyType enum value to its string representation.
    /// \param value The ProxyType enum value to convert.
    /// \return String representation of the ProxyType value.
    inline const std::string &to_str(const kurlyk::ProxyType value) noexcept {
        static const std::vector<std::string> proxy_type_strings = {
            "HTTP",
            "HTTPS",
            "HTTP_1_0",
            "SOCKS4",
            "SOCKS4A",
            "SOCKS5",
            "SOCKS5_HOSTNAME"
        };
        return proxy_type_strings[static_cast<size_t>(value)];
    }

    //--------------------------------------------------------------------------

    template <typename EnumType>
    EnumType to_enum(const std::string &str);

    inline const bool to_enum(const std::string &str, ApiType &value) noexcept {
        static const std::map<std::string, ApiType> data =    {
            {"UNKNOWN",                 ApiType::UNKNOWN,     },
            {"SIMULATOR",               ApiType::SIMULATOR,   },
            {"CLICKER",                 ApiType::CLICKER,     },
            {"INTRADE_BAR",             ApiType::INTRADE_BAR, },
            {"REST_API",                ApiType::REST_API,    },
        };
        auto it = data.find(to_upper_case(str));
        if (it != data.end()) {
            value = it->second;
            return true;
        }
        return false;
    }

    inline const bool to_enum(const std::string &str, OptionType &value) noexcept {
        static const std::map<std::string,OptionType> data = {
            {"UNKNOWN", OptionType::UNKNOWN},
            {"SPRINT", OptionType::SPRINT},
            {"CLASSIC", OptionType::CLASSIC}
        };
        auto it = data.find(to_upper_case(str));
        if (it != data.end()) {
            value = it->second;
            return true;
        }
        return false;
    }

    inline const bool to_enum(const std::string &str, OrderType &value) noexcept {
        static const std::map<std::string,OrderType> data_mode_0 = {
            {"UNKNOWN", OrderType::UNKNOWN},
            {"BUY", OrderType::BUY},
            {"SELL", OrderType::SELL}
        };
        static const std::map<std::string,OrderType> data_mode_1 = {
            {"UNKNOWN", OrderType::UNKNOWN},
            {"UP", OrderType::BUY},
            {"DN", OrderType::SELL}
        };

        std::string upper_str = to_upper_case(str);
        auto it_mode_0 = data_mode_0.find(upper_str);
        if (it_mode_0 != data_mode_0.end()) {
            value = it_mode_0->second;
            return true;
        }

        auto it_mode_1 = data_mode_1.find(upper_str);
        if (it_mode_1 != data_mode_1.end()) {
            value = it_mode_1->second;
            return true;
        }

        return false;
    }

    inline const bool to_enum(const std::string &str, AccountType &value) noexcept {
        static const std::map<std::string, AccountType> account_type_map = {
            {"UNKNOWN", AccountType::UNKNOWN},
            {"DEMO", AccountType::DEMO},
            {"REAL", AccountType::REAL}
        };

        auto it = account_type_map.find(to_upper_case(str));
        if (it != account_type_map.end()) {
            value = it->second;
            return true;
        }
        return false;
    }

    template <>
    inline AccountType to_enum<AccountType>(const std::string &str) {
        static const std::map<std::string, AccountType> account_type_map = {
            {"UNKNOWN", AccountType::UNKNOWN},
            {"DEMO", AccountType::DEMO},
            {"REAL", AccountType::REAL}
        };

        auto it = account_type_map.find(to_upper_case(str));
        if (it != account_type_map.end()) {
            return it->second;
        }

        throw std::invalid_argument("Invalid AccountType string: " + str);
    }

    inline const bool to_enum(const std::string &str, CurrencyType &value) noexcept {
        static const std::map<std::string, CurrencyType> currency_type_map = {
            {"UNKNOWN", CurrencyType::UNKNOWN},
            {"USD",     CurrencyType::USD},
            {"EUR",     CurrencyType::EUR},
            {"GBP",     CurrencyType::GBP},
            {"BTC",     CurrencyType::BTC},
            {"ETH",     CurrencyType::ETH},
            {"USDT",    CurrencyType::USDT},
            {"USDC",    CurrencyType::USDC},
            {"RUB",     CurrencyType::RUB},
            {"UAH",     CurrencyType::UAH},
            {"KZT",     CurrencyType::KZT}
        };

        auto it = currency_type_map.find(to_upper_case(str));
        if (it != currency_type_map.end()) {
            value = it->second;
            return true;
        }
        return false;
    }

    template <>
    inline CurrencyType to_enum<CurrencyType>(const std::string &str) {
        static const std::map<std::string, CurrencyType> currency_type_map = {
            {"UNKNOWN", CurrencyType::UNKNOWN},
            {"USD",     CurrencyType::USD},
            {"EUR",     CurrencyType::EUR},
            {"GBP",     CurrencyType::GBP},
            {"BTC",     CurrencyType::BTC},
            {"ETH",     CurrencyType::ETH},
            {"USDT",    CurrencyType::USDT},
            {"USDC",    CurrencyType::USDC},
            {"RUB",     CurrencyType::RUB},
            {"UAH",     CurrencyType::UAH},
            {"KZT",     CurrencyType::KZT}
        };

        auto it = currency_type_map.find(to_upper_case(str));
        if (it != currency_type_map.end()) {
            return it->second;
        }

        throw std::invalid_argument("Invalid CurrencyType string: " + str);
    }

    inline const bool to_enum(const std::string &str, OrderState &value) noexcept {
        static const std::map<std::string, OrderState> data_mode_0 = {
            {"UNKNOWN",                 OrderState::UNKNOWN,        },
            {"CANCELED_TRADE",          OrderState::CANCELED_TRADE, },
            {"WAITING_OPEN",            OrderState::WAITING_OPEN,   },
            {"OPEN_SUCCESS",            OrderState::OPEN_SUCCESS,   },
            {"OPEN_ERROR",              OrderState::OPEN_ERROR,     },
            {"CHECK_ERROR",             OrderState::CHECK_ERROR,    },
            {"WIN",                     OrderState::WIN,            },
            {"LOSS",                    OrderState::LOSS,           },
            {"STANDOFF",                OrderState::STANDOFF,       },
            {"REFUND",                  OrderState::REFUND,         }
        };
        auto it_mode_0 = data_mode_0.find(to_upper_case(str));
        if (it_mode_0 != data_mode_0.end()) {
            value = it_mode_0->second;
            return true;
        }
        return false;
    }

    inline const bool to_enum(const std::string &str, MmSystemType &value) noexcept {
        static const std::map<std::string, MmSystemType> data_mode_0 = {
            {"NONE",                   MmSystemType::NONE,  				},
            {"FIXED",                  MmSystemType::FIXED,                 },
            {"PERCENT",                MmSystemType::PERCENT,               },
            {"KELLY_CRITERION",        MmSystemType::KELLY_CRITERION,       },
            {"MARTINGALE_SIGNAL",      MmSystemType::MARTINGALE_SIGNAL,     },
            {"MARTINGALE_SYMBOL",      MmSystemType::MARTINGALE_SYMBOL,     },
            {"MARTINGALE_BAR",         MmSystemType::MARTINGALE_BAR,        },
            {"ANTI_MARTINGALE_SIGNAL", MmSystemType::ANTI_MARTINGALE_SIGNAL,},
            {"ANTI_MARTINGALE_SYMBOL", MmSystemType::ANTI_MARTINGALE_SYMBOL,},
            {"ANTI_MARTINGALE_BAR",    MmSystemType::ANTI_MARTINGALE_BAR,   },
            {"LABOUCHERE_SIGNAL",      MmSystemType::LABOUCHERE_SIGNAL,     },
            {"LABOUCHERE_SYMBOL",      MmSystemType::LABOUCHERE_SYMBOL,     },
            {"LABOUCHERE_BAR",         MmSystemType::LABOUCHERE_BAR,        },
            {"SKU_SIGNAL",             MmSystemType::SKU_SIGNAL,            },
            {"SKU_SYMBOL",             MmSystemType::SKU_SYMBOL,            },
            {"SKU_BAR",                MmSystemType::SKU_BAR                }
        };
        auto it_mode_0 = data_mode_0.find(to_upper_case(str));
        if (it_mode_0 != data_mode_0.end()) {
            value = it_mode_0->second;
            return true;
        }
        return false;
    }

    /// \brief Converts a string to its corresponding ProxyType enum value.
    /// \param str The string representation of the ProxyType value.
    /// \return The ProxyType enum value corresponding to the string.
    /// \throws std::invalid_argument if the string does not match any ProxyType value.
    template <>
    inline kurlyk::ProxyType to_enum<kurlyk::ProxyType>(const std::string &str) {
        static const std::map<std::string, kurlyk::ProxyType> proxy_type_map = {
            {"HTTP", kurlyk::ProxyType::HTTP},
            {"HTTPS", kurlyk::ProxyType::HTTPS},
            {"HTTP_1_0", kurlyk::ProxyType::HTTP_1_0},
            {"SOCKS4", kurlyk::ProxyType::SOCKS4},
            {"SOCKS4A", kurlyk::ProxyType::SOCKS4A},
            {"SOCKS5", kurlyk::ProxyType::SOCKS5},
            {"SOCKS5_HOSTNAME", kurlyk::ProxyType::SOCKS5_HOSTNAME}
        };

        auto it = proxy_type_map.find(to_upper_case(str));
        if (it != proxy_type_map.end()) {
            return it->second;
        }

        throw std::invalid_argument("Invalid ProxyType string: " + str);
    }

}; // namespace optionx

#endif // _OPTIONX_ENUMS_HPP_INCLUDED
