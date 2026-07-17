#pragma once
#ifndef OPTIONX_HEADER_BRIDGES_METATRADER_FILE_META_TRADER_FILE_PATH_UTILS_HPP_INCLUDED
#define OPTIONX_HEADER_BRIDGES_METATRADER_FILE_META_TRADER_FILE_PATH_UTILS_HPP_INCLUDED

/// \file MetaTraderFilePathUtils.hpp
/// \brief Path and identifier helpers for the MetaTrader file transport.

namespace optionx::bridges::metatrader_file {

    /// \brief Returns the default MetaQuotes common files directory.
    /// \return `%APPDATA%\MetaQuotes\Terminal\Common\Files` when it can be resolved; otherwise empty.
    inline std::string default_common_files_root() {
        return optionx::utils::metatrader::default_common_files_root().u8string();
    }

    /// \brief Returns an ASCII-uppercase copy for protocol/path identifiers.
    inline std::string ascii_upper_copy(std::string value) {
        std::transform(
            value.begin(),
            value.end(),
            value.begin(),
            [](const unsigned char ch) {
                return static_cast<char>(std::toupper(ch));
            });
        return value;
    }

    /// \brief Returns true when a path component is a Windows reserved device name.
    /// \details Windows treats names such as `CON` and `COM1` as devices even
    /// when an extension is appended, so the base name before the first dot is checked.
    inline bool is_windows_reserved_device_name(const std::string& value) {
        if (value.empty()) return false;

        const auto dot = value.find('.');
        const auto base = ascii_upper_copy(value.substr(0, dot));
        if (base == "CON" ||
            base == "PRN" ||
            base == "AUX" ||
            base == "NUL") {
            return true;
        }
        if (base.size() == 4 &&
            (base.substr(0, 3) == "COM" || base.substr(0, 3) == "LPT") &&
            base[3] >= '1' &&
            base[3] <= '9') {
            return true;
        }
        return false;
    }

    /// \brief Returns true when a path segment is safe for protocol filenames.
    /// \details The file transport uses `_` as a separator, so stable IDs must
    /// not contain it. Conservative IDs keep parsing portable across MQL and C++.
    inline bool is_safe_file_transport_id(const std::string& value) {
        if (value.empty() ||
            value == "." ||
            value == ".." ||
            value.back() == '.' ||
            is_windows_reserved_device_name(value)) {
            return false;
        }
        for (const unsigned char ch : value) {
            const bool ok =
                (ch >= 'A' && ch <= 'Z') ||
                (ch >= 'a' && ch <= 'z') ||
                (ch >= '0' && ch <= '9') ||
                ch == '-' ||
                ch == '.';
            if (!ok) return false;
        }
        return true;
    }

    /// \brief Returns a normalized absolute path without touching the filesystem.
    inline std::filesystem::path lexically_absolute_normal(
            const std::filesystem::path& path) {
        return std::filesystem::absolute(path).lexically_normal();
    }

    /// \brief Returns true when `child` is inside or equal to `parent`.
    inline bool path_is_within_or_equal(
            const std::filesystem::path& parent,
            const std::filesystem::path& child) {
        const auto normalized_parent = lexically_absolute_normal(parent);
        const auto normalized_child = lexically_absolute_normal(child);

        if (normalized_parent == normalized_child) {
            return true;
        }

        const auto relative = normalized_child.lexically_relative(normalized_parent);
        if (relative.empty() || relative.is_absolute()) {
            return false;
        }

        const auto first = relative.begin();
        return first != relative.end() && *first != "..";
    }

    /// \brief Returns true when a namespace subdirectory is a safe relative path.
    inline bool is_safe_namespace_subdir(const std::string& value) {
        if (value.empty()) return false;
        const auto path = std::filesystem::u8path(value);
        if (path.is_absolute() || path.has_root_name() || path.has_root_directory()) {
            return false;
        }
        for (const auto& part : path) {
            const auto text = part.u8string();
            if (!is_safe_file_transport_id(text)) {
                return false;
            }
        }
        return true;
    }

} // namespace optionx::bridges::metatrader_file

#endif // OPTIONX_HEADER_BRIDGES_METATRADER_FILE_META_TRADER_FILE_PATH_UTILS_HPP_INCLUDED
