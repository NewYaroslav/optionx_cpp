#pragma once
#ifndef _OPTIONX_BASE_ACCOUNT_INFO_DATA_HPP_INCLUDED
#define _OPTIONX_BASE_ACCOUNT_INFO_DATA_HPP_INCLUDED

/// \file BaseAccountInfoData.hpp
/// \brief Defines the BaseAccountInfoData class, which provides an interface for retrieving account-related information.

namespace optionx {

    /// \class BaseAccountInfoData
    /// \brief Abstract base class for retrieving various types of account information.
    ///
    /// This class provides a unified interface for accessing account data across different brokers.
    /// Implementing classes must override protected methods to provide actual data retrieval.
    class BaseAccountInfoData {
    public:
        /// \brief Retrieves account information based on a detailed request.
        /// \tparam T The expected type of the returned value.
        /// \param request The request containing specific parameters for data retrieval.
        /// \return The requested account information of type `T`.
        template<class T>
        const T get_info(const AccountInfoRequest& request) const;

        /// \brief Retrieves account information by `AccountInfoType`.
        /// \tparam T The expected type of the returned value.
        /// \param type The specific type of account information.
        /// \param timestamp Optional timestamp for retrieving historical values (default: 0).
        /// \return The requested account information of type `T`.
        template<class T>
        const T get_info(AccountInfoType type, int64_t timestamp = 0) const {
            AccountInfoRequest request;
            request.type = type;
            request.timestamp = timestamp;
            return get_info<T>(request);
        }

        /// \brief Retrieves information about the availability of a symbol.
        /// \tparam T The expected type of the returned value.
        /// \param symbol The trading symbol (e.g., currency pair, stock ticker).
        /// \param timestamp Optional timestamp for retrieving historical values (default: 0).
        /// \return The requested symbol availability information of type `T`.
        template<class T>
        const T get_by_symbol(const std::string &symbol, int64_t timestamp = 0) const {
            AccountInfoRequest request;
            request.type = AccountInfoType::SYMBOL_AVAILABILITY;
            request.symbol = symbol;
            request.timestamp = timestamp;
            return get_info<T>(request);
        }

        /// \brief Checks if a specific `OptionType` is available.
        /// \tparam T The expected type of the returned value.
        /// \param option_type The option type (e.g., Classic, Sprint).
        /// \param timestamp Optional timestamp for retrieving historical values (default: 0).
        /// \return The availability status of the option type as type `T`.
        template<class T>
        const T get_by_option(OptionType option_type, int64_t timestamp = 0) const {
            AccountInfoRequest request;
            request.type = AccountInfoType::OPTION_TYPE_AVAILABILITY;
            request.option_type = option_type;
            request.timestamp = timestamp;
            return get_info<T>(request);
        }

        /// \brief Checks if a specific `OrderType` is available.
        /// \tparam T The expected type of the returned value.
        /// \param order_type The order type (e.g., Market, Limit, Stop).
        /// \param timestamp Optional timestamp for retrieving historical values (default: 0).
        /// \return The availability status of the order type as type `T`.
        template<class T>
        const T get_by_order(OrderType order_type, int64_t timestamp = 0) const {
            AccountInfoRequest request;
            request.type = AccountInfoType::ORDER_TYPE_AVAILABILITY;
            request.order_type = order_type;
            request.timestamp = timestamp;
            return get_info<T>(request);
        }

        /// \brief Checks if a specific `AccountType` is available.
        /// \tparam T The expected type of the returned value.
        /// \param account_type The account type (e.g., Demo, Real).
        /// \param timestamp Optional timestamp for retrieving historical values (default: 0).
        /// \return The availability status of the account type as type `T`.
        template<class T>
        const T get_by_account(AccountType account_type, int64_t timestamp = 0) const {
            AccountInfoRequest request;
            request.type = AccountInfoType::ACCOUNT_TYPE_AVAILABILITY;
            request.account_type = account_type;
            request.timestamp = timestamp;
            return get_info<T>(request);
        }

        /// \brief Checks if a specific `CurrencyType` is available.
        /// \tparam T The expected type of the returned value.
        /// \param currency The currency type (e.g., USD, EUR).
        /// \param timestamp Optional timestamp for retrieving historical values (default: 0).
        /// \return The availability status of the currency type as type `T`.
        template<class T>
        const T get_by_currency(CurrencyType currency, int64_t timestamp = 0) const {
            AccountInfoRequest request;
            request.type = AccountInfoType::CURRENCY_AVAILABILITY;
            request.currency = currency;
            request.timestamp = timestamp;
            return get_info<T>(request);
        }

        /// \brief Retrieves account information specific to a trade request.
        /// \tparam T The expected type of the returned value.
        /// \param info_type The type of information to retrieve.
        /// \param trade_request The trade request associated with the information query.
        /// \param timestamp Optional timestamp for retrieving historical values (default: 0).
        /// \return The requested account information of type `T`.
        template<class T>
        const T get_for_trade(
                AccountInfoType info_type,
                const std::shared_ptr<TradeRequest>& trade_request,
                int64_t timestamp = 0) const {
            AccountInfoRequest request(trade_request, info_type);
            request.timestamp = timestamp;
            return get_info<T>(request);
        }

        /// \brief Retrieves the API type associated with this account data.
        /// \return The platform type (`PlatformType::UNKNOWN` by default).
        virtual const PlatformType platform_type() const {
            return PlatformType::UNKNOWN;
        }

        /// \brief Creates a unique pointer to a clone of this account info data instance.
        /// \return Unique pointer to a cloned `BaseAccountInfoData` instance.
        virtual std::unique_ptr<BaseAccountInfoData> clone_unique() const = 0;

        /// \brief Creates a shared pointer to a clone of this account info data instance.
        /// \return Shared pointer to a cloned `BaseAccountInfoData` instance.
        virtual std::shared_ptr<BaseAccountInfoData> clone_shared() const = 0;

        virtual ~BaseAccountInfoData() = default;

    protected:
        /// \brief Retrieves boolean account information based on the request type.
        virtual bool get_info_bool(const AccountInfoRequest& request) const = 0;

        /// \brief Retrieves integer account information based on the request type.
        virtual int64_t get_info_int64(const AccountInfoRequest& request) const = 0;

        /// \brief Retrieves floating-point account information based on the request type.
        virtual double get_info_f64(const AccountInfoRequest& request) const = 0;

        /// \brief Retrieves string account information based on the request type.
        virtual std::string get_info_str(const AccountInfoRequest& request) const = 0;

        /// \brief Retrieves the account type.
        virtual AccountType get_info_account_type(const AccountInfoRequest& request) const = 0;

        /// \brief Retrieves the account currency type.
        virtual CurrencyType get_info_currency(const AccountInfoRequest& request) const = 0;
    };

    // Template specializations for retrieving specific account information types

    /// \brief Template specialization for retrieving boolean account information.
    template<>
    const bool BaseAccountInfoData::get_info<bool>(const AccountInfoRequest& request) const {
        return get_info_bool(request);
    }

    /// \brief Template specialization for retrieving integer account information.
    template<>
    const int BaseAccountInfoData::get_info<int>(const AccountInfoRequest& request) const {
        return static_cast<int>(get_info_int64(request));
    }

    /// \brief Template specialization for retrieving int64_t account information.
    template<>
    const int64_t BaseAccountInfoData::get_info<int64_t>(const AccountInfoRequest& request) const {
        return get_info_int64(request);
    }

    /// \brief Template specialization for retrieving size_t account information.
    template<>
    const size_t BaseAccountInfoData::get_info<size_t>(const AccountInfoRequest& request) const {
        return static_cast<size_t>(get_info_int64(request));
    }

    /// \brief Template specialization for retrieving CurrencyType account information.
    template<>
    const CurrencyType BaseAccountInfoData::get_info<CurrencyType>(const AccountInfoRequest& request) const {
        return static_cast<CurrencyType>(get_info_int64(request));
    }

    /// \brief Template specialization for retrieving AccountType account information.
    template<>
    const AccountType BaseAccountInfoData::get_info<AccountType>(const AccountInfoRequest& request) const {
        return static_cast<AccountType>(get_info_int64(request));
    }

    /// \brief Template specialization for retrieving double account information.
    template<>
    const double BaseAccountInfoData::get_info<double>(const AccountInfoRequest& request) const {
        return get_info_f64(request);
    }

    /// \brief Template specialization for retrieving string account information.
    template<>
    const std::string BaseAccountInfoData::get_info<std::string>(const AccountInfoRequest& request) const {
        return get_info_str(request);
    }

}; // namespace optionx

#endif // _OPTIONX_BASE_ACCOUNT_INFO_DATA_HPP_INCLUDED
