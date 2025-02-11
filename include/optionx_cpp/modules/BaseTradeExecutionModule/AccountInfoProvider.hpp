#pragma once
#ifndef _OPTIONX_MODULES_ACCOUNT_INFO_PROVIDER_HPP_INCLUDED
#define _OPTIONX_MODULES_ACCOUNT_INFO_PROVIDER_HPP_INCLUDED

/// \file AccountInfoProvider.hpp
/// \brief Defines the AccountInfoProvider class for retrieving account-related information.

namespace optionx::modules {

    /// \class AccountInfoProvider
    /// \brief Provides a unified interface for accessing various account-related information.
    ///
    /// This class acts as an abstraction layer over `BaseAccountInfoData`, offering methods
    /// to retrieve balances, trading limits, response timeouts, and other platform-specific details.
    class AccountInfoProvider {
    public:

        /// \brief Constructs an `AccountInfoProvider` instance with a shared reference to `BaseAccountInfoData`.
        /// \param account_info A shared pointer to a `BaseAccountInfoData` instance.
        explicit AccountInfoProvider(std::shared_ptr<BaseAccountInfoData> account_info)
            : m_account_info(std::move(account_info)) {}

        /// \brief Retrieves account information based on a detailed request.
        /// \tparam T The expected type of the returned value.
        /// \param request The request containing specific parameters for data retrieval.
        /// \return The requested account information of type `T`.
        template<class T>
        inline T get_info(const AccountInfoRequest& request) const;

        /// \brief Retrieves account information by `AccountInfoType`.
        /// \tparam T The expected type of the returned value.
        /// \param type The specific type of account information to retrieve.
        /// \param timestamp Optional timestamp for retrieving historical values (default: 0).
        /// \return The requested account information of type `T`.
        template<class T>
        inline T get_info(AccountInfoType type, int64_t timestamp = 0) const;

        /// \brief Checks the availability of a trading symbol.
        /// \tparam T The expected type of the returned value.
        /// \param symbol The trading symbol (e.g., currency pair, stock ticker).
        /// \param timestamp Optional timestamp for retrieving historical values (default: 0).
        /// \return The availability status of the symbol as type `T`.
        template<class T>
        inline T get_by_symbol(const std::string &symbol, int64_t timestamp = 0) const;

        /// \brief Checks if a specific option type is available.
        /// \tparam T The expected type of the returned value.
        /// \param option The option type (e.g., Classic, Sprint).
        /// \param timestamp Optional timestamp for retrieving historical values (default: 0).
        /// \return The availability status of the option type as type `T`.
        template<class T>
        inline T get_by_option(OptionType option, int64_t timestamp = 0) const;

        /// \brief Checks if a specific order type is available.
        /// \tparam T The expected type of the returned value.
        /// \param order The order type (e.g., Market, Limit, Stop).
        /// \param timestamp Optional timestamp for retrieving historical values (default: 0).
        /// \return The availability status of the order type as type `T`.
        template<class T>
        inline T get_by_order(OrderType order, int64_t timestamp = 0) const;

        /// \brief Checks if a specific account type is available.
        /// \tparam T The expected type of the returned value.
        /// \param account The account type (e.g., Demo, Real).
        /// \param timestamp Optional timestamp for retrieving historical values (default: 0).
        /// \return The availability status of the account type as type `T`.
        template<class T>
        inline T get_by_account(AccountType account, int64_t timestamp = 0) const;

        /// \brief Checks if a specific currency type is available.
        /// \tparam T The expected type of the returned value.
        /// \param currency The currency type (e.g., USD, EUR).
        /// \param timestamp Optional timestamp for retrieving historical values (default: 0).
        /// \return The availability status of the currency type as type `T`.
        template<class T>
        inline T get_by_currency(CurrencyType currency, int64_t timestamp = 0) const;

        /// \brief Retrieves account information related to a specific trade request.
        /// \tparam T The expected type of the returned value.
        /// \param info_type The type of account information to retrieve.
        /// \param trade_request A shared pointer to a trade request instance.
        /// \param timestamp Optional timestamp for retrieving historical values (default: 0).
        /// \return The requested trade-related account information of type `T`.
        template<class T>
        inline T get_for_trade(
            AccountInfoType info_type,
            const std::shared_ptr<TradeRequest>& trade_request,
            int64_t timestamp = 0) const;

        /// \brief Retrieves the maximum response timeout for trade operations.
        /// \return The response timeout in milliseconds.
        int64_t get_response_timeout() const {
            return time_shield::sec_to_ms(get_info<int64_t>(AccountInfoType::RESPONSE_TIMEOUT));
        }

    private:
        std::shared_ptr<BaseAccountInfoData> m_account_info; ///< Shared reference to account data provider.
    };

    // Implementation of inline methods

    template<class T>
    inline T AccountInfoProvider::get_info(const AccountInfoRequest& request) const {
        return m_account_info->get_info<T>(request);
    }

    template<class T>
    inline T AccountInfoProvider::get_info(AccountInfoType type, int64_t timestamp) const {
        return m_account_info->get_info<T>(type, timestamp);
    }

    template<class T>
    inline T AccountInfoProvider::get_by_symbol(const std::string &symbol, int64_t timestamp) const {
        return m_account_info->get_by_symbol<T>(symbol, timestamp);
    }

    template<class T>
    inline T AccountInfoProvider::get_by_option(OptionType option, int64_t timestamp) const {
        return m_account_info->get_by_option<T>(option, timestamp);
    }

    template<class T>
    inline T AccountInfoProvider::get_by_order(OrderType order, int64_t timestamp) const {
        return m_account_info->get_by_order<T>(order, timestamp);
    }

    template<class T>
    inline T AccountInfoProvider::get_by_account(AccountType account, int64_t timestamp) const {
        return m_account_info->get_by_account<T>(account, timestamp);
    }

    template<class T>
    inline T AccountInfoProvider::get_by_currency(CurrencyType currency, int64_t timestamp) const {
        return m_account_info->get_by_currency<T>(currency, timestamp);
    }

    template<class T>
    inline T AccountInfoProvider::get_for_trade(
            AccountInfoType info_type,
            const std::shared_ptr<TradeRequest>& trade_request,
            int64_t timestamp) const {
        return m_account_info->get_for_trade<T>(info_type, trade_request, timestamp);
    }

} // namespace optionx::modules

#endif // _OPTIONX_MODULES_ACCOUNT_INFO_PROVIDER_HPP_INCLUDED
