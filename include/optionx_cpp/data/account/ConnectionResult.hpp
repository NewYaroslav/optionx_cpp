#pragma once
#ifndef _OPTIONX_CONNECTION_RESULT_HPP_INCLUDED
#define _OPTIONX_CONNECTION_RESULT_HPP_INCLUDED

/// \file ConnectionResult.hpp
/// \brief

namespace optionx {

    /// \struct ConnectionResult
    /// \brief Encapsulates the result of a connection or disconnection attempt.
    struct ConnectionResult {
        bool success;                         ///< Indicates whether the connection/disconnection was successful.
        std::string reason;                   ///< Provides additional context or an error message.
        std::unique_ptr<IAuthData> auth_data; ///< Authorization data used for the connection.

        /// \brief Constructor to initialize all fields.
        ConnectionResult(
            bool s,
            std::string r,
            std::unique_ptr<IAuthData> data = nullptr)
            : success(s), reason(std::move(r)), auth_data(std::move(data)) {}
    };

    /// \brief Callback type for connection-related events.
    using connection_callback_t = std::function<void(const ConnectionResult&)>;

} // namespace optionx

#endif // _OPTIONX_CONNECTION_RESULT_HPP_INCLUDED
