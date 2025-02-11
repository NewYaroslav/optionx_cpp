#pragma once
#ifndef _OPTIONX_IAUTH_DATA_HPP_INCLUDED
#define _OPTIONX_IAUTH_DATA_HPP_INCLUDED

/// \file IAuthData.hpp
/// \brief Defines the IAuthData interface for handling authorization data and authentication status.

#include <nlohmann/json.hpp>
#include <functional>
#include <list>

namespace optionx {

    /// \class IAuthData
    /// \brief Interface for handling authorization data used in API connections.
    class IAuthData {
    public:

        /// \typedef callback_t
        /// \brief Callback type for handling authorization success or failure.
        ///
        /// The callback receives:
        /// - `success` - `true` if authentication was successful, otherwise `false`.
        /// - `message` - Additional message describing the authentication result.
        using callback_t = std::function<void(bool success, const std::string& message)>;

        /// \brief Serializes authorization data into a JSON object.
        /// \param j JSON object to hold serialized data.
        virtual void to_json(nlohmann::json& j) const = 0;

        /// \brief Deserializes authorization data from a JSON object.
        /// \param j JSON object containing authorization data.
        virtual void from_json(const nlohmann::json& j) = 0;

        /// \brief Validates the authorization data.
        /// \return `true` if the authorization data is valid; otherwise, `false`.
        virtual bool check() const = 0;

        /// \brief Clones the authorization data instance as a unique pointer.
        /// \return A `std::unique_ptr<IAuthData>` pointing to the cloned instance.
        virtual std::unique_ptr<IAuthData> clone_unique() const = 0;

        /// \brief Clones the authorization data instance as a shared pointer.
        /// \return A `std::shared_ptr<IAuthData>` pointing to the cloned instance.
        virtual std::shared_ptr<IAuthData> clone_shared() const = 0;

        /// \brief Retrieves the API type associated with this authorization data.
        /// \return The `PlatformType` associated with this authentication data.
        virtual PlatformType platform_type() const = 0;

        /// \brief Registers a callback to handle authorization results.
        /// \param callback A function to be called with the authentication result.
        void add_callback(callback_t callback) {
            m_callbacks.push_back(std::move(callback));
        }

        /// \brief Dispatches all registered authorization callbacks with the authentication result.
        /// \param success `true` if authentication succeeded, otherwise `false`.
        /// \param message A message providing additional details about the authentication outcome.
        void dispatch_callbacks(bool success, const std::string& message) const {
            for (const auto& callback : m_callbacks) {
                callback(success, message);
            }
        }

        /// \brief Virtual destructor for `IAuthData`.
        virtual ~IAuthData() = default;

    private:
        std::list<callback_t> m_callbacks; ///< List of callbacks to notify about authentication results.
    }; // class IAuthData

} // namespace optionx

#endif // _OPTIONX_IAUTH_DATA_HPP_INCLUDED
