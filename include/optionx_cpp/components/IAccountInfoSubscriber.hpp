#pragma once
#ifndef OPTIONX_HEADER_COMPONENTS_IACCOUNT_INFO_SUBSCRIBER_HPP_INCLUDED
#define OPTIONX_HEADER_COMPONENTS_IACCOUNT_INFO_SUBSCRIBER_HPP_INCLUDED

/// \file IAccountInfoSubscriber.hpp
/// \brief Defines the object interface used by account-info fan-out helpers.

namespace optionx::components {

    /// \class IAccountInfoSubscriber
    /// \brief Interface for consumers that receive account information updates.
    /// \details AccountInfoUpdate::status tells which account aspect changed,
    ///          while AccountInfoUpdate::account_info carries the current
    ///          account snapshot that can be queried for the new value.
    class IAccountInfoSubscriber {
    public:
        /// \brief Virtual destructor.
        virtual ~IAccountInfoSubscriber() = default;

        /// \brief Handles an account information update.
        /// \param update Account update payload.
        virtual void on_account_info(const AccountInfoUpdate& update) = 0;
    };

} // namespace optionx::components

#endif // OPTIONX_HEADER_COMPONENTS_IACCOUNT_INFO_SUBSCRIBER_HPP_INCLUDED
