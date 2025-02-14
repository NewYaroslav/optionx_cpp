#pragma once
#ifndef _OPTIONX_IBRIDGE_CONFIG_HPP_INCLUDED
#define _OPTIONX_IBRIDGE_CONFIG_HPP_INCLUDED

/// \file IBridgeConfig.hpp
/// \brief Defines the IBridgeConfig interface for handling bridge configuration data.

namespace optionx {

    /// \class IBridgeConfig
    /// \brief Interface for bridge configuration data management.
    class IBridgeConfig {
    public:
        virtual ~IBridgeConfig() = default;

        /// \typedef callback_t
        /// \brief Callback type for handling configuration success or failure.
        ///
        /// The callback receives:
        /// - `success` - `true` if configuration was successful, otherwise `false`.
        /// - `message` - Additional message describing the configuration result.
        using callback_t = std::function<void(bool success, const std::string& message)>;

        /// \brief Serializes configuration data into a JSON object.
        /// \param j JSON object to hold serialized data.
        virtual void to_json(nlohmann::json& j) const = 0;

        /// \brief Deserializes configuration data from a JSON object.
        /// \param j JSON object containing configuration data.
        virtual void from_json(const nlohmann::json& j) = 0;

        /// \brief Validates the configuration data with detailed error message.
        /// \return A pair where the first element is true if data is valid, and the second element contains an error message in case of failure.
        virtual std::pair<bool, std::string> validate() const = 0;

        /// \brief Clones the configuration data instance as a unique pointer.
        /// \return A `std::unique_ptr<IBridgeConfig>` pointing to the cloned instance.
        virtual std::unique_ptr<IBridgeConfig> clone_unique() const = 0;

        /// \brief Clones the configuration data instance as a shared pointer.
        /// \return A `std::shared_ptr<IBridgeConfig>` pointing to the cloned instance.
        virtual std::shared_ptr<IBridgeConfig> clone_shared() const = 0;

        /// \brief Retrieves the bridge type associated with this configuration data.
        /// \return The `BridgeType` associated with this configuration data.
        virtual BridgeType bridge_type() const = 0;

        /// \brief Registers a callback to handle configuration results.
        /// \param callback A function to be called with the configuration result.
        void add_callback(callback_t callback) {
            m_callbacks.push_back(std::move(callback));
        }

        /// \brief Dispatches all registered configuration callbacks with the result.
        /// \param success `true` if configuration succeeded, otherwise `false`.
        /// \param message A message providing additional details about the configuration outcome.
        void dispatch_callbacks(bool success, const std::string& message) const {
            for (const auto& callback : m_callbacks) {
                callback(success, message);
            }
        }

    private:
        std::list<callback_t> m_callbacks; ///< List of callbacks to notify about configuration results.
    };

} // namespace optionx

#endif // _OPTIONX_IBRIDGE_CONFIG_HPP_INCLUDED
