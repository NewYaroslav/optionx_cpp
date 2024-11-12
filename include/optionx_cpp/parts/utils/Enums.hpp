#pragma once
#ifndef _OPTIONX_ENUMS_HPP_INCLUDED
#define _OPTIONX_ENUMS_HPP_INCLUDED

/// \file Enums.hpp
/// \brief

namespace optionx {

    /** \brief Типы API
     */
    enum class ApiType {
        UNKNOWN = 0,
        SIMULATOR,
        CLICKER,
        INTRADE_BAR,
        REST_API,
    };

    /** \brief Тип опциона
     */
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
    enum class OrderState {
        UNKNOWN = 0,
        CANCELED_TRADE,
        WAITING_OPEN,
        OPEN_SUCCESS,
        OPEN_ERROR,
        WAITING_CLOSE,
        CHECK_ERROR,
        WIN,
        LOSS,
        STANDOFF,
        REFUND
    };

    /** \brief Тип информации об аккаунте
     */
    enum class AccountInfoType {
        UNKNOWN = 0,
        PARTNER_ID,             /// ID партнера (строка)
        USER_ID,                /// ID юзера (строка)
        BALANCE,                /// Баланс
        BONUS,                  /// Бонус
        CONNECTION_STATUS,      /// Состояние подключения
        VIP_STATUS,             /// Вип статус аккаунта
        BROKER,                 /// Тип брокера
        ACCOUNT_TYPE,           /// Тип аккаунта
        CURRENCY,               /// Валюта аккаунта
        OPEN_ORDERS,            /// Количество открытых ордеров
        MAX_ORDERS,             /// Максимальное количество открытых ордеров
        PAYOUT,                 /// Процент выплаты
        MIN_AMOUNT,             /// Минимальный размер сделки
        MAX_AMOUNT,             /// Максимальный размер сделки
        MAX_REFUND,             /// Максимальный процент возврата
        MIN_DURATION,           /// Минимальная продолжительность опциона
        MAX_DURATION,           /// Максимальная продолжительность опциона
        START_TIME,             /// Начало торгового дня
        END_TIME                /// Конец торгового дня
    };

    /** \brief Типы ошибок
     */
    enum class OrderErrorCode {
        SUCCESS = 0,
        INVALID_SYMBOL,
        INVALID_OPTION,
        INVALID_ORDER,
        INVALID_ACCOUNT,
        INVALID_CURRENCY,
        AMOUNT_TOO_LOW,
        AMOUNT_TOO_HIGH,
        REFUND_TOO_LOW,
        REFUND_TOO_HIGH,
        PAYOUT_TOO_LOW,
        INVALID_DURATION,
        INVALID_EXPIRY_TIME,
        LIMIT_OPEN_ORDERS,
        INVALID_REQUEST,
        LONG_QUEUE_WAIT,
        LONG_RESPONSE_WAIT,
        NO_CONNECTION,
        CLIENT_FORCED_CLOSE,
        PARSER_ERROR,
        CANCELED_TRADE
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

    inline const std::string &to_str(const OrderErrorCode value) noexcept {
        static const std::vector<std::string> data_mode_0 = {
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
            "Canceled"
        };
        return data_mode_0[static_cast<size_t>(value)];
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

    //--------------------------------------------------------------------------

    inline const bool to_enum(const std::string &str, ApiType &value) noexcept {
        static const std::map<std::string, ApiType> data_mode_0 =    {
            {"UNKNOWN",                 ApiType::UNKNOWN,     },
            {"SIMULATOR",               ApiType::SIMULATOR,   },
            {"CLICKER",                 ApiType::CLICKER,     },
            {"INTRADE_BAR",             ApiType::INTRADE_BAR, },
            {"REST_API",                ApiType::REST_API,    },
        };
        auto it_mode_0 = data_mode_0.find(str);
        if (it_mode_0 != data_mode_0.end()) {
            value = it_mode_0->second;
            return true;
        }
        return false;
    }

    inline const bool to_enum(const std::string &str, OptionType &value) noexcept {
        static const std::map<std::string,OptionType> data_mode_0 = {
            {"UNKNOWN", OptionType::UNKNOWN},
            {"SPRINT", OptionType::SPRINT},
            {"CLASSIC", OptionType::CLASSIC}
        };
        static const std::map<std::string,OptionType> data_mode_1 = {
            {"UNKNOWN", OptionType::UNKNOWN},
            {"sprint", OptionType::SPRINT},
            {"classic", OptionType::CLASSIC}
        };
        auto it_mode_0 = data_mode_0.find(str);
        if (it_mode_0 != data_mode_0.end()) {
            value = it_mode_0->second;
            return true;
        }

        auto it_mode_1 = data_mode_1.find(str);
        if (it_mode_1 != data_mode_1.end()) {
            value = it_mode_1->second;
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
            {"buy", OrderType::BUY},
            {"sell", OrderType::SELL}
        };
        static const std::map<std::string,OrderType> data_mode_2 = {
            {"UNKNOWN", OrderType::UNKNOWN},
            {"UP", OrderType::BUY},
            {"DN", OrderType::SELL}
        };
        static const std::map<std::string,OrderType> data_mode_3 = {
            {"UNKNOWN", OrderType::UNKNOWN},
            {"up", OrderType::BUY},
            {"dn", OrderType::SELL}
        };
        auto it_mode_0 = data_mode_0.find(str);
        if (it_mode_0 != data_mode_0.end()) {
            value = it_mode_0->second;
            return true;
        }

        auto it_mode_1 = data_mode_1.find(str);
        if (it_mode_1 != data_mode_1.end()) {
            value = it_mode_1->second;
            return true;
        }

        auto it_mode_2 = data_mode_2.find(str);
        if (it_mode_2 != data_mode_2.end()) {
            value = it_mode_2->second;
            return true;
        }

        auto it_mode_3 = data_mode_3.find(str);
        if (it_mode_3 != data_mode_3.end()) {
            value = it_mode_3->second;
            return true;
        }
        return false;
    }

    inline const bool to_enum(const std::string &str, AccountType &value) noexcept {
        static const std::map<std::string,AccountType> data_mode_0 = {
            {"UNKNOWN", AccountType::UNKNOWN},
            {"DEMO", AccountType::DEMO},
            {"REAL", AccountType::REAL}
        };
        static const std::map<std::string,AccountType> data_mode_1 = {
            {"UNKNOWN", AccountType::UNKNOWN},
            {"Demo", AccountType::DEMO},
            {"Real", AccountType::REAL}
        };
        auto it_mode_0 = data_mode_0.find(str);
        if (it_mode_0 != data_mode_0.end()) {
            value = it_mode_0->second;
            return true;
        }

        auto it_mode_1 = data_mode_1.find(str);
        if (it_mode_1 != data_mode_1.end()) {
            value = it_mode_1->second;
            return true;
        }
        return false;
    }

    inline const bool to_enum(const std::string &str, CurrencyType &value) noexcept {
        static const std::map<std::string,CurrencyType> data_mode_0 = {
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
        static const std::map<std::string,CurrencyType> data_mode_1 = {
            {"UNKNOWN", CurrencyType::UNKNOWN},
            {"usd",     CurrencyType::USD},
            {"eur",     CurrencyType::EUR},
            {"gbp",     CurrencyType::GBP},
            {"btc",     CurrencyType::BTC},
            {"eth",     CurrencyType::ETH},
            {"usdt",    CurrencyType::USDT},
            {"usdc",    CurrencyType::USDC},
            {"rub",     CurrencyType::RUB},
            {"uah",     CurrencyType::UAH},
            {"kzt",     CurrencyType::KZT}
        };
        auto it_mode_0 = data_mode_0.find(str);
        if (it_mode_0 != data_mode_0.end()) {
            value = it_mode_0->second;
            return true;
        }

        auto it_mode_1 = data_mode_1.find(str);
        if (it_mode_1 != data_mode_1.end()) {
            value = it_mode_1->second;
            return true;
        }
        return false;
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
        auto it_mode_0 = data_mode_0.find(str);
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
        auto it_mode_0 = data_mode_0.find(str);
        if (it_mode_0 != data_mode_0.end()) {
            value = it_mode_0->second;
            return true;
        }
        return false;
    }

}; // namespace optionx

#endif // _OPTIONX_ENUMS_HPP_INCLUDED
