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

        /// \brief Retrieves the API type associated with this account data.
        /// \return The type of API used.
        virtual const ApiType api_type() const {
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
        /// \param request Specifies the type of account information requested.
        /// \return Boolean account information.
        virtual bool get_account_info_bool(const AccountInfoRequest& request) = 0;

        /// \brief Retrieves integer account information based on the request type.
        /// \param request Specifies the type of account information requested.
        /// \return Integer account information.
        virtual int64_t get_account_info_int64(const AccountInfoRequest& request) = 0;

        /// \brief Retrieves floating-point account information based on the request type.
        /// \param request Specifies the type of account information requested.
        /// \return Floating-point account information.
        virtual double get_account_info_f64(const AccountInfoRequest& request) = 0;

        /// \brief Retrieves string account information based on the request type.
        /// \param request Specifies the type of account information requested.
        /// \return String account information.
        virtual std::string get_account_info_str(const AccountInfoRequest& request) = 0;
    };

    // Template specializations for retrieving specific account information types

    /// \brief Template specialization for retrieving integer account information.
    template<>
    const int IAccountInfoData::get_account_info<int>(const AccountInfoRequest& request) {
        return static_cast<int>(get_account_info_int64(request));
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

}; // namespace optionx

#endif // _OPTIONX_IACCOUNT_INFO_DATA_HPP_INCLUDED
