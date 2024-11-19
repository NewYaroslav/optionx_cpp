#pragma once
#ifndef _OPTIONX_PLATFORMS_SERVICE_SESSION_DB_HPP_INCLUDED
#define _OPTIONX_PLATFORMS_SERVICE_SESSION_DB_HPP_INCLUDED

/// \file ServiceSessionDB.hpp
/// \brief Provides a singleton-based class for managing session data storage and retrieval.

#include <sqlite_containers/KeyValueDB.hpp>
#include "../crypto/AESCrypt.hpp"
#include <optional>
#include <memory>
#include <string>
#include <mutex>
#include "aes.hpp" // Подключение AES библиотеки

namespace optionx {
namespace platforms {

class ServiceSessionDB final {
public:
    /// \brief Gets the singleton instance of ServiceSessionDB.
    /// \return Reference to the singleton instance.
    static ServiceSessionDB& get_instance() {
        static ServiceSessionDB instance;
        return instance;
    }

    /// \brief Sets the encryption key.
    /// \param key A new key for encryption.
    /// \return True if the key length matches the expected length for the current mode, otherwise false.
    template<class T>
    bool set_key(const T& key) {
        m_secure.set_key(key);
        return true;
    }

    /// \brief Retrieves a session value by email and platform.
    /// \param platform The platform name.
    /// \param email The email address.
    /// \return The session value as a string, or std::nullopt if not found.
    std::optional<std::string> get_session_value(const std::string& platform, const std::string& email) {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::string key = platform + ":" + email;
        try {
            std::string encrypted_key = m_aes.encrypt(key);
            std::string encrypted_value;
            if (!m_db.find(encrypted_key, encrypted_value)) return std::nullopt;
            if (encrypted_value.empty()) return std::nullopt;
            std::string value = m_aes.decrypt(encrypted_value);
            if (value.empty()) return std::nullopt;
            return {value};
        } catch(...) {}
        return std::nullopt;
    }

    /// \brief Sets a session value for a specific platform and email.
    /// \param platform The platform name.
    /// \param email The email address.
    /// \param value The session value to store.
    bool set_session_value(const std::string& platform, const std::string& email, const std::string& value) {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::string key = platform + ":" + email;
        try {
            std::string encrypted_key = m_aes.encrypt(key);
            std::string encrypted_value = m_aes.encrypt(value);
            m_db.insert(encrypted_key, encrypted_value);
        }
    }

private:

    ServiceSessionDB() : m_aes(crypto::AesMode::CBC_256) {
        std::array<uint8_t, 32> def_key = {
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
            0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
            0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
            0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F
        };
        m_aes.set_key(def_key);
    }

    ~ServiceSessionDB() = default;

    ServiceSessionDB(const ServiceSessionDB&) = delete;
    ServiceSessionDB& operator=(const ServiceSessionDB&) = delete;

    /// \brief The SQLite KeyValueDB instance.
    sqlite_containers::KeyValueDB<std::string, std::string> m_db; ///< The SQLite KeyValueDB instance.
    std::mutex  m_mutex; ///<  Mutex for thread safety.

    crypto::AESCrypt m_aes;
};

} // namespace platforms
} // namespace optionx

#endif // _OPTIONX_PLATFORMS_SERVICE_SESSION_DB_HPP_INCLUDED
