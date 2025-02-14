#pragma once
#ifndef _OPTIONX_BRIDGE_STATUS_UPDATE_HPP_INCLUDED
#define _OPTIONX_BRIDGE_STATUS_UPDATE_HPP_INCLUDED

/// \file BridgeStatusUpdate.hpp
/// \brief Defines the BridgeStatusUpdate structure and bridge status update callback type.

namespace optionx {

    /// \struct BridgeStatusUpdate
    /// \brief Represents an update to bridge status, including state changes and additional context.
    struct BridgeStatusUpdate {
        BridgeStatus status;                  ///< Type of bridge status update.
        std::string  connection_id;           ///< Identifier for the client connection.
        std::string  message;                 ///< Additional context or error message.

        /// \brief Constructs a bridge status update event.
        /// \param s The type of bridge status update.
        /// \param conn_id Identifier for the client connection.
        /// \param msg Optional message providing additional details.
        BridgeStatusUpdate(
            BridgeStatus s,
            std::string conn_id = {},
            std::string msg = {})
            : status(s), connection_id(std::move(conn_id)), message(std::move(msg)) {}
    };

    /// \brief Callback type for handling bridge status updates.
    using bridge_status_callback_t = std::function<void(const BridgeStatusUpdate&)>;

} // namespace optionx

#endif // _OPTIONX_BRIDGE_STATUS_UPDATE_HPP_INCLUDED
