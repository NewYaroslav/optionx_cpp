#pragma once
#ifndef _OPTIONX_BRIDGE_ENUMS_HPP_INCLUDED
#define _OPTIONX_BRIDGE_ENUMS_HPP_INCLUDED

/// \file enums.hpp
/// \brief

namespace optionx {

    /// \enum BridgeStatus
    /// \brief Represents the type of bridge status update or state change.
    enum class BridgeStatus {
        UNKNOWN = 0,          ///< Unknown status.
        SERVER_STARTED,       ///< Bridge server started.
        SERVER_STOPPED,       ///< Bridge server stopped.
        SERVER_START_FAILED,  ///< Bridge server failed to start.
        CLIENT_CONNECTED,     ///< Client connected to the bridge.
        CLIENT_DISCONNECTED,  ///< Client disconnected from the bridge.
        CONNECTION_ERROR      ///< Error in client connection.
    };

        /// \brief Converts BridgeStatus to its string representation.
    /// \param value The BridgeStatus enumeration value.
    /// \return Constant reference to the corresponding string.
	inline const std::string &to_str(BridgeStatus value) noexcept {
        static const std::vector<std::string> str_data = {
            "UNKNOWN",
            "SERVER_STARTED",
            "SERVER_STOPPED",
            "SERVER_START_FAILED",
            "CLIENT_CONNECTED",
            "CLIENT_DISCONNECTED",
            "CONNECTION_ERROR"};
		return str_data[static_cast<size_t>(value)];
    };

	/// \brief Converts string to BridgeStatus enumeration.
    /// \param str Input string to convert.
    /// \param value Output enumeration value.
    /// \return True if conversion succeeded.
	inline const bool to_enum(const std::string &str, BridgeStatus &value) noexcept {
        static const std::unordered_map<std::string, BridgeStatus> str_data = {
            {"UNKNOWN",             BridgeStatus::UNKNOWN            },
            {"SERVER_STARTED",      BridgeStatus::SERVER_STARTED     },
            {"SERVER_STOPPED",      BridgeStatus::SERVER_STOPPED     },
            {"SERVER_START_FAILED", BridgeStatus::SERVER_START_FAILED},
            {"CLIENT_CONNECTED",    BridgeStatus::CLIENT_CONNECTED   },
            {"CLIENT_DISCONNECTED", BridgeStatus::CLIENT_DISCONNECTED},
            {"CONNECTION_ERROR",    BridgeStatus::CONNECTION_ERROR   }
        };
        auto it = str_data.find(utils::to_upper_case(str));
        if (it != str_data.end()) {
            value = it->second;
            return true;
        }
        return false;
    }

	/// \brief Template specialization for BridgeStatus enum conversion.
	template <>
    inline BridgeStatus to_enum<BridgeStatus>(const std::string &str) {
        BridgeStatus value;
        if (!to_enum(str, value)) {
            throw std::invalid_argument("Invalid BridgeStatus string: " + str);
        }
		return value;
    }

	/// \brief Converts BridgeStatus to JSON.
    inline void to_json(nlohmann::json& j, const BridgeStatus& type) {
        j = optionx::to_str(type);
    }

    /// \brief Converts JSON to BridgeStatus.
    inline void from_json(const nlohmann::json& j, BridgeStatus& type) {
        type = optionx::to_enum<BridgeStatus>(j.get<std::string>());
    }

	/// \brief Stream output operator for BridgeStatus.
	std::ostream& operator<<(std::ostream& os, BridgeStatus value) {
        os << optionx::to_str(value);
        return os;
    }

} // namespace optionx

#endif // _OPTIONX_BRIDGE_ENUMS_HPP_INCLUDED
