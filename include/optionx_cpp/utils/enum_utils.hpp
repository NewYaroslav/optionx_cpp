#pragma once
#ifndef _OPTIONX_ENUM_UTILS_HPP_INCLUDED
#define _OPTIONX_ENUM_UTILS_HPP_INCLUDED

/// \file enum_utils.hpp
/// \brief Contains utility functions for working with enums.

#include <string>
#include <map>
#include <stdexcept>
#include <vector>

namespace optionx {

    template <typename EnumType>
    EnumType to_enum(const std::string& str);

    namespace utils {

        /// \brief Returns an enum string by index or a stable UNKNOWN fallback.
        /// \param values Dense string table whose first entry is UNKNOWN.
        /// \param index Enum value converted to an index.
        /// \return Matching string, or UNKNOWN when the enum value is out of range.
        inline const std::string& enum_string_or_unknown(
                const std::vector<std::string>& values,
                std::size_t index) noexcept {
            static const std::string unknown = "UNKNOWN";
            if (values.empty()) return unknown;
            return index < values.size() ? values[index] : values.front();
        }

    } // namespace utils

} // namespace optionx

#endif // _OPTIONX_ENUM_UTILS_HPP_INCLUDED
