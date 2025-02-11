#pragma once
#ifndef _OPTIONX_UTILS_BASE36_HPP_INCLUDED
#define _OPTIONX_UTILS_BASE36_HPP_INCLUDED

/// \file Base36.hpp
/// \brief Provides encoding and decoding utilities for Base36 format.

#include <map>
#include <vector>
#include <string>
#include <random>
#include <atomic>

namespace optionx::utils {

    /// \class Base36
    /// \brief A utility class for Base36 encoding and decoding.
    class Base36 {
    public:
        /// \brief Initializes the Base36 character-to-value mapping.
        /// \return A map of Base36 characters to their integer values.
        inline static std::map<char, int> init_char_map() {
            std::map<char, int> m;
            for (char c = '0'; c <= '9'; ++c) m[c] = c - '0';
            for (char c = 'a'; c <= 'z'; ++c) m[c] = c - 'a' + 10;
            for (char c = 'A'; c <= 'Z'; ++c) m[c] = c - 'A' + 10;
            return m;
        }

        /// \brief Encodes an integer to a Base36 string.
        /// \param n The integer to encode.
        /// \return A Base36 string representation of the integer.
        static std::string encode_int(long long n) {
            std::string result;
            do {
                result = BASE36_MAP[n % BASE36] + result;
                n /= BASE36;
            } while (n > 0);
            return result;
        }

        /// \brief Decodes a Base36 string to an integer.
        /// \param s The Base36 string to decode.
        /// \return The decoded integer value.
        static long long decode_int(const std::string& s) {
            long long result = 0;
            long long base = 1;
            for (auto it = s.rbegin(); it != s.rend(); ++it) {
                auto map_it = BASE36_CHAR_MAP.find(*it);
                if (map_it == BASE36_CHAR_MAP.end()) {
                    throw std::invalid_argument("Invalid Base36 character");
                }
                result += map_it->second * base;
                base *= BASE36;
            }
            return result;
        }

        /// \brief Encodes an array of integers to a Base36 string.
        /// \param arr The array of integers.
        /// \param len The length of the array.
        /// \return A Base36 string representation of the array.
        static std::string encode_array(const int arr[], size_t len) {
            std::string result;
            for (size_t i = 0; i < len; ++i) {
                if (arr[i] >= BASE36 || arr[i] < 0) {
                    result += BASE36_INVALID;
                } else {
                    result += BASE36_MAP[arr[i]];
                }
            }
            return result;
        }

        /// \brief Encodes a vector of integers to a Base36 string.
        /// \param vec The vector of integers.
        /// \return A Base36 string representation of the vector.
        static std::string encode_array(const std::vector<int>& vec) {
            return encode_array(vec.data(), vec.size());
        }

        /// \brief Decodes a Base36 string into a vector of integers.
        /// \param str The Base36 string to decode.
        /// \param vec [out] The resulting vector of integers.
        static void decode_array(const std::string& str, std::vector<int>& vec) {
            for (char c : str) {
                auto map_it = BASE36_CHAR_MAP.find(c);
                vec.push_back(map_it == BASE36_CHAR_MAP.end() ? -1 : map_it->second);
            }
        }

        /// \brief Generates a random Base36 string of a specified length.
        /// \param length The length of the random string.
        /// \return A random Base36 string.
        static std::string random_string(size_t length) {
            std::string result;
            std::mt19937 mt(get_random_seed() + get_random_offset());
            std::uniform_int_distribution<uint32_t> dist(0, BASE36 - 1);
            for (size_t i = 0; i < length; ++i) {
                result += BASE36_MAP[dist(mt)];
            }
            return result;
        }

        /// \brief Encodes a string to a Base36-encoded string.
        /// \param input The input string to encode.
        /// \return A Base36-encoded string representation of the input.
        static std::string encode_string(const std::string& input) {
            std::string result;
            for (unsigned char c : input) {
                result += encode_int(static_cast<long long>(c));
            }
            return result;
        }

        /// \brief Decodes a Base36-encoded string back to its original form.
        /// \param input The Base36-encoded string to decode.
        /// \return The decoded original string.
        /// \throws std::invalid_argument If the input contains invalid Base36 characters.
        static std::string decode_string(const std::string& input) {
            std::string result;
            size_t i = 0;
            while (i < input.size()) {
                std::string chunk;
                while (i < input.size() && chunk.size() < 2) {
                    chunk += input[i++];
                }
                long long value = decode_int(chunk);
                if (value < 0 || value > 255) {
                    throw std::invalid_argument("Invalid Base36 chunk for decoding.");
                }
                result += static_cast<char>(value);
            }
            return result;
        }

        /// \brief Generates a random Base36 string of a random length within a range.
        /// \param length_min The minimum length of the string.
        /// \param length_max The maximum length of the string.
        /// \return A random Base36 string.
        static std::string random_string(size_t length_min, size_t length_max) {
            std::mt19937 mt(get_random_seed() + get_random_offset());
            std::uniform_int_distribution<uint32_t> length_dist(length_min, length_max);
            return random_string(length_dist(mt));
        }

        /// \brief Clears the static random offset.
        inline static void reset_random_offset() {
            random_offset.store(0);
        }

    private:
        /// \brief Gets a random seed based on the current time.
        /// \return A random seed.
        inline static uint64_t get_random_seed() {
            return static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
        }

        /// \brief Gets a unique random offset.
        /// \return A random offset.
        inline static uint64_t get_random_offset() {
            return random_offset++;
        }

        static const int BASE36 = 36; ///< Base36 numeric base.
        static const char BASE36_INVALID = '?'; ///< Character for invalid Base36 values.
        static const char BASE36_MAP[36]; ///< Map of Base36 characters.
        static const std::map<char, int> BASE36_CHAR_MAP; ///< Map of Base36 character values.

        inline static std::atomic<uint64_t> random_offset{0}; ///< Random offset for random string generation.
    };

    const char Base36::BASE36_MAP[36] = {
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
        'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',
        'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't',
        'u', 'v', 'w', 'x', 'y', 'z'
    };

    const std::map<char, int> Base36::BASE36_CHAR_MAP = Base36::init_char_map();

} // namespace optionx

#endif // _OPTIONX_UTILS_BASE36_HPP_INCLUDED
