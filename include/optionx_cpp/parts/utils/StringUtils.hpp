#pragma once
#ifndef _OPTIONX_STRING_UTILS_HPP_INCLUDED
#define _OPTIONX_STRING_UTILS_HPP_INCLUDED

/// \file StringUtils.hpp
/// \brief Provides utility functions for string manipulation, hex conversion, and formatting.

#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <locale>
#include <sstream>
#include <regex>
#include <cstdarg>

namespace optionx {

    /// \brief Converts a hexadecimal string to a byte array.
    /// \param source Hexadecimal string.
    /// \return Byte array represented by the hex string.
    std::vector<uint8_t> str_hex_to_vector(const std::string &source) noexcept {
        if (source.find_first_not_of("0123456789ABCDEFabcdef") != std::string::npos) {
            return {};
        }

        union {
            uint64_t binary;
            char byte[8];
        } value{};

        auto size = source.size(), offset = (size % 16);
        std::vector<uint8_t> binary;
        binary.reserve((size + 1) / 2);

        if (offset) {
            value.binary = std::stoull(source.substr(0, offset), nullptr, 16);
            for (auto index = (offset + 1) / 2; index--;) {
                binary.emplace_back(value.byte[index]);
            }
        }

        for (; offset < size; offset += 16) {
            value.binary = std::stoull(source.substr(offset, 16), nullptr, 16);
            for (auto index = 8; index--;) {
                binary.emplace_back(value.byte[index]);
            }
        }

        return binary;
    }

    /// \brief Converts a number to a hexadecimal string.
    /// \tparam I Integer type.
    /// \param w Number to convert.
    /// \param hex_len Length of the resulting hex string.
    /// \return Hexadecimal string.
    template <typename I>
    std::string n2hexstr(I w, size_t hex_len = sizeof(I) << 1) noexcept {
        static const char* digits = "0123456789ABCDEF";
        std::string rc(hex_len, '0');
        for (size_t i = 0, j = (hex_len - 1) * 4; i < hex_len; ++i, j -= 4)
            rc[i] = digits[(w >> j) & 0x0f];
        return rc;
    }

    /// \brief Converts a byte vector to a hexadecimal string.
    /// \param source Byte vector.
    /// \return Hexadecimal string.
    std::string vector_to_str_hex(const std::vector<uint8_t> &source) noexcept {
        std::string temp;
        for (const auto& byte : source) {
            temp += n2hexstr(byte);
        }
        return temp;
    }

    /// \brief Removes all whitespace characters from a string.
    /// \param str String to be processed.
    inline void remove_space(std::string &str) noexcept {
        str.erase(std::remove_if(
            std::begin(str), std::end(str),
            [](unsigned char ch) { return std::isspace(ch); }
        ), str.end());
    }

    /// \brief Converts a string to uppercase.
    /// \param str Input string.
    /// \return Uppercase version of the string.
    std::string to_upper_case(std::string str) {
        std::transform(str.begin(), str.end(), str.begin(), [](unsigned char ch) {
            return std::toupper(ch);
        });
        return str;
    }

    /// \brief Converts a string to lowercase.
    /// \param str Input string.
    /// \return Lowercase version of the string.
    std::string to_lower_case(std::string str) {
        std::transform(str.begin(), str.end(), str.begin(), [](unsigned char ch) {
            return std::tolower(ch);
        });
        return str;
    }

    /// \brief Replaces all occurrences of a substring within a string.
    /// \param inout String to be modified.
    /// \param what Substring to find.
    /// \param with Substring to replace with.
    void replace_all(std::string& inout, const std::string &what, const std::string &with) {
        for (std::string::size_type pos{};
             (pos = inout.find(what, pos)) != std::string::npos;
             pos += with.length()) {
            inout.replace(pos, what.length(), with);
        }
    }

    /// \brief Extracts a substring from the source string following a delimiter.
    /// \param source Source string.
    /// \param delimiter Starting delimiter.
    /// \param out Output substring.
    /// \return Position of the delimiter in the source string.
    std::size_t extract_after(const std::string &source, const std::string &delimiter, std::string &out) {
        std::size_t beg_pos = source.find(delimiter);
        if (beg_pos != std::string::npos) {
            out = source.substr(beg_pos + delimiter.size());
            return beg_pos;
        }
        return std::string::npos;
    }

    /// \brief Extracts a substring from the source string between two delimiters.
    /// \param source Source string.
    /// \param start_delimiter Starting delimiter.
    /// \param end_delimiter Ending delimiter.
    /// \param out Output substring.
    /// \param start_pos Starting position for search.
    /// \return Position of the ending delimiter in the source string.
    std::size_t extract_between(
        const std::string &source,
        const std::string &start_delimiter,
        const std::string &end_delimiter,
        std::string &out,
        std::size_t start_pos = 0) {

        std::size_t beg_pos = source.find(start_delimiter, start_pos);
        if (beg_pos != std::string::npos) {
            std::size_t end_pos = source.find(end_delimiter, beg_pos + start_delimiter.size());
            if (end_pos != std::string::npos) {
                out = source.substr(beg_pos + start_delimiter.size(), end_pos - beg_pos - start_delimiter.size());
                return end_pos;
            }
        }
        return std::string::npos;
    }

    /// \brief Parses a comma-separated list into a vector of strings.
    /// \param value Comma-separated string.
    /// \param items Vector to store parsed items.
    void parse_list(std::string value, std::vector<std::string> &items) noexcept {
        if (value.back() != ',') value += ",";
        std::size_t start_pos = 0;
        while (true) {
            std::size_t found_beg = value.find(',', start_pos);
            if (found_beg != std::string::npos) {
                std::size_t len = found_beg - start_pos;
                if (len > 0) items.push_back(value.substr(start_pos, len));
                start_pos = found_beg + 1;
            } else break;
        }
    }

    /// \brief Formats a string using a printf-style format.
    /// \param fmt Format string.
    /// \return Formatted string.
    std::string format(const char *fmt, ...) {
        va_list args;
        va_start(args, fmt);
        std::vector<char> buffer(1024);
        for (;;) {
            va_list args_copy;
            va_copy(args_copy, args); // Copy args to prevent modifying the original list.
            int res = vsnprintf(buffer.data(), buffer.size(), fmt, args_copy);
            va_end(args_copy); // Clean up the copied argument list.

            if ((res >= 0) && (res < static_cast<int>(buffer.size()))) {
                va_end(args); // Clean up the original argument list.
                return std::string(buffer.data()); // Return the formatted string.
            }

            // If the buffer was too small, resize it.
            const size_t size = res < 0 ? buffer.size() * 2 : static_cast<size_t>(res) + 1;
            buffer.clear();
            buffer.resize(size);
        }
    }

    /// \brief Converts a boolean value to a string.
    /// \param value Boolean value.
    /// \return "true" if value is true, "false" otherwise.
    inline std::string to_bool_str(bool value) noexcept {
        return value ? "true" : "false";
    }

} // namespace optionx

#endif // _OPTIONX_STRING_UTILS_HPP_INCLUDED
