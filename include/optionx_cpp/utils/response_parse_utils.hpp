#pragma once
#ifndef _OPTIONX_RESPONSE_PARSE_UTILS_HPP_INCLUDED
#define _OPTIONX_RESPONSE_PARSE_UTILS_HPP_INCLUDED

/// \file response_parse_utils.hpp
/// \brief Lightweight helpers for parsing raw text and HTML-like responses.

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>

#include "string_utils.hpp"

namespace optionx::utils {

    /// \brief Extracts a double-quoted HTML/XML attribute value.
    /// \param html Source markup fragment.
    /// \param attr_name Attribute name.
    /// \return Attribute value or std::nullopt when the attribute is missing.
    inline std::optional<std::string> extract_html_attr(
            const std::string& html,
            const std::string& attr_name) {
        std::string value;
        if (extract_between(html, attr_name + "=\"", "\"", value) == std::string::npos) {
            return std::nullopt;
        }
        return value;
    }

    /// \brief Parses a non-negative decimal int64_t value and rejects partial parses.
    /// \param value Raw numeric string.
    /// \return Parsed value or std::nullopt when the whole string is not digits.
    inline std::optional<int64_t> parse_i64_strict(const std::string& value) {
        if (value.empty()) return std::nullopt;
        if (!std::all_of(value.begin(), value.end(), [](unsigned char ch) {
                return std::isdigit(ch) != 0;
            })) {
            return std::nullopt;
        }
        try {
            return std::stoll(value);
        } catch (...) {
            return std::nullopt;
        }
    }

    /// \brief Parses a non-negative decimal int value and rejects partial parses.
    /// \param value Raw numeric string.
    /// \return Parsed value or std::nullopt when the value is invalid or too large.
    inline std::optional<int> parse_int_strict(const std::string& value) {
        const auto parsed = parse_i64_strict(value);
        if (!parsed ||
            *parsed > static_cast<int64_t>(std::numeric_limits<int>::max())) {
            return std::nullopt;
        }
        return static_cast<int>(*parsed);
    }

    /// \brief Parses a finite double value and rejects partial parses.
    /// \param value Raw numeric string.
    /// \return Parsed value or std::nullopt when the full string is not a number.
    inline std::optional<double> parse_double_strict(const std::string& value) {
        if (value.empty()) return std::nullopt;
        if (std::any_of(value.begin(), value.end(), [](unsigned char ch) {
                return std::isspace(ch) != 0;
            })) {
            return std::nullopt;
        }
        try {
            std::size_t parsed = 0;
            const double result = std::stod(value, &parsed);
            if (parsed != value.size() || !std::isfinite(result)) {
                return std::nullopt;
            }
            return result;
        } catch (...) {
            return std::nullopt;
        }
    }

    /// \brief Extracts an attribute and parses it as a strict non-negative int64_t.
    /// \param html Source markup fragment.
    /// \param attr_name Attribute name.
    /// \return Parsed value or std::nullopt when the attribute is missing or invalid.
    inline std::optional<int64_t> parse_i64_attr(
            const std::string& html,
            const std::string& attr_name) {
        auto value = extract_html_attr(html, attr_name);
        if (!value) return std::nullopt;
        return parse_i64_strict(*value);
    }

    /// \brief Extracts an attribute and parses it as a strict non-negative int.
    /// \param html Source markup fragment.
    /// \param attr_name Attribute name.
    /// \return Parsed value or std::nullopt when the attribute is missing or invalid.
    inline std::optional<int> parse_int_attr(
            const std::string& html,
            const std::string& attr_name) {
        auto value = extract_html_attr(html, attr_name);
        if (!value) return std::nullopt;
        return parse_int_strict(*value);
    }

    /// \brief Extracts an attribute and parses it as a strict finite double.
    /// \param html Source markup fragment.
    /// \param attr_name Attribute name.
    /// \return Parsed value or std::nullopt when the attribute is missing or invalid.
    inline std::optional<double> parse_double_attr(
            const std::string& html,
            const std::string& attr_name) {
        auto value = extract_html_attr(html, attr_name);
        if (!value) return std::nullopt;
        return parse_double_strict(*value);
    }

    /// \brief Checks whether a raw response body contains only whitespace.
    /// \param content Raw response body.
    /// \return True if the body is empty or contains only whitespace.
    inline bool is_blank_response(const std::string& content) {
        return std::all_of(content.begin(), content.end(), [](unsigned char ch) {
            return std::isspace(ch) != 0;
        });
    }

} // namespace optionx::utils

#endif // _OPTIONX_RESPONSE_PARSE_UTILS_HPP_INCLUDED
