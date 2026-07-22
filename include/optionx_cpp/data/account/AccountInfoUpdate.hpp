#pragma once
#ifndef OPTIONX_HEADER_DATA_ACCOUNT_ACCOUNT_INFO_UPDATE_HPP_INCLUDED
#define OPTIONX_HEADER_DATA_ACCOUNT_ACCOUNT_INFO_UPDATE_HPP_INCLUDED

/// \file AccountInfoUpdate.hpp
/// \brief Defines the AccountInfoUpdate structure and account info update callback type.

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

namespace optionx {

    /// \struct AccountInfoUpdate
    /// \brief Represents an update to account information, including status changes and additional context.
    struct AccountInfoUpdate {
        std::shared_ptr<BaseAccountInfoData> account_info; ///< Updated broker/platform account information.
        AccountUpdateStatus status; ///< Type of account update.
        std::string message; ///< Additional context or error message.
        std::int64_t account_id = 0; ///< Internal OptionX account ID; 0 means unspecified.

        /// \brief Constructs an account update event.
        /// \param info Shared pointer to the updated account information.
        /// \param s The type of account update.
        /// \param msg Optional message providing additional details.
        AccountInfoUpdate(
            std::shared_ptr<BaseAccountInfoData> info,
            AccountUpdateStatus s,
            std::string msg = {},
            std::int64_t internal_account_id = 0)
            : account_info(std::move(info)),
              status(s),
              message(std::move(msg)),
              account_id(internal_account_id) {}

        /// \brief Constructs an account update event with an internal account ID.
        /// \param info Shared pointer to the updated account information.
        /// \param s The type of account update.
        /// \param internal_account_id Stable OptionX account ID.
        /// \param msg Optional message providing additional details.
        AccountInfoUpdate(
            std::shared_ptr<BaseAccountInfoData> info,
            AccountUpdateStatus s,
            std::int64_t internal_account_id,
            std::string msg = {})
            : AccountInfoUpdate(
                  std::move(info),
                  s,
                  std::move(msg),
                  internal_account_id) {}
    };

    /// \brief Callback type for handling account information updates.
    using account_info_callback_t = std::function<void(const AccountInfoUpdate&)>;

} // namespace optionx

#endif // OPTIONX_HEADER_DATA_ACCOUNT_ACCOUNT_INFO_UPDATE_HPP_INCLUDED
