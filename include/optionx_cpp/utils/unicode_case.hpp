#pragma once
#ifndef OPTIONX_HEADER_UTILS_UNICODE_CASE_HPP_INCLUDED
#define OPTIONX_HEADER_UTILS_UNICODE_CASE_HPP_INCLUDED

/// \file unicode_case.hpp
/// \brief Unicode-aware case folding helpers for caseless matching.

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

#if __has_include(<uni_algo/case.h>)
#include <uni_algo/case.h>
#define OPTIONX_UTILS_HAS_UNI_ALGO 1
#else
#define OPTIONX_UTILS_HAS_UNI_ALGO 0
#endif

namespace optionx::utils {

    /// \brief Returns true when the build can use uni-algo Unicode case folding.
    inline constexpr bool has_unicode_case_folding() noexcept {
        return OPTIONX_UTILS_HAS_UNI_ALGO != 0;
    }

    /// \brief Folds text for case-insensitive matching.
    /// \details Uses Unicode Default Case Folding when uni-algo is available.
    ///          The fallback keeps ASCII and basic Cyrillic behavior for
    ///          minimal builds where third-party Unicode headers are absent.
    inline std::string unicode_case_fold(std::string_view value) {
#if OPTIONX_UTILS_HAS_UNI_ALGO
        return una::cases::to_casefold_utf8(value);
#else
        std::string folded;
        folded.reserve(value.size());

        for (std::size_t index = 0; index < value.size();) {
            const auto ch = static_cast<unsigned char>(value[index]);
            if (ch < 0x80) {
                folded.push_back(static_cast<char>(std::tolower(ch)));
                ++index;
                continue;
            }

            if (index + 1 < value.size()) {
                const auto next = static_cast<unsigned char>(value[index + 1]);
                if (ch == 0xD0 && next >= 0x90 && next <= 0x9F) {
                    folded.push_back(static_cast<char>(0xD0));
                    folded.push_back(static_cast<char>(next + 0x20));
                    index += 2;
                    continue;
                }
                if (ch == 0xD0 && next >= 0xA0 && next <= 0xAF) {
                    folded.push_back(static_cast<char>(0xD1));
                    folded.push_back(static_cast<char>(next - 0x20));
                    index += 2;
                    continue;
                }
                if (ch == 0xD0 && next == 0x81) {
                    folded.push_back(static_cast<char>(0xD1));
                    folded.push_back(static_cast<char>(0x91));
                    index += 2;
                    continue;
                }
            }

            folded.push_back(value[index]);
            ++index;
        }

        return folded;
#endif
    }

    /// \brief Compares two UTF-8 strings after Unicode case folding.
    inline bool unicode_iequals(std::string_view lhs, std::string_view rhs) {
        return unicode_case_fold(lhs) == unicode_case_fold(rhs);
    }

    /// \brief Returns true when a folded haystack contains a folded needle.
    inline bool unicode_case_contains(std::string_view haystack, std::string_view needle) {
        const auto folded_needle = unicode_case_fold(needle);
        if (folded_needle.empty()) {
            return false;
        }
        return unicode_case_fold(haystack).find(folded_needle) != std::string::npos;
    }

} // namespace optionx::utils

#endif // OPTIONX_HEADER_UTILS_UNICODE_CASE_HPP_INCLUDED
