#pragma once
#ifndef _OPTIONX_UTILS_BASE64_HPP_INCLUDED
#define _OPTIONX_UTILS_BASE64_HPP_INCLUDED

/// \file Base64.hpp
/// \brief Provides encoding and decoding utilities for Base64 format.

#include <string>
#include <vector>
#include <stdexcept>

namespace optionx::utils {

    /// \class Base64
    /// \brief A utility class for Base64 encoding and decoding.
    class Base64 {
    public:
        /// \brief Encodes a string to Base64 format.
        /// \param input The input string to encode.
        /// \return A Base64-encoded string.
        static std::string encode(const std::string& input) {
            static const char BASE64_ALPHABET[] =
                "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
            std::string output;
            size_t padding = 0;

            for (size_t i = 0; i < input.size(); i += 3) {
                uint32_t buffer = 0;
                size_t remaining = std::min<size_t>(3, input.size() - i);

                for (size_t j = 0; j < remaining; ++j) {
                    buffer |= static_cast<uint8_t>(input[i + j]) << (16 - j * 8);
                }

                for (size_t j = 0; j < 4; ++j) {
                    if (j <= (remaining + 1)) {
                        output += BASE64_ALPHABET[(buffer >> (18 - j * 6)) & 0x3F];
                    } else {
                        output += '=';
                        ++padding;
                    }
                }
            }

            return output;
        }

        /// \brief Decodes a Base64-encoded string back to its original form.
        /// \param input The Base64-encoded string to decode.
        /// \return The decoded original string.
        /// \throws std::invalid_argument If the input is not valid Base64.
        static std::string decode(const std::string& input) {
            static const int BASE64_DECODING_TABLE[] = {
                -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 0-15
                -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 16-31
                -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,  // 32-47
                52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -2, -1, -1,  // 48-63
                -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,            // 64-79
                15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,  // 80-95
                -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,  // 96-111
                41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1   // 112-127
            };

            std::string output;
            uint32_t buffer = 0;
            size_t bits_collected = 0;

            for (char c : input) {
                if (c == '=') {
                    break;
                }

                if (c < 0 || c > 127 || BASE64_DECODING_TABLE[static_cast<unsigned char>(c)] == -1) {
                    throw std::invalid_argument("Invalid Base64 input.");
                }

                buffer = (buffer << 6) | BASE64_DECODING_TABLE[static_cast<unsigned char>(c)];
                bits_collected += 6;

                if (bits_collected >= 8) {
                    bits_collected -= 8;
                    output += static_cast<char>((buffer >> bits_collected) & 0xFF);
                }
            }

            return output;
        }
    };

} // namespace optionx

#endif // _OPTIONX_UTILS_BASE64_HPP_INCLUDED
