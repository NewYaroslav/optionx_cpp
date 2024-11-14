#pragma once
#ifndef _OPTIONX_IACCOUNT_INFO_DATA_HPP_INCLUDED
#define _OPTIONX_IACCOUNT_INFO_DATA_HPP_INCLUDED

/// \file IAccountInfoData.hpp
/// \brief Contains the IAccountInfoData interface for handling account information retrieval.

#include "../utils/Enums.hpp"
#include "../utils/AccountInfoRequest.hpp"
#include <memory>
#include <string>

namespace optionx {

    /// \class IAccountInfoData
    /// \brief Interface for retrieving various types of account information.
    /// This interface provides a unified approach to accessing account data across different brokers.
    class IAccountInfoData {
    public:
        /// \brief Retrieves account information based on the request type.
        /// \tparam T The expected return type for the requested data.
        /// \param request Specifies the type of account information requested.
        /// \return The account information as type T.
        template<class T>
        const T get_account_info(const AccountInfoRequest& request);

        /// \brief Retrieves account information by AccountInfoType.
        /// \param type Type of account information requested.
        /// \param timestamp Timestamp to use for the request (optional).
        template<class T>
        const T get_account_info(AccountInfoType type, int64_t timestamp = 0) {
            AccountInfoRequest request;
            request.type = type;
            request.timestamp = timestamp;
            return get_account_info<T>(request);
        }

        /// \brief Retrieves symbol availability information.
        /// \param symbol Symbol for which availability is checked.
        /// \param timestamp Timestamp to use for the request (optional).
        template<class T>
        const T get_account_info(const std::string &symbol, int64_t timestamp = 0) {
            AccountInfoRequest request;
            request.type = AccountInfoType::SYMBOL_AVAILABILITY;
            request.symbol = symbol;
            request.timestamp = timestamp;
            return get_account_info<T>(request);
        }

        /// \brief Checks if an OptionType is available.
        /// \param option OptionType for which availability is checked.
        /// \param timestamp Timestamp to use for the request (optional).
        template<class T>
        const T get_account_info(OptionType option, int64_t timestamp = 0) {
            AccountInfoRequest request;
            request.type = AccountInfoType::OPTION_TYPE_AVAILABILITY;
            request.option = option;
            request.timestamp = timestamp;
            return get_account_info<T>(request);
        }

        /// \brief Checks if an OrderType is available.
        /// \param order OrderType for which availability is checked.
        /// \param timestamp Timestamp to use for the request (optional).
        template<class T>
        const T get_account_info(OrderType order, int64_t timestamp = 0) {
            AccountInfoRequest request;
            request.type = AccountInfoType::ORDER_TYPE_AVAILABILITY;
            request.order = order;
            request.timestamp = timestamp;
            return get_account_info<T>(request);
        }

        /// \brief Checks if an AccountType is available.
        /// \param account AccountType for which availability is checked.
        /// \param timestamp Timestamp to use for the request (optional).
        template<class T>
        const T get_account_info(AccountType account, int64_t timestamp = 0) {
            AccountInfoRequest request;
            request.type = AccountInfoType::ACCOUNT_TYPE_AVAILABILITY;
            request.account = account;
            request.timestamp = timestamp;
            return get_account_info<T>(request);
        }

        /// \brief Checks if a CurrencyType is available.
        /// \param currency CurrencyType for which availability is checked.
        /// \param timestamp Timestamp to use for the request (optional).
        template<class T>
        const T get_account_info(CurrencyType currency, int64_t timestamp = 0) {
            AccountInfoRequest request;
            request.type = AccountInfoType::CURRENCY_AVAILABILITY;
            request.currency = currency;
            request.timestamp = timestamp;
            return get_account_info<T>(request);
        }

        /// \brief Retrieves account information for a specific TradeRequest.
        /// \param info_type Type of information requested.
        /// \param trade_request Shared pointer to a TradeRequest instance.
        /// \param timestamp Timestamp to use for the request (optional).
        template<class T>
        const T get_account_info(AccountInfoType info_type, std::shared_ptr<TradeRequest>& trade_request, int64_t timestamp = 0) {
            AccountInfoRequest request(trade_request, info_type);
            request.timestamp = timestamp;
            return get_account_info<T>(request);
        }

        /// \brief Retrieves the API type associated with this account data.
        /// \return The type of API used.
        virtual ApiType api_type() const {
            return ApiType::UNKNOWN;
        }

        /// \brief Creates a unique pointer to a clone of this account info data instance.
        /// \return Unique pointer to a cloned IAccountInfoData instance.
        virtual std::unique_ptr<IAccountInfoData> clone_unique() const = 0;

        /// \brief Creates a shared pointer to a clone of this account info data instance.
        /// \return Shared pointer to a cloned IAccountInfoData instance.
        virtual std::shared_ptr<IAccountInfoData> clone_shared() const = 0;

        virtual ~IAccountInfoData() = default;

    protected:
        /// \brief Retrieves boolean account information based on the request type.
        virtual bool get_account_info_bool(const AccountInfoRequest& request) = 0;

        /// \brief Retrieves integer account information based on the request type.
        virtual int64_t get_account_info_int64(const AccountInfoRequest& request) = 0;

        /// \brief Retrieves floating-point account information based on the request type.
        virtual double get_account_info_f64(const AccountInfoRequest& request) = 0;

        /// \brief Retrieves string account information based on the request type.
        virtual std::string get_account_info_str(const AccountInfoRequest& request) = 0;

        /// \brief Retrieves the account type.
        virtual AccountType get_account_type(const AccountInfoRequest& request) = 0;

        /// \brief Retrieves the account currency type.
        virtual CurrencyType get_account_currency(const AccountInfoRequest& request) = 0;
    };

    // Template specializations for retrieving specific account information types

    /// \brief Template specialization for retrieving boolean account information.
    template<>
    bool IAccountInfoData::get_account_info<bool>(const AccountInfoRequest& request) {
        return get_account_info_bool(request);
    }

    /// \brief Template specialization for retrieving integer account information.
    template<>
    const int IAccountInfoData::get_account_info<int>(const AccountInfoRequest& request) {
        return static_cast<int>(get_account_info_int64(request));
    }

    /// \brief Template specialization for retrieving int64_t account information.
    template<>
    const int64_t IAccountInfoData::get_account_info<int64_t>(const AccountInfoRequest& request) {
        return get_account_info_int64(request);
    }

    /// \brief Template specialization for retrieving size_t account information.
    template<>
    const size_t IAccountInfoData::get_account_info<size_t>(const AccountInfoRequest& request) {
        return static_cast<size_t>(get_account_info_int64(request));
    }

    /// \brief Template specialization for retrieving double account information.
    template<>
    const double IAccountInfoData::get_account_info<double>(const AccountInfoRequest& request) {
        return get_account_info_f64(request);
    }

    /// \brief Template specialization for retrieving string account information.
    template<>
    const std::string IAccountInfoData::get_account_info<std::string>(const AccountInfoRequest& request) {
        return get_account_info_str(request);
    }

    /// \brief Template specialization for retrieving AccountType.
    template<>
    const AccountType IAccountInfoData::get_account_info<AccountType>(const AccountInfoRequest& request) {
        return get_account_type(request);
    }

    /// \brief Template specialization for retrieving CurrencyType.
    template<>
    const CurrencyType IAccountInfoData::get_account_info<CurrencyType>(const AccountInfoRequest& request) {
        return get_account_currency(request);
    }

    /// \brief Template specialization for retrieving ApiType.
    template<>
    const ApiType IAccountInfoData::get_account_info<ApiType>(const AccountInfoRequest&) {
        return api_type();
    }

    /// \brief Default template specialization for unsupported types.
    /// This ensures compile-time error for unsupported types used with get_account_info.
    template<class T>
    T IAccountInfoData::get_account_info(const AccountInfoRequest& request) {
        static_assert(sizeof(T) == 0, "Unsupported type for get_account_info");
        return T();
    }

}; // namespace optionx

#endif // _OPTIONX_IACCOUNT_INFO_DATA_HPP_INCLUDED
