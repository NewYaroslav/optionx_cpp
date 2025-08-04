#pragma once
#ifndef _OPTIONX_STORAGE_SERVICE_SESSION_DB_HPP_INCLUDED
#define _OPTIONX_STORAGE_SERVICE_SESSION_DB_HPP_INCLUDED

/// \file ServiceSessionDB.hpp
/// \brief Provides a singleton-based class for managing session data storage and retrieval.

#include <mdbx_containers/KeyValueTable.hpp>
#include <optional>
#include <memory>
#include <string>
#include <mutex>

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
    /// \brief Manages session data storage and retrieval using an SQLite database.
    class ServiceSessionDB {
    public:

        /// \brief Gets the singleton instance of ServiceSessionDB.
        /// \return Reference to the singleton instance.
        static ServiceSessionDB& get_instance() {
            static ServiceSessionDB instance;
            return instance;
        }

        /// \brief Sets the encryption key.
        /// \tparam T Type of the encryption key container (e.g., std::array, std::vector).
        /// \param key New encryption key.
        /// \return True if key is set successfully.
        template<class T>
        bool set_key(const T& key) {
            return m_aes.set_key(key);
        }

        /// \brief Retrieves session value by platform and email.
        /// \param platform Platform name.
        /// \param email Email address.
        /// \return Session value as a string, or std::nullopt if not found.
        std::optional<std::string> get_session_value(const std::string& platform, const std::string& email) {
            std::lock_guard<std::mutex> lock(m_mutex);
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
            std::string key = platform + ":" + email;
            try {
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
            m_db->disconnect();
            m_aes.clear_key();
        }

    private:
        std::mutex       m_mutex; ///< Mutex for thread safety.
        crypto::AESCrypt m_aes;   ///< AES encryption and decryption instance.
        std::unique_ptr<mdbxc::KeyValueTable<std::string, std::string>> m_db; ///< The SQLite KeyValueDB instance.

        /// \brief Private constructor for singleton pattern.
        ServiceSessionDB() : m_aes(crypto::AesMode::CBC_256) {
            std::array<uint8_t, 32> def_key = {
                0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
                0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
                0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F
            };
            m_aes.set_key(def_key);
            try {
                mdbxc::Config config;
                config.pathname = OPTIONX_SESSION_DB_FILE;
                config.max_dbs = 1;
                config.no_subdir = false;
                config.relative_to_exe = true;
                m_db = std::make_unique<mdbxc::KeyValueTable<std::string, std::string>>(config, "sessions");
            } catch(const mdbxc::MdbxException& ex){
                LOGIT_PRINT_ERROR("Database connection error: ", ex);
            } catch(const std::exception& ex){
                LOGIT_PRINT_ERROR("General error: ", ex);
            }
        }

        ~ServiceSessionDB() {
            shutdown();
        }

        ServiceSessionDB(const ServiceSessionDB&) = delete;
        ServiceSessionDB& operator=(const ServiceSessionDB&) = delete;
    };

} // namespace optionx::storage

#endif // _OPTIONX_STORAGE_SERVICE_SESSION_DB_HPP_INCLUDED
