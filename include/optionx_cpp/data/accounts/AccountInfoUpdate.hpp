#pragma once
#ifndef _OPTIONX_ACCOUNT_INFO_UPDATE_HPP_INCLUDED
#define _OPTIONX_ACCOUNT_INFO_UPDATE_HPP_INCLUDED

/// \file AccountInfoUpdate.hpp
/// \brief Defines the AccountInfoUpdate structure and account info update callback type.

namespace optionx {

    /// \struct AccountInfoUpdate
    /// \brief Represents an update to account information, including status changes and additional context.
    struct AccountInfoUpdate {
        std::shared_ptr<BaseAccountInfoData> account_info; ///< Updated account information.
        AccountUpdateStatus               status;       ///< Type of account update.
        std::string                       message;      ///< Additional context or error message.

        /// \brief Constructs an account update event.
        /// \param info Shared pointer to the updated account information.
        /// \param s The type of account update.
        /// \param msg Optional message providing additional details.
        AccountInfoUpdate(
            std::shared_ptr<BaseAccountInfoData> info,
            AccountUpdateStatus s,
            std::string msg = {})
            : account_info(std::move(info)), status(s), message(std::move(msg)) {}
    };

    /// \brief Callback type for handling account information updates.
    using account_info_callback_t = std::function<void(const AccountInfoUpdate&)>;

} // namespace optionx

#endif // _OPTIONX_ACCOUNT_INFO_UPDATE_HPP_INCLUDED
