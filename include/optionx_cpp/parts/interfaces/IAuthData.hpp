#pragma once
#ifndef _OPTIONX_IAUTH_DATA_HPP_INCLUDED
#define _OPTIONX_IAUTH_DATA_HPP_INCLUDED

/// \file IAuthData.hpp
/// \brief Contains the IAuthData interface for handling authorization data and status.

#include "../utils/Enums.hpp"
#include <nlohmann/json.hpp>
#include <functional>
#include <memory>
#include <list>

namespace optionx {

    /// \class IAuthData
    /// \brief Interface class for handling authorization data for API connections.
    class IAuthData {
    public:
        using json = nlohmann::json;

        /// \typedef callback_t
        /// \brief Callback type for handling authorization success or failure.
        using callback_t = std::function<void(bool success, const std::string& message)>;

        /// \brief Serializes authorization data to a JSON object.
        /// \param j JSON object to hold serialized data.
        /// \return True if serialization was successful; false otherwise.
        virtual bool to_json(json &j) = 0;

        /// \brief Deserializes JSON data into authorization data.
        /// \param j JSON object containing authorization data.
        /// \return True if deserialization was successful; false otherwise.
        virtual bool from_json(json &j) = 0;

        /// \brief Validates the authorization data.
        /// \return True if authorization data is valid; false otherwise.
        virtual bool check() const = 0;

        /// \brief Clones the authorization data instance to a unique pointer.
        /// \return Unique pointer to a cloned IAuthData instance.
        virtual std::unique_ptr<IAuthData> clone_unique() const = 0;

        /// \brief Clones the authorization data instance to a shared pointer.
        /// \return Shared pointer to a cloned IAuthData instance.
        virtual std::shared_ptr<IAuthData> clone_shared() const = 0;

        /// \brief Retrieves the API type associated with this authorization data.
        /// \return The type of API used.
        virtual ApiType api_type() const = 0;

        /// \brief Adds a callback to handle authorization result.
        /// \param callback Function to handle success or failure of authorization.
        void add_callback(callback_t callback) {
            m_callbacks.push_back(std::move(callback));
        }

        /// \brief Executes all registered authorization callbacks with the specified result.
        /// \param success Boolean indicating success (true) or failure (false) of authorization.
        /// \param message Message providing details about the authorization result.
        void execute_callback(bool success, const std::string& message) const {
            for (const auto& callback : m_callbacks) {
                callback(success, message);
            }
        }

        virtual ~IAuthData() = default;

    private:
        std::list<callback_t> m_callbacks; ///< List of callbacks to notify authorization result.
    }; // IAuthData

}; // namespace optionx

#endif // _OPTIONX_IAUTH_DATA_HPP_INCLUDED
