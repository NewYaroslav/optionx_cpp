#pragma once
#ifndef _OPTIONX_CRYPTO_AES_CRYPT_HPP_INCLUDED
#define _OPTIONX_CRYPTO_AES_CRYPT_HPP_INCLUDED

/// \file AESCrypt.hpp
/// \brief Provides a thread-safe AES encryption and decryption class.

#include "AESUtils.hpp"
#include "SecureKey.hpp"
#include <vector>
#include <mutex>

namespace optionx {
namespace crypto {

    /// \class AESCrypt
    /// \brief Provides thread-safe AES encryption and decryption functionality with secure key management.
    class AESCrypt {
    public:
        /// \brief Default constructor.
        AESCrypt() = default;

        /// \brief Constructor with AES mode.
        /// \param mode The AES encryption mode.
        explicit AESCrypt(AesMode mode) : m_aes_mode(mode) {}

        /// \brief Sets the encryption key.
        /// \tparam T Type of the key container (e.g., std::array, std::vector).
        /// \param key A new key for encryption.
        /// \return True if the key length matches the expected length for the current mode, otherwise false.
        template<class T>
        bool set_key(const T& key) {
            std::lock_guard<std::mutex> lock(m_mutex);
            size_t expected_length = get_expected_key_length();
            if (key.size() != expected_length) {
                return false;
            }
            m_secure.set_key(key);
            return true;
        }

        /// \brief Sets the AES encryption mode.
        /// \param mode The AES encryption mode.
        void set_mode(AesMode mode) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_aes_mode = mode;
        }

        /// \brief Encrypts a string.
        /// \param plain_text The text to encrypt.
        /// \return The encrypted string with the IV prepended.
        /// \throws std::runtime_error If encryption fails.
        std::string encrypt(const std::string& plain_text) const {
            std::lock_guard<std::mutex> lock(m_mutex);
            std::array<uint8_t, 32> key = m_secure.get_key();
            try {
                std::string encrypted_text = optionx::crypto::encrypt(plain_text, key, m_aes_mode);
                secure_clear(key);
                return encrypted_text;
            } catch (...) {
                secure_clear(key);
                throw;
            }
        }

        /// \brief Decrypts a string.
        /// \param encrypted_text The encrypted text with IV prepended.
        /// \return The decrypted string.
        /// \throws std::runtime_error If decryption fails.
        std::string decrypt(const std::string& encrypted_text) const {
            std::lock_guard<std::mutex> lock(m_mutex);
            std::array<uint8_t, 32> key = m_secure.get_key();
            try {
                std::string plain_text = optionx::crypto::decrypt(encrypted_text, key, m_aes_mode);
                secure_clear(key);
                return plain_text;
            } catch (...) {
                secure_clear(key);
                throw;
            }
        }

        /// \brief Encrypts a string without throwing exceptions.
        /// \param plain_text The text to encrypt.
        /// \param encrypted_text [out] The resulting encrypted text.
        /// \return True if encryption succeeds, false otherwise.
        bool encrypt(const std::string& plain_text, std::string& encrypted_text) const {
            try {
                encrypted_text = encrypt(plain_text);
                return true;
            } catch (...) {
                return false;
            }
        }

        /// \brief Decrypts a string without throwing exceptions.
        /// \param encrypted_text The encrypted text with IV prepended.
        /// \param plain_text [out] The resulting decrypted text.
        /// \return True if decryption succeeds, false otherwise.
        bool decrypt(const std::string& encrypted_text, std::string& plain_text) const {
            try {
                plain_text = decrypt(encrypted_text);
                return true;
            } catch (...) {
                return false;
            }
        }

        /// \brief Generates a random key for the current mode.
        /// \return A random key of the correct length for the current mode.
        std::vector<uint8_t> generate_key() const {
            size_t length = get_expected_key_length();
            std::vector<uint8_t> random_key(length);
            auto seed = static_cast<uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count());
            std::mt19937 generator(seed);
            std::uniform_int_distribution<uint8_t> distribution(0, 255);
            for (size_t i = 0; i < length; ++i) {
                random_key[i] = distribution(generator);
            }
            return random_key;
        }

        /// \brief Clears the encryption key from memory.
        void clear_key() {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_secure.clear();
        }

        /// \brief Destructor, ensures the key is cleared from memory.
        ~AESCrypt() {
            clear_key();
        }

    private:
        mutable std::mutex m_mutex;                       ///< Mutex for thread-safe access.
        AesMode            m_aes_mode = AesMode::CBC_256; ///< The AES encryption mode.
        SecureKey          m_secure;                      ///< Secure key storage.

        /// \brief Gets the expected key length for the current mode.
        /// \return The expected key length in bytes.
        /// \throws std::invalid_argument If the AES mode is invalid.
        size_t get_expected_key_length() const {
            switch (m_aes_mode) {
                case AesMode::CBC_256:
                case AesMode::CFB_256:
                    return 32; // 256-bit key
                case AesMode::CBC_192:
                case AesMode::CFB_192:
                    return 24; // 192-bit key
                case AesMode::CBC_128:
                case AesMode::CFB_128:
                    return 16; // 128-bit key
                default:
                    throw std::invalid_argument("Invalid AES mode.");
            }
        }
    };

} // namespace crypto
} // namespace optionx

#endif // _OPTIONX_CRYPTO_AES_CRYPT_HPP_INCLUDED
