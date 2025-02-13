#pragma once
#ifndef _OPTIONX_STORAGE_SERVICE_SESSION_DB_HPP_INCLUDED
#define _OPTIONX_STORAGE_SERVICE_SESSION_DB_HPP_INCLUDED

/// \file ServiceSessionDB.hpp
/// \brief Provides a singleton-based class for managing session data storage and retrieval.

#include <sqlite_containers/KeyValueDB.hpp>
#include <optional>
#include <memory>
#include <string>
#include <mutex>

#if defined(_WIN32) || defined(_WIN64)

#   ifndef OPTIONX_DATA_PATH
#   define OPTIONX_DATA_PATH "data"
#   endif

#   ifndef OPTIONX_DB_PATH
#   define OPTIONX_DB_PATH OPTIONX_DATA_PATH "\\databases"
#   endif

#   ifndef OPTIONX_SESSION_DB_FILE
#   define OPTIONX_SESSION_DB_FILE OPTIONX_DB_PATH "\\session_data.db"
#   endif

#else

#   ifndef OPTIONX_DATA_PATH
#   define OPTIONX_DATA_PATH "data"
#   endif

#   ifndef OPTIONX_DB_PATH
#   define OPTIONX_DB_PATH OPTIONX_DATA_PATH "/databases"
#   endif

#   ifndef OPTIONX_SESSION_DB_FILE
#   define OPTIONX_SESSION_DB_FILE OPTIONX_DB_PATH "/session_data.db"
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
        /// \param key A new key for encryption.
        /// \return True if the key is set successfully.
        template<class T>
        bool set_key(const T& key) {
            return m_aes.set_key(key);
        }

        /// \brief Retrieves a session value by platform and email.
        /// \param platform The platform name.
        /// \param email The email address.
        /// \return The session value as a string, or std::nullopt if not found.
        std::optional<std::string> get_session_value(const std::string& platform, const std::string& email) {
            std::lock_guard<std::mutex> lock(m_mutex);
            std::string key = platform + ":" + email;
            try {
                std::string base64_encrypted_value;
                if (!m_db.find(utils::Base64::encode(key), base64_encrypted_value)) return std::nullopt;
                if (base64_encrypted_value.empty()) return std::nullopt;
                std::string value = m_aes.decrypt(utils::Base64::decode(base64_encrypted_value));
                if (value.empty()) return std::nullopt;
                return {value};
            } catch (const sqlite_containers::sqlite_exception& ex) {
                LOGIT_PRINT_ERROR("Database error: ", ex);
            } catch (const std::exception& ex) {
                LOGIT_PRINT_ERROR("General error: ", ex);
            };
            return std::nullopt;
        }

        /// \brief Stores a session value for a specific platform and email.
        /// \param platform The platform name.
        /// \param email The email address.
        /// \param value The session value to store.
        /// \return True if the session value is stored successfully, otherwise false.
        bool set_session_value(const std::string& platform, const std::string& email, const std::string& value) {
            std::lock_guard<std::mutex> lock(m_mutex);
            std::string key = platform + ":" + email;
            try {
                m_db.insert(utils::Base64::encode(key), utils::Base64::encode(m_aes.encrypt(value)));
                return true;
            } catch(const sqlite_containers::sqlite_exception& ex){
                LOGIT_PRINT_ERROR("Database error: ", ex);
            } catch(const std::exception& ex){
                LOGIT_PRINT_ERROR("General error: ", ex);
            }
            return false;
        }

        /// \brief Removes a session value for a specific platform and email.
        /// \param platform The platform name.
        /// \param email The email address.
        /// \return True if the session value is removed successfully, otherwise false.
        bool remove_session(const std::string& platform, const std::string& email) {
            std::lock_guard<std::mutex> lock(m_mutex);
            std::string key = platform + ":" + email;
            try {
                std::string base64_key = utils::Base64::encode(key);
                m_db.remove(base64_key);
                return true;
            } catch(const sqlite_containers::sqlite_exception& ex){
                LOGIT_PRINT_ERROR("Database error: ", ex);
            } catch(const std::exception& ex){
                LOGIT_PRINT_ERROR("General error: ", ex);
            }
            return false;
        }

        /// \brief Clears all session data from the database.
        /// \return True if the database is cleared successfully, otherwise false.
        bool clear() {
            std::lock_guard<std::mutex> lock(m_mutex);
            try {
                m_db.clear();
                return true;
            } catch(const sqlite_containers::sqlite_exception& ex){
                LOGIT_PRINT_ERROR("Database error: ", ex);
            } catch(const std::exception& ex){
                LOGIT_PRINT_ERROR("General error: ", ex);
            }
            return false;
        }

    private:
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
                sqlite_containers::Config config;
#               if defined(_WIN32) || defined(_WIN64)
                config.db_path = utils::get_exec_dir() + std::string("\\") + OPTIONX_SESSION_DB_FILE;
#               else
                config.db_path = utils:get_exec_dir() + std::string("/") + OPTIONX_SESSION_DB_FILE;
#               endif
                config.table_name = "sessions";
                config.busy_timeout = 1000;               // Таймаут при блокировке базы данных.
                config.page_size = 4096;                  // Размер страницы по умолчанию.
                config.cache_size = 1000;                 // Небольшой кэш, так как объем данных невелик.
                config.analysis_limit = 500;              // Уменьшить лимит анализа для оптимизации.
                config.journal_mode = sqlite_containers::JournalMode::DELETE_MODE;
                config.synchronous = sqlite_containers::SynchronousMode::FULL;
                config.auto_vacuum_mode = sqlite_containers::AutoVacuumMode::FULL;
                config.default_txn_mode = sqlite_containers::TransactionMode::DEFERRED;
                m_db.set_config(config);
                m_db.connect();
            } catch(const sqlite_containers::sqlite_exception& ex){
                LOGIT_PRINT_ERROR("Database connection error: ", ex);
            } catch(const std::exception& ex){
                LOGIT_PRINT_ERROR("General error: ", ex);
            }
        }

        ~ServiceSessionDB() = default;

        ServiceSessionDB(const ServiceSessionDB&) = delete;
        ServiceSessionDB& operator=(const ServiceSessionDB&) = delete;

        std::mutex       m_mutex; ///< Mutex for thread safety.
        crypto::AESCrypt m_aes;   ///< AES encryption and decryption instance.
        sqlite_containers::KeyValueDB<std::string, std::string> m_db; ///< The SQLite KeyValueDB instance.
    };

} // namespace optionx::storage

#endif // _OPTIONX_STORAGE_SERVICE_SESSION_DB_HPP_INCLUDED
