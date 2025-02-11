#pragma once
#ifndef _OPTIONX_CRYPTO_AES_UTILS_HPP_INCLUDED
#define _OPTIONX_CRYPTO_AES_UTILS_HPP_INCLUDED

/// \file AESUtils.hpp
/// \brief Provides utility functions for AES encryption and decryption.

#include <string>
#include <array>
#include <random>
#include <chrono>
#include <stdexcept>
#include <AES.h>

#ifdef _WIN32
#include <Windows.h>
#define optionx_secure_clear_impl SecureZeroMemory
#else
#include <cstring>
inline void optionx_secure_clear_impl(void* ptr, size_t size) {
    std::memset(ptr, 0, size);
    __asm__ __volatile__("" : : "r"(ptr) : "memory");
}
#endif

namespace optionx {
namespace crypto {

    constexpr size_t BLOCK_SIZE = 16; ///< AES block size in bytes (128 bits).

    /// \enum AesMode
    /// \brief Represents the available AES encryption modes.
    enum class AesMode {
        CBC_256,
        CBC_192,
        CBC_128,
        CFB_256,
        CFB_192,
        CFB_128
    };

    /// \brief Retrieves the key length for the given AES mode.
    /// \param mode The AES encryption mode.
    /// \return The corresponding AES key length.
    inline AESKeyLength get_aes_key_length(AesMode mode) {
        switch (mode) {
            case AesMode::CBC_256:
            case AesMode::CFB_256:
                return AESKeyLength::AES_256;
            case AesMode::CBC_192:
            case AesMode::CFB_192:
                return AESKeyLength::AES_192;
            case AesMode::CBC_128:
            case AesMode::CFB_128:
                return AESKeyLength::AES_128;
            default:
                throw std::invalid_argument("Invalid AES mode.");
        }
    }

    /// \brief Validates the length of the encryption key for the given mode.
    /// \param key The encryption key.
    /// \param mode The AES encryption mode.
    /// \throws std::invalid_argument If the key length does not match the expected length.
    template <class T>
    void validate_key_length(const T& key, AesMode mode) {
        size_t expected_length = 0;
        switch (mode) {
            case AesMode::CBC_256:
            case AesMode::CFB_256:
                expected_length = 32; // 256-bit key
                break;
            case AesMode::CBC_192:
            case AesMode::CFB_192:
                expected_length = 24; // 192-bit key
                break;
            case AesMode::CBC_128:
            case AesMode::CFB_128:
                expected_length = 16; // 128-bit key
                break;
            default:
                throw std::invalid_argument("Invalid AES mode.");
        }

        if (key.size() < expected_length) {
            throw std::invalid_argument("Invalid key length for the specified AES mode.");
        }
    }

    /// \brief Securely clears a memory region.
    /// \param ptr Pointer to the memory region to clear.
    /// \param size Size of the memory region in bytes.
    /// \details This function ensures that the memory is zeroed out and not optimized away by the compiler.
    inline void secure_clear(void* ptr, size_t size) {
        optionx_secure_clear_impl(ptr, size);
    }

    /// \brief Securely clears a container holding sensitive data.
    /// \tparam T Type of the container (e.g., std::array, std::vector).
    /// \param key Container to be securely cleared.
    /// \details The contents of the container are cleared, and the memory is zeroed out.
    template<class T>
    inline void secure_clear(T& key) {
        optionx_secure_clear_impl(key.data(), key.size());
    }

    /// \brief Generates a random IV (initialization vector).
    /// \return A random IV as a std::array<uint8_t, 16>.
    inline std::array<uint8_t, BLOCK_SIZE> generate_iv() {
        std::array<uint8_t, BLOCK_SIZE> iv;
        auto seed = static_cast<uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count());
        std::mt19937 generator(seed);
        std::uniform_int_distribution<uint8_t> distribution(0, 255);
        for (auto& byte : iv) {
            byte = distribution(generator);
        }
        return iv;
    }

    /// \brief Adds PKCS#7 padding to the input data.
    /// \param data The input string to pad.
    /// \return The padded string.
    inline std::string add_padding(const std::string& data) {
        size_t padding_size = BLOCK_SIZE - (data.size() % BLOCK_SIZE);
        std::string padded_data = data;
        padded_data.append(padding_size, static_cast<char>(padding_size));
        return padded_data;
    }

    /// \brief Removes PKCS#7 padding from the input data.
    /// \param data The padded string.
    /// \return The unpadded string.
    /// \throws std::invalid_argument If the padding is invalid.
    inline std::string remove_padding(const std::string& data) {
        if (data.empty()) {
            throw std::invalid_argument("Data is empty, cannot remove padding.");
        }
        unsigned char padding_size = static_cast<unsigned char>(data.back());
        if (padding_size > BLOCK_SIZE || padding_size == 0) {
            throw std::invalid_argument("Invalid padding size.");
        }
        for (size_t i = data.size() - padding_size; i < data.size(); ++i) {
            if (static_cast<unsigned char>(data[i]) != padding_size) {
                throw std::invalid_argument("Invalid padding detected.");
            }
        }
        return data.substr(0, data.size() - padding_size);
    }

    /// \brief Appends the IV to the beginning of the ciphertext.
    /// \param ciphertext The ciphertext.
    /// \param iv The initialization vector (IV) of size 16 bytes.
    /// \return A string containing the IV followed by the ciphertext.
    inline std::string add_iv_to_ciphertext(const std::string& ciphertext, const std::array<uint8_t, BLOCK_SIZE>& iv) {
        std::string result;
        result.reserve(iv.size() + ciphertext.size());
        result.append(reinterpret_cast<const char*>(iv.data()), iv.size());
        result.append(ciphertext);
        return result;
    }

    /// \brief Extracts the IV from the beginning of the ciphertext.
    /// \param ciphertext_with_iv The input string containing the IV and the ciphertext.
    /// \param iv A reference to an array to store the extracted IV.
    /// \return The ciphertext without the prepended IV.
    /// \throws std::invalid_argument If the input string is too short to contain a valid IV.
    inline std::string extract_iv_from_ciphertext(const std::string& ciphertext_with_iv, std::array<uint8_t, BLOCK_SIZE>& iv) {
        if (ciphertext_with_iv.size() < BLOCK_SIZE) {
            throw std::invalid_argument("Ciphertext is too short to contain a valid IV.");
        }
        std::copy(ciphertext_with_iv.begin(), ciphertext_with_iv.begin() + BLOCK_SIZE, iv.begin());
        return ciphertext_with_iv.substr(BLOCK_SIZE);
    }

    /// \brief Encrypts a string using AES in the specified mode.
    /// \tparam T Type of the encryption key (e.g., std::array<uint8_t, 32>).
    /// \param plain_text The string to encrypt.
    /// \param key The AES encryption key.
    /// \param mode The AES encryption mode.
    /// \return The encrypted string with the IV prepended.
    /// \throws std::invalid_argument If the key length is invalid or the AES mode is unsupported.
    template <class T>
    std::string encrypt(const std::string& plain_text, const T& key, AesMode mode) {
        validate_key_length(key, mode);
        AES aes(get_aes_key_length(mode));
        auto iv = generate_iv();
        std::string padded_text = add_padding(plain_text);

        unsigned char* encrypted = nullptr;
        switch (mode) {
            case AesMode::CBC_256:
            case AesMode::CBC_192:
            case AesMode::CBC_128:
                encrypted = aes.EncryptCBC(
                    reinterpret_cast<const unsigned char*>(padded_text.data()),
                    padded_text.size(),
                    key.data(),
                    iv.data()
                );
                break;
            case AesMode::CFB_256:
            case AesMode::CFB_192:
            case AesMode::CFB_128:
                encrypted = aes.EncryptCFB(
                    reinterpret_cast<const unsigned char*>(padded_text.data()),
                    padded_text.size(),
                    key.data(),
                    iv.data()
                );
                break;
            default:
                throw std::invalid_argument("Invalid AES mode.");
        }

        std::string ciphertext(reinterpret_cast<char*>(encrypted), padded_text.size());
        delete[] encrypted;
        return add_iv_to_ciphertext(ciphertext, iv);
    }

    /// \brief Decrypts a string using AES in the specified mode.
    /// \tparam T Type of the encryption key (e.g., std::array<uint8_t, 32>).
    /// \param encrypted_text The string to decrypt, which contains the IV prepended.
    /// \param key The AES decryption key.
    /// \param mode The AES decryption mode.
    /// \return The decrypted string.
    /// \throws std::invalid_argument If the key length is invalid or the AES mode is unsupported.
    template <class T>
    std::string decrypt(const std::string& encrypted_text, const T& key, AesMode mode) {
        validate_key_length(key, mode);
        std::array<uint8_t, 16> iv;
        std::string ciphertext = extract_iv_from_ciphertext(encrypted_text, iv);
        AES aes(get_aes_key_length(mode));

        unsigned char* decrypted = nullptr;
        switch (mode) {
            case AesMode::CBC_256:
            case AesMode::CBC_192:
            case AesMode::CBC_128:
                decrypted = aes.DecryptCBC(
                    reinterpret_cast<const unsigned char*>(ciphertext.data()),
                    ciphertext.size(),
                    key.data(),
                    iv.data()
                );
                break;
            case AesMode::CFB_256:
            case AesMode::CFB_192:
            case AesMode::CFB_128:
                decrypted = aes.DecryptCFB(
                    reinterpret_cast<const unsigned char*>(ciphertext.data()),
                    ciphertext.size(),
                    key.data(),
                    iv.data()
                );
                break;
            default:
                throw std::invalid_argument("Invalid AES mode.");
        }

        std::string plain_text(reinterpret_cast<char*>(decrypted), ciphertext.size());
        delete[] decrypted;
        return remove_padding(plain_text);
    }

} // namespace crypto
} // namespace optionx

#endif // _OPTIONX_CRYPTO_AES_UTILS_HPP_INCLUDED
