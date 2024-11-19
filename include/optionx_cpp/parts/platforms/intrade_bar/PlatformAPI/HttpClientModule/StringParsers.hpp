#pragma once
#ifndef _OPTIONX_PLATFORMS_INTRADERBAR_HTTP_CLIENT_MODULE_STRING_PARSERS_HPP_INCLUDED
#define _OPTIONX_PLATFORMS_INTRADERBAR_HTTP_CLIENT_MODULE_STRING_PARSERS_HPP_INCLUDED

/// \file StringParsers.hpp
/// \brief

#include <optional>
#include "../../../../utils/StringUtils.hpp"

namespace optionx {
namespace platforms {
namespace intrade_bar {

    /// \brief Parses the login response and extracts user ID and hash.
    /// \param content The HTTP response content to parse.
    /// \return A pair of optional values for user_id and user_hash. If parsing fails, both values will be std::nullopt.
    std::optional<std::pair<std::string, std::string>> parse_login(const std::string& content) {
        try {
            std::string user_id, user_hash, fragment;
            // Extract "/auth/" fragment
            if (extract_between(content, "/auth/", "'", fragment) == std::string::npos || fragment.empty()) {
                LOGIT_ERROR("Failed to extract auth fragment.");
                return std::nullopt;
            }

            // Extract user ID
            if (extract_between(fragment, "id=", "&", user_id) == std::string::npos || user_id.empty()) {
                LOGIT_ERROR("Failed to extract user ID.");
                return std::nullopt;
            }

            // Extract user hash
            if (extract_after(fragment, "hash=", user_hash) == std::string::npos || user_hash.empty()) {
                LOGIT_ERROR("Failed to extract user hash.");
                return std::nullopt;
            }

            return {{user_id, user_hash}};
        } catch (...) {
            return std::nullopt;
        }
    }

    std::optional<std::pair<double, CurrencyType>> parse_balance(const std::string& content) {
        try {
            const std::string STR_RUB = u8"₽";
            const std::string STR_USD = u8"$";

            CurrencyType currency = CurrencyType::UNKNOWN;
            if (content.find(STR_RUB) != std::string::npos || content.find("RUB") != std::string::npos) {
                currency = CurrencyType::RUB;
            } else if (content.find(STR_USD) != std::string::npos || content.find("USD") != std::string::npos) {
                currency = CurrencyType::USD;
            } else {
                LOGIT_ERROR("Unsupported currency type detected.");
                return std::nullopt;
            }

            std::string cleaned_content = content;
            cleaned_content.replace(cleaned_content.find(","), 1, ".");
            const std::string marker = (currency == CurrencyType::RUB) ? STR_RUB : STR_USD;
            size_t pos = cleaned_content.find(marker);
            if (pos != std::string::npos) {
                cleaned_content = cleaned_content.substr(0, pos);
            }

            cleaned_content.erase(std::remove(cleaned_content.begin(), cleaned_content.end(), ' '), cleaned_content.end());
            return {{std::stod(cleaned_content), currency}};
        } catch (...) {
            return std::nullopt;
        }
    }

    /// \brief Parses the response content to extract account and currency information.
    /// \param content The HTTP response content to parse.
    /// \return A pair of optional values for CurrencyType and AccountType. If parsing fails, both values will be std::nullopt.
    std::pair<CurrencyType, AccountType> parse_profile_response(const std::string& content) {
        // Strings for matching
        const char str_demo_ru[] = u8"Демо";
        const char str_real_ru[] = u8"Реал";
        const char str_demo_en[] = u8"Demo";
        const char str_real_en[] = u8"Real";
        const char str_rub[] = u8"RUB";
        const char str_usd[] = u8"USD";

        CurrencyType currency = CurrencyType::UNKNOWN;
        AccountType account = AccountType::UNKNOWN;

        // Offset for parsing
        size_t offset = 0;
        while (true) {
            std::string temp;
            size_t new_offset = extract_between(content, "<div class=\"radio\">", "</div>", temp, offset);
            if (new_offset == std::string::npos) break;
            offset = new_offset;

            // Determine account type
            if ((temp.find(str_demo_ru) != std::string::npos ||
                 temp.find(str_demo_en) != std::string::npos) &&
                temp.find("checked=\"checked\"") != std::string::npos) {
                account = AccountType::DEMO;
            } else if ((temp.find(str_real_ru) != std::string::npos ||
                        temp.find(str_real_en) != std::string::npos) &&
                       temp.find("checked=\"checked\"") != std::string::npos) {
                account = AccountType::REAL;
            }

            // Determine currency
            if (temp.find(str_rub) != std::string::npos &&
                temp.find("checked=\"checked\"") != std::string::npos) {
                currency = CurrencyType::RUB;
            } else if (temp.find(str_usd) != std::string::npos &&
                       temp.find("checked=\"checked\"") != std::string::npos) {
                currency = CurrencyType::USD;
            }
        }

        return {currency, account};
    }

    /// \brief Parses the main page response and extracts necessary fields.
    /// \param content The HTTP response content.
    /// \param headers The HTTP response headers.
    /// \return An optional tuple containing request ID, request value, and cookies. Returns std::nullopt if parsing fails.
    std::optional<std::tuple<std::string, std::string, std::string>> parse_main_page_response(
            const std::string& content,
            const kurlyk::Headers& headers) {
        std::string req_id, req_value, cookies;

        // Extract hidden input fields
        std::string fragment;
        if (extract_between(content, "<input type=\"hidden\" name=\"g-rec-res-l\"", "\">", fragment) == std::string::npos) {
            LOGIT_ERROR("Failed to extract hidden input fragment.");
            return std::nullopt;
        }
        if (extract_between(fragment, "id=\"", "\"", req_id) == std::string::npos) {
            LOGIT_ERROR("Failed to extract request ID.");
            return std::nullopt;
        }
        if (extract_after(fragment, "value=\"", req_value) == std::string::npos) {
            LOGIT_ERROR("Failed to extract request value.");
            return std::nullopt;
        }

        // Parse cookies
        kurlyk::Cookies set_cookie;
        for (const auto& item : headers) {
            if (item.first == "set-cookie") {
                kurlyk::Cookies cookie(kurlyk::utils::parse_cookie(item.second));
                set_cookie.insert(cookie.begin(), cookie.end());
            }
        }
        cookies = kurlyk::utils::to_cookie_string(set_cookie);

        return std::make_tuple(req_id, req_value, cookies);
    }

} // namespace intrade_bar
} // namespace platforms
} // namespace optionx

#endif // _OPTIONX_PLATFORMS_INTRADERBAR_HTTP_CLIENT_MODULE_STRING_PARSERS_HPP_INCLUDED
