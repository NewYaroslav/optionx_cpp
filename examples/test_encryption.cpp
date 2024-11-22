#include <iostream>
#include <string>
#include <array>
#include <random>
#include <chrono>
#include "AES.h"
#include "optionx_cpp/parts/crypto/AESCrypt.hpp"

class TestCrypt {
public:

    TestCrypt() {
        m_key = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
                 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
                 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F};
    }

    ~TestCrypt() = default;

    /// \brief Encrypts a string using AES.
    /// \param plain_text The string to encrypt.
    /// \return The encrypted string.
    std::string encrypt(const std::string& plain_text) {
        AES aes(AESKeyLength::AES_256);
        auto iv = generate_iv();
        constexpr size_t IV_SIZE = 16;
        static_assert(iv.size() == IV_SIZE, "IV size must be equal to AES block size (16 bytes).");
        std::string padded_text = add_padding(plain_text);
        unsigned char* encrypted = aes.EncryptCBC(
            reinterpret_cast<const unsigned char*>(padded_text.data()),
            padded_text.size(),
            m_key.data(),
            iv.data()
        );
        std::string ciphertext(reinterpret_cast<char*>(encrypted), padded_text.size());
        delete[] encrypted;
        return add_iv_to_ciphertext(ciphertext, iv);
    }

    /// \brief Decrypts a string using AES.
    /// \param encrypted_text The string to decrypt.
    /// \return The decrypted string.
    std::string decrypt(const std::string& encrypted_text) {
        std::array<uint8_t, 16> iv;
        std::string ciphertext = extract_iv_from_ciphertext(encrypted_text, iv);
        AES aes(AESKeyLength::AES_256);
        unsigned char* decrypted = aes.DecryptCBC(
            reinterpret_cast<const unsigned char*>(ciphertext.data()),
            ciphertext.size(),
            m_key.data(),
            iv.data()
        );
        std::string plain_text(reinterpret_cast<char*>(decrypted), ciphertext.size());
        delete[] decrypted;
        return remove_padding(plain_text);
    }

    /// \brief AES encryption key.
    std::array<uint8_t, 32> m_key;

    /// \brief Генерирует случайный IV (вектор инициализации).
    /// \return Случайный IV в виде std::array<uint8_t, 16>.
    std::array<uint8_t, 16> generate_iv() {
        std::array<uint8_t, 16> iv;
        auto seed = static_cast<uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count());
        std::mt19937 generator(seed);
        std::uniform_int_distribution<uint8_t> distribution(0, 255);

        for (auto& byte : iv) {
            byte = distribution(generator);
        }

        return iv;
    }

    /// \brief Добавляет IV в начало шифротекста.
    /// \param plaintext Шифротекст (std::string).
    /// \param iv Вектор инициализации (IV) длиной 16 байт.
    /// \return Строка, содержащая IV и шифротекст.
    std::string add_iv_to_ciphertext(const std::string& ciphertext, const std::array<uint8_t, 16>& iv) {
        std::string result;
        result.reserve(iv.size() + ciphertext.size());
        result.append(reinterpret_cast<const char*>(iv.data()), iv.size());
        result.append(ciphertext);
        return result;
    }

    /// \brief Извлекает IV из начала шифротекста.
    /// \param ciphertext_with_iv Строка, содержащая IV и шифротекст.
    /// \param iv Ссылка на массив для хранения извлеченного IV.
    /// \return Чистый шифротекст без IV.
    /// \throws std::invalid_argument Если строка меньше длины IV.
    std::string extract_iv_from_ciphertext(const std::string& ciphertext_with_iv, std::array<uint8_t, 16>& iv) {
        constexpr size_t IV_SIZE = 16;
        if (ciphertext_with_iv.size() < IV_SIZE) {
            throw std::invalid_argument("Ciphertext is too short to contain a valid IV.");
        }
        std::copy(ciphertext_with_iv.begin(), ciphertext_with_iv.begin() + IV_SIZE, iv.begin());
        return ciphertext_with_iv.substr(IV_SIZE);
    }

    /// \brief Добавляет PKCS#7 паддинг к данным.
    /// \param data Оригинальный текст.
    /// \return Текст с добавленным паддингом.
    std::string add_padding(const std::string& data) {
        constexpr size_t BLOCK_SIZE = 16; // Размер блока AES
        size_t padding_size = BLOCK_SIZE - (data.size() % BLOCK_SIZE);
        std::string padded_data = data;
        padded_data.append(padding_size, static_cast<char>(padding_size)); // Добавляем паддинг
        return padded_data;
    }

    /// \brief Удаляет PKCS#7 паддинг из данных.
    /// \param data Текст с паддингом.
    /// \return Оригинальный текст без паддинга.
    /// \throws std::invalid_argument Если паддинг некорректен.
    std::string remove_padding(const std::string& data) {
        if (data.empty()) {
            throw std::invalid_argument("Data is empty, cannot remove padding.");
        }
        unsigned char padding_size = static_cast<unsigned char>(data.back());
        if (padding_size > 16 || padding_size == 0) {
            throw std::invalid_argument("Invalid padding size.");
        }
        for (size_t i = data.size() - padding_size; i < data.size(); ++i) {
            if (static_cast<unsigned char>(data[i]) != padding_size) {
                throw std::invalid_argument("Invalid padding detected.");
            }
        }
        return data.substr(0, data.size() - padding_size);
    }
};

int main() {

/*
    TestCrypt crypt;

    std::string text = "1234567812345678";
    std::cout << "text: " << text << std::endl;

    std::string padded_text = crypt.add_padding(text);
    std::cout << "padded_text: " << padded_text.size() << std::endl;

    std::string encrypt_text = crypt.encrypt(text);
    std::cout << "encrypt_text: " << encrypt_text << std::endl;

    std::string decrypt_text = crypt.decrypt(encrypt_text);
    std::cout << "decrypt_text: " << decrypt_text << std::endl;
    */

    //-----------
    optionx::crypto::AESCrypt crypt(optionx::crypto::AesMode::CBC_256);

    auto key = crypt.generate_key();
    crypt.set_key(key);

    std::string text = "Test 1234567812345678!";
    std::cout << "text: " << text << std::endl;

    std::string encrypt_text = crypt.encrypt(text);
    std::cout << "encrypt_text: " << encrypt_text << std::endl;

    std::string decrypt_text = crypt.decrypt(encrypt_text);
    std::cout << "decrypt_text: " << decrypt_text << std::endl;

    return 0;
}
