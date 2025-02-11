#pragma once
#ifndef _OPTIONX_ACCOUNT_ENUMS_HPP_INCLUDED
#define _OPTIONX_ACCOUNT_ENUMS_HPP_INCLUDED

/// \file enums.hpp
/// \brief Account metadata and validation parameters enumeration.
/// \details Defines data types and validation checks available for account configuration
///          and trade request pre-processing. Used to query account capabilities, limits
///          and validate trading parameters.

namespace optionx {

    /// \enum AccountInfoType
    /// \brief Specifies account-related data queries and validation checks.
    /// \details Each entry represents either:
    ///          - A retrievable account property (e.g., balance, currency)
    ///          - A validation rule for trade requests (e.g., amount limits)
    ///          - Availability flags for trading features
    enum class AccountInfoType {
        // Account properties
        UNKNOWN = 0,              ///< Invalid or unspecified query type
        PARTNER_ID,               ///< Broker partnership identifier (UTF-8 string)
        USER_ID,                  ///< Unique user account ID (UTF-8 string)
        BALANCE,                  ///< Current funds in account currency (double)
        BONUS,                    ///< Available bonus funds (double)
        CONNECTION_STATUS,        ///< Connection state: 0-disconnected, 1-connected (int)
        VIP_STATUS,               ///< VIP tier: 0-standard, 1-VIP, 2-premium (int)
        PLATFORM_TYPE,            ///< API protocol version (implementation-specific enum)
        ACCOUNT_TYPE,             ///< Account classification (DEMO/REAL)
        CURRENCY,                 ///< Base currency (e.g., USD, EUR, BTC)
        OPEN_TRADES,              ///< Currently active trades count (uint32)
        MAX_TRADES,               ///< Maximum concurrent trades allowed (uint32)
        PAYOUT,                   ///< Default payout ratio (0.0-1.0)

        // Trading limits
        MIN_AMOUNT,               ///< Minimum allowed trade amount (double)
        MAX_AMOUNT,               ///< Maximum allowed trade amount (double)
        MAX_REFUND,               ///< Maximum refund percentage (0.0-1.0)
        MIN_DURATION,             ///< Shortest allowed option duration in seconds (uint64)
        MAX_DURATION,             ///< Longest allowed option duration in seconds (uint64)

        // Session parameters
        START_TIME,               ///< Trading session start (seconds since midnight UTC)
        END_TIME,                 ///< Trading session end (seconds since midnight UTC)
        ORDER_QUEUE_TIMEOUT,      ///< Max queue wait time in milliseconds (uint32)
        RESPONSE_TIMEOUT,         ///< Trade execution timeout in milliseconds (uint32)
        ORDER_INTERVAL_MS,        ///< Minimum delay between orders in ms (uint32)

        // Feature availability flags (bool)
        SYMBOL_AVAILABILITY,      ///< Check if symbol is tradable
        OPTION_TYPE_AVAILABILITY, ///< Check if option type is supported
        ORDER_TYPE_AVAILABILITY,  ///< Check if order direction is allowed
        ACCOUNT_TYPE_AVAILABILITY,///< Check if account type is permitted
        CURRENCY_AVAILABILITY,    ///< Check if currency is enabled

        // Validation checks (bool)
        TRADE_LIMIT_NOT_EXCEEDED, ///< Verify open trades < MAX_TRADES
        AMOUNT_BELOW_MAX,         ///< Verify trade amount <= MAX_AMOUNT
        AMOUNT_ABOVE_MIN,         ///< Verify trade amount >= MIN_AMOUNT
        REFUND_BELOW_MAX,         ///< Verify refund rate <= MAX_REFUND
        REFUND_ABOVE_MIN,         ///< Verify refund rate >= 0.0
        DURATION_AVAILABLE,       ///< Verify MIN_DURATION <= duration <= MAX_DURATION
        EXPIRATION_DATE_AVAILABLE,///< Verify expiry within trading session hours
        PAYOUT_ABOVE_MIN,         ///< Verify payout >= system minimum
        AMOUNT_BELOW_BALANCE      ///< Verify trade amount <= available balance
    };

    /// \brief Converts a ProxyType enum value to its string representation.
    /// \param value The ProxyType enum value to convert.
    /// \return String representation of the ProxyType value.
    inline const std::string &to_str(const kurlyk::ProxyType value) noexcept {
        static const std::vector<std::string> data_str = {
            "HTTP",
            "HTTPS",
            "HTTP_1_0",
            "SOCKS4",
            "SOCKS4A",
            "SOCKS5",
            "SOCKS5_HOSTNAME"
        };
        return data_str[static_cast<size_t>(value)];
    }

    /// \brief Converts a string to its corresponding ProxyType enum value.
    /// \param str The string representation of the ProxyType value.
    /// \return The ProxyType enum value corresponding to the string.
    /// \throws std::invalid_argument if the string does not match any ProxyType value.
    template <>
    inline kurlyk::ProxyType to_enum<kurlyk::ProxyType>(const std::string &str) {
        static const std::map<std::string, kurlyk::ProxyType> data_str = {
            {"HTTP", kurlyk::ProxyType::HTTP},
            {"HTTPS", kurlyk::ProxyType::HTTPS},
            {"HTTP_1_0", kurlyk::ProxyType::HTTP_1_0},
            {"SOCKS4", kurlyk::ProxyType::SOCKS4},
            {"SOCKS4A", kurlyk::ProxyType::SOCKS4A},
            {"SOCKS5", kurlyk::ProxyType::SOCKS5},
            {"SOCKS5_HOSTNAME", kurlyk::ProxyType::SOCKS5_HOSTNAME}
        };

        auto it = data_str.find(utils::to_upper_case(str));
        if (it != data_str.end()) {
            return it->second;
        }

        throw std::invalid_argument("Invalid ProxyType string: " + str);
    }

    /// \brief Converts kurlyk::ProxyType to JSON.
    inline void to_json(nlohmann::json& j, const kurlyk::ProxyType& type) {
        j = optionx::to_str(type);
    }

    /// \brief Converts JSON to kurlyk::ProxyType.
    inline void from_json(const nlohmann::json& j, kurlyk::ProxyType& type) {
        type = optionx::to_enum<kurlyk::ProxyType>(j.get<std::string>());
    }

	/// \brief Stream output operator for kurlyk::ProxyType.
	std::ostream& operator<<(std::ostream& os, kurlyk::ProxyType value) {
        os << optionx::to_str(value);
        return os;
    }

//------------------------------------------------------------------------------

    /// \enum AccountUpdateStatus
    /// \brief Represents the type of account update or status change.
    enum class AccountUpdateStatus {
        UNKNOWN = 0,          ///< Unknown status.
        BALANCE_UPDATED,      ///< Balance updated.
        ACCOUNT_TYPE_CHANGED, ///< Account type changed.
        CURRENCY_CHANGED,     ///< Currency changed.
        OPEN_TRADES_CHANGED,  ///< Number of open trades changed.
        CONNECTING,           ///< Connecting to account.
        CONNECTED,            ///< Connection established.
        DISCONNECTED,         ///< Connection disconnected.
        FAILED_TO_CONNECT     ///< Connection attempt failed.
    };

    /// \brief Converts AccountUpdateStatus to its string representation.
    /// \param value The AccountUpdateStatus enumeration value.
    /// \return Constant reference to the corresponding string.
	inline const std::string &to_str(AccountUpdateStatus value) noexcept {
        static const std::vector<std::string> str_data = {
            "UNKNOWN",
            "BALANCE_UPDATED",
            "ACCOUNT_TYPE_CHANGED",
            "CURRENCY_CHANGED",
            "OPEN_TRADES_CHANGED",
            "CONNECTING",
            "CONNECTED",
            "DISCONNECTED",
            "FAILED_TO_CONNECT"};
		return str_data[static_cast<size_t>(value)];
    };

	/// \brief Converts string to AccountUpdateStatus enumeration.
    /// \param str Input string to convert.
    /// \param value Output enumeration value.
    /// \return True if conversion succeeded.
	inline const bool to_enum(const std::string &str, AccountUpdateStatus &value) noexcept {
        static const std::unordered_map<std::string, AccountUpdateStatus> str_data = {
            {"UNKNOWN",              AccountUpdateStatus::UNKNOWN             },
            {"BALANCE_UPDATED",      AccountUpdateStatus::BALANCE_UPDATED     },
            {"ACCOUNT_TYPE_CHANGED", AccountUpdateStatus::ACCOUNT_TYPE_CHANGED},
            {"CURRENCY_CHANGED",     AccountUpdateStatus::CURRENCY_CHANGED    },
            {"OPEN_TRADES_CHANGED",  AccountUpdateStatus::OPEN_TRADES_CHANGED },
            {"CONNECTING",           AccountUpdateStatus::CONNECTING          },
            {"CONNECTED",            AccountUpdateStatus::CONNECTED           },
            {"DISCONNECTED",         AccountUpdateStatus::DISCONNECTED        },
            {"FAILED_TO_CONNECT",    AccountUpdateStatus::FAILED_TO_CONNECT   }
        };
        auto it = str_data.find(utils::to_upper_case(str));
        if (it != str_data.end()) {
            value = it->second;
            return true;
        }
        return false;
    }

	/// \brief Template specialization for AccountUpdateStatus enum conversion.
	template <>
    inline AccountUpdateStatus to_enum<AccountUpdateStatus>(const std::string &str) {
        AccountUpdateStatus value;
        if (!to_enum(str, value)) {
            throw std::invalid_argument("Invalid AccountUpdateStatus string: " + str);
        }
		return value;
    }

	/// \brief Converts AccountUpdateStatus to JSON.
    inline void to_json(nlohmann::json& j, const AccountUpdateStatus& type) {
        j = optionx::to_str(type);
    }

    /// \brief Converts JSON to AccountUpdateStatus.
    inline void from_json(const nlohmann::json& j, AccountUpdateStatus& type) {
        type = optionx::to_enum<AccountUpdateStatus>(j.get<std::string>());
    }

	/// \brief Stream output operator for AccountUpdateStatus.
	std::ostream& operator<<(std::ostream& os, AccountUpdateStatus value) {
        os << optionx::to_str(value);
        return os;
    }

} // namespace optionx

#endif // _OPTIONX_ACCOUNT_ENUMS_HPP_INCLUDED
