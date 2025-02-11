#pragma once
#ifndef _OPTIONX_CRYPTO_SECURE_KEY_HPP_INCLUDED
#define _OPTIONX_CRYPTO_SECURE_KEY_HPP_INCLUDED

/// \file SecureKey.hpp
/// \brief Provides a class for secure storage and management of encryption keys.

#include <array>
#include <random>
#include <cstring>
#include <chrono>

namespace optionx {
namespace crypto {

    /// \class SecureKey
    /// \brief Provides secure storage and XOR-based obfuscation for encryption keys.
    class SecureKey {
    public:
        /// \brief Default constructor.
        /// Initializes the XOR key with a randomly generated value.
        SecureKey() {
            m_xor_key = generate_xor_key();
        }

        /// \brief Constructor with key initialization.
        /// \param key The encryption key to secure.
        SecureKey(const std::array<uint8_t, 32>& key) {
            m_xor_key = generate_xor_key();
            set_key(key);
        }

        /// \brief Destructor.
        /// Ensures the keys are cleared from memory.
        ~SecureKey() {
            clear();
        }

        /// \brief Sets a new encryption key.
        /// \param key The new encryption key to secure.
        template<class T>
        void set_key(const T& key) {
            for (size_t i = 0; i < key.size(); ++i) {
                m_encrypted_key[i] = key[i] ^ m_xor_key[i];
            }
        }

        /// \brief Retrieves the decrypted encryption key.
        /// \return The decrypted encryption key as a std::array.
        std::array<uint8_t, 32> get_key() const {
            std::array<uint8_t, 32> decrypted_key;
            for (size_t i = 0; i < m_encrypted_key.size(); ++i) {
                decrypted_key[i] = m_encrypted_key[i] ^ m_xor_key[i];
            }
            return decrypted_key;
        }

        /// \brief Clears both the encrypted key and XOR key from memory.
        /// Ensures that sensitive data is wiped from memory.
        void clear() {
            secure_clear(m_encrypted_key);
            secure_clear(m_xor_key);
        }

    private:

        /// \brief Generates a random XOR key.
        /// \return A randomly generated XOR key as a std::array.
        std::array<uint8_t, 32> generate_xor_key() {
            std::array<uint8_t, 32> xor_key;
            auto seed = static_cast<uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count());
            std::mt19937 generator(seed);
            std::uniform_int_distribution<uint8_t> distribution(0, 255);
            for (auto& byte : xor_key) {
                byte = distribution(generator);
            }
            return xor_key;
        }

        std::array<uint8_t, 32> m_encrypted_key = {};   ///< The encrypted key, stored in an obfuscated form.
        std::array<uint8_t, 32> m_xor_key = {};         ///< The XOR key used for obfuscation of the encrypted key.
    };

} // namespace crypto
} // namespace optionx

#endif // _OPTIONX_CRYPTO_SECURE_KEY_HPP_INCLUDED
