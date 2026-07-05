#pragma once
#ifndef OPTIONX_HEADER_UTILS_LOG_REDACTION_HPP_INCLUDED
#define OPTIONX_HEADER_UTILS_LOG_REDACTION_HPP_INCLUDED

/// \file log_redaction.hpp
/// \brief Helpers for removing sensitive values from diagnostic logs.

#include <algorithm>
#include <cctype>
#include <string>

namespace optionx::utils {

    /// \brief Replaces a sensitive value with a fixed redaction marker.
    /// \param value Secret value that must not be written to logs.
    /// \return Empty string for empty values, otherwise "***".
    inline std::string redact_secret_value(const std::string& value) {
        return value.empty() ? std::string() : std::string("***");
    }

    /// \brief Backward-compatible alias for redacting a single sensitive value.
    inline std::string redact_secret(const std::string& value) {
        return redact_secret_value(value);
    }

    /// \brief Redacts common secret fields inside a diagnostic string.
    /// \param text Text that may contain key/value secrets.
    /// \return Text with recognized secret values replaced by "***".
    inline std::string redact_secrets_in_text(std::string text) {
        static constexpr const char* keys[] = {
            "auth_token",
            "authorization",
            "cookie",
            "cookies",
            "password",
            "passwd",
            "proxy_auth",
            "session",
            "token",
            "user_hash",
            "x-api-token"
        };

        auto to_lower_ascii = [](std::string value) {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
                return static_cast<char>(std::tolower(ch));
            });
            return value;
        };

        auto is_key_boundary = [](char ch) {
            return !(std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == '-');
        };

        auto is_value_delimiter = [](char ch) {
            return ch == '&' || ch == ',' || ch == ';' ||
                   ch == '\r' || ch == '\n' || ch == '\t' || ch == ' ';
        };

        std::string lower = to_lower_ascii(text);
        for (const std::string key : keys) {
            std::size_t pos = 0;
            while ((pos = lower.find(key, pos)) != std::string::npos) {
                const bool left_ok = (pos == 0) || is_key_boundary(lower[pos - 1]);
                const std::size_t key_end = pos + key.size();
                const bool right_ok = key_end >= lower.size() || is_key_boundary(lower[key_end]);
                if (!left_ok || !right_ok) {
                    pos = key_end;
                    continue;
                }

                std::size_t sep = key_end;
                while (sep < lower.size() && lower[sep] == ' ') ++sep;
                if (sep >= lower.size() || (lower[sep] != '=' && lower[sep] != ':')) {
                    pos = key_end;
                    continue;
                }

                std::size_t value_begin = sep + 1;
                while (value_begin < lower.size() && lower[value_begin] == ' ') ++value_begin;
                if (value_begin >= lower.size()) {
                    pos = value_begin;
                    continue;
                }

                const char quote = (lower[value_begin] == '"' || lower[value_begin] == '\'')
                    ? lower[value_begin]
                    : '\0';
                if (quote != '\0') ++value_begin;

                std::size_t value_end = value_begin;
                if (quote != '\0') {
                    while (value_end < lower.size() && lower[value_end] != quote) ++value_end;
                } else if (key == "cookie" || key == "cookies" || key == "authorization") {
                    while (value_end < lower.size() && lower[value_end] != '\r' && lower[value_end] != '\n') {
                        ++value_end;
                    }
                } else {
                    while (value_end < lower.size() && !is_value_delimiter(lower[value_end])) ++value_end;
                }

                text.replace(value_begin, value_end - value_begin, "***");
                lower = to_lower_ascii(text);
                pos = value_begin + 3;
            }
        }

        return text;
    }

} // namespace optionx::utils

#endif // OPTIONX_HEADER_UTILS_LOG_REDACTION_HPP_INCLUDED
