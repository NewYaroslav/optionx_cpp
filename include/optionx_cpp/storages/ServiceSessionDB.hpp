#pragma once
#ifndef _OPTIONX_STORAGE_SERVICE_SESSION_DB_HPP_INCLUDED
#define _OPTIONX_STORAGE_SERVICE_SESSION_DB_HPP_INCLUDED

/// \file ServiceSessionDB.hpp
/// \brief Provides a singleton-based class for managing session data storage and retrieval.

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>

#include <mdbx_containers/KeyValueTable.hpp>

#if defined(_WIN32) || defined(_WIN64)

#   ifndef OPTIONX_DATA_PATH
#   define OPTIONX_DATA_PATH "data"
#   endif

#   ifndef OPTIONX_DB_PATH
#   define OPTIONX_DB_PATH OPTIONX_DATA_PATH "\\db"
#   endif

#   ifndef OPTIONX_SESSION_DB_FILE
#   define OPTIONX_SESSION_DB_FILE OPTIONX_DB_PATH "\\session_data"
#   endif

#else

#   ifndef OPTIONX_DATA_PATH
#   define OPTIONX_DATA_PATH "data"
#   endif

#   ifndef OPTIONX_DB_PATH
#   define OPTIONX_DB_PATH OPTIONX_DATA_PATH "/db"
#   endif

#   ifndef OPTIONX_SESSION_DB_FILE
#   define OPTIONX_SESSION_DB_FILE OPTIONX_DB_PATH "/session_data"
#   endif

#endif

namespace optionx::storage {

    /// \class ServiceSessionDB
    /// \brief Manages encrypted broker session data using an MDBX key-value table.
    class ServiceSessionDB {
    public:

        /// \brief Gets the singleton instance of ServiceSessionDB.
        /// \return Reference to the singleton instance.
        static ServiceSessionDB& get_instance() {
            static ServiceSessionDB instance;
            return instance;
        }

        /// \brief Creates default MDBX configuration for broker sessions.
        /// \return MDBX config using OPTIONX_SESSION_DB_FILE and the executable directory as the base path.
        static mdbxc::Config default_config() {
            mdbxc::Config config;
            config.pathname = OPTIONX_SESSION_DB_FILE;
            config.max_dbs = 1;
            config.no_subdir = false;
            config.relative_to_exe = true;
            return config;
        }

        /// \brief Constructs session storage with a custom MDBX configuration.
        /// \param config MDBX connection configuration.
        /// \param table_name Key-value table name for encrypted sessions.
        explicit ServiceSessionDB(
            mdbxc::Config config,
            std::string table_name = "sessions")
            : m_aes(crypto::AesMode::CBC_256) {
            initialize_default_key();
            open(std::move(config), std::move(table_name));
        }

        /// \brief Shuts down session storage.
        ~ServiceSessionDB() {
            shutdown();
        }

        /// \brief Sets the encryption key.
        /// \tparam T Type of the encryption key container (e.g., std::array, std::vector).
        /// \param key New encryption key.
        /// \return True if key is set successfully.
        template<class T>
        bool set_key(const T& key) {
            std::lock_guard<std::mutex> lock(m_mutex);
            const bool success = m_aes.set_key(key);
            if (success) {
                m_uses_default_key = is_default_key(key);
                m_default_key_warning_logged = false;
            }
            return success;
        }

        /// \brief Returns true while the built-in fallback encryption key is active.
        /// \details The default key is useful for tests and simple local apps, but
        /// should be treated as obfuscation rather than strong secret protection.
        bool uses_default_key() const {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_uses_default_key;
        }

        /// \brief Returns true after a caller-provided non-default key is installed.
        bool has_custom_key() const {
            return !uses_default_key();
        }

        /// \brief Checks whether the session database was opened successfully.
        /// \return True when the backing database is available.
        bool is_open() const {
            std::lock_guard<std::mutex> lock(m_mutex);
            return static_cast<bool>(m_db);
        }

        /// \brief Retrieves session value by platform and email.
        /// \param platform Platform name.
        /// \param email Email address.
        /// \return Session value as a string, or std::nullopt if not found.
        std::optional<std::string> get_session_value(const std::string& platform, const std::string& email) {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_db) {
                LOGIT_WARN("Session database is not open.");
                return std::nullopt;
            }
            std::string key = platform + ":" + email;
            try {
                std::string base64_encrypted_value;
                auto result = m_db->find(utils::Base64::encode(key));
                if (!result) return std::nullopt;
                base64_encrypted_value = *result;
                if (base64_encrypted_value.empty()) return std::nullopt;
                std::string value = m_aes.decrypt(utils::Base64::decode(base64_encrypted_value));
                if (value.empty()) return std::nullopt;
                return {value};
            } catch (const mdbxc::MdbxException& ex) {
                LOGIT_PRINT_ERROR("Database error: ", ex);
            } catch (const std::exception& ex) {
                LOGIT_PRINT_ERROR("General error: ", ex);
            };
            return std::nullopt;
        }

        /// \brief Stores session value for a specific platform and email.
        /// \param platform Platform name.
        /// \param email Email address.
        /// \param value Session value to store.
        /// \return True if session value is stored successfully, otherwise false.
        bool set_session_value(const std::string& platform, const std::string& email, const std::string& value) {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_db) {
                LOGIT_WARN("Session database is not open.");
                return false;
            }
            std::string key = platform + ":" + email;
            try {
                warn_default_key_once();
                m_db->insert_or_assign(utils::Base64::encode(key), utils::Base64::encode(m_aes.encrypt(value)));
                return true;
            } catch(const mdbxc::MdbxException& ex){
                LOGIT_PRINT_ERROR("Database error: ", ex);
            } catch(const std::exception& ex){
                LOGIT_PRINT_ERROR("General error: ", ex);
            }
            return false;
        }

        /// \brief Removes session value for a specific platform and email.
        /// \param platform Platform name.
        /// \param email Email address.
        /// \return True if session value is removed successfully, otherwise false.
        bool remove_session(const std::string& platform, const std::string& email) {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_db) {
                LOGIT_WARN("Session database is not open.");
                return false;
            }
            std::string key = platform + ":" + email;
            try {
                m_db->erase(utils::Base64::encode(key));
                return true;
            } catch(const mdbxc::MdbxException& ex){
                LOGIT_PRINT_ERROR("Database error: ", ex);
            } catch(const std::exception& ex){
                LOGIT_PRINT_ERROR("General error: ", ex);
            }
            return false;
        }

        /// \brief Clears all session data from the database.
        /// \return True if database is cleared successfully, otherwise false.
        bool clear() {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_db) {
                LOGIT_WARN("Session database is not open.");
                return false;
            }
            try {
                m_db->clear();
                return true;
            } catch(const mdbxc::MdbxException& ex){
                LOGIT_PRINT_ERROR("Database error: ", ex);
            } catch(const std::exception& ex){
                LOGIT_PRINT_ERROR("General error: ", ex);
            }
            return false;
        }

        /// \brief Shuts down session service.
        /// \details Disconnects database and clears encryption key.
        void shutdown() {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_db) {
                m_db->disconnect();
                m_db.reset();
            }
            m_aes.clear_key();
            m_uses_default_key = true;
            m_default_key_warning_logged = false;
        }

    private:
        mutable std::mutex m_mutex; ///< Mutex for thread safety.
        crypto::AESCrypt m_aes;   ///< AES encryption and decryption instance.
        std::unique_ptr<mdbxc::KeyValueTable<std::string, std::string>> m_db; ///< MDBX-backed session key-value table.
        bool m_uses_default_key = true; ///< True while the built-in fallback key is active.
        bool m_default_key_warning_logged = false; ///< Suppresses repeated default-key warnings.

        /// \brief Private constructor for singleton pattern.
        ServiceSessionDB() : ServiceSessionDB(default_config()) {}

        inline static constexpr std::array<std::uint8_t, 32> kDefaultKey = {{
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
            0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
            0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
            0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F
        }};

        void initialize_default_key() {
            m_aes.set_key(kDefaultKey);
            m_uses_default_key = true;
            m_default_key_warning_logged = false;
        }

        template<class T>
        static bool is_default_key(const T& key) {
            if (key.size() != kDefaultKey.size()) {
                return false;
            }
            return std::equal(kDefaultKey.begin(), kDefaultKey.end(), key.begin());
        }

        void warn_default_key_once() {
            if (!m_uses_default_key || m_default_key_warning_logged) {
                return;
            }
            LOGIT_WARN(
                "ServiceSessionDB is storing a broker session with the built-in default encryption key. "
                "Set a custom key before storing production sessions.");
            m_default_key_warning_logged = true;
        }

        void open(mdbxc::Config config, const std::string& table_name) {
            try {
                if (config.max_dbs < 1) {
                    config.max_dbs = 1;
                }
                m_db = std::make_unique<mdbxc::KeyValueTable<std::string, std::string>>(config, table_name);
            } catch(const mdbxc::MdbxException& ex){
                LOGIT_PRINT_ERROR("Database connection error: ", ex);
            } catch(const std::exception& ex){
                LOGIT_PRINT_ERROR("General error: ", ex);
            }
        }

        ServiceSessionDB(const ServiceSessionDB&) = delete;
        ServiceSessionDB& operator=(const ServiceSessionDB&) = delete;
    };

} // namespace optionx::storage

#endif // _OPTIONX_STORAGE_SERVICE_SESSION_DB_HPP_INCLUDED
