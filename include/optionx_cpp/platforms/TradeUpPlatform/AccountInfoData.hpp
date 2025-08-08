#pragma once
#ifndef _OPTIONX_PLATFORMS_TRADEUP_ACCOUNT_INFO_DATA_HPP_INCLUDED
#define _OPTIONX_PLATFORMS_TRADEUP_ACCOUNT_INFO_DATA_HPP_INCLUDED

/// \file AccountInfoData.hpp
/// \brief Contains the AccountInfoData class for TradeUp platform account information.

#include "optionx_cpp/data/account/BaseAccountInfoData.hpp"
#include "optionx_cpp/data/trading/enums.hpp"

namespace optionx::platforms::tradeup {

    /// \class AccountInfoData
    /// \brief Account information data for TradeUp platform.
    class AccountInfoData : public BaseAccountInfoData {
    public:
        std::string   user_id;                      ///< User identifier
        double        balance      = 0.0;           ///< Account balance
        CurrencyType  currency     = CurrencyType::UNKNOWN; ///< Account currency
        AccountType   account_type = AccountType::UNKNOWN;  ///< Account type
        bool          connect      = false;         ///< Connection status flag

        const PlatformType platform_type() const override { return PlatformType::TRADEUP; }

        std::unique_ptr<BaseAccountInfoData> clone_unique() const override {
            return std::make_unique<AccountInfoData>(*this);
        }

        std::shared_ptr<BaseAccountInfoData> clone_shared() const override {
            return std::make_shared<AccountInfoData>(*this);
        }

    protected:
        bool get_info_bool(const AccountInfoRequest& request) const override {
            if (request.type == AccountInfoType::CONNECTION_STATUS) return connect;
            return false;
        }

        int64_t get_info_int64(const AccountInfoRequest& request) const override {
            switch (request.type) {
                case AccountInfoType::BALANCE: return static_cast<int64_t>(balance);
                case AccountInfoType::PLATFORM_TYPE: return static_cast<int64_t>(platform_type());
                case AccountInfoType::ACCOUNT_TYPE: return static_cast<int64_t>(account_type);
                case AccountInfoType::CURRENCY: return static_cast<int64_t>(currency);
                default: break;
            }
            return 0;
        }

        double get_info_f64(const AccountInfoRequest& request) const override {
            if (request.type == AccountInfoType::BALANCE) return balance;
            return 0.0;
        }

        std::string get_info_str(const AccountInfoRequest& request) const override {
            switch (request.type) {
                case AccountInfoType::USER_ID: return user_id;
                case AccountInfoType::BALANCE: return std::to_string(balance);
                case AccountInfoType::PLATFORM_TYPE: return to_str(platform_type());
                case AccountInfoType::ACCOUNT_TYPE: return to_str(account_type);
                case AccountInfoType::CURRENCY: return to_str(currency);
                default: break;
            }
            return {};
        }
    };

} // namespace optionx::platforms::tradeup

#endif // _OPTIONX_PLATFORMS_TRADEUP_ACCOUNT_INFO_DATA_HPP_INCLUDED
