#pragma once
#ifndef OPTIONX_HEADER_UTILS_METATRADER_PATHS_HPP_INCLUDED
#define OPTIONX_HEADER_UTILS_METATRADER_PATHS_HPP_INCLUDED

/// \file metatrader_paths.hpp
/// \brief Utilities for locating MetaTrader data and common-files directories.

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <knownfolders.h>
#include <objbase.h>
#include <shlobj.h>
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#endif

namespace optionx::utils::metatrader {

#if defined(_WIN32)
    /// \brief Releases memory returned by Windows known-folder APIs.
    struct CoTaskMemDeleter {
        void operator()(wchar_t* ptr) const noexcept {
            CoTaskMemFree(ptr);
        }
    };

    /// \brief Error category used when an HRESULT is not a wrapped Win32 error.
    class HResultErrorCategory final : public std::error_category {
    public:
        const char* name() const noexcept override {
            return "hresult";
        }

        std::string message(int value) const override {
            return "HRESULT " + std::to_string(value);
        }
    };

    /// \brief Returns the process-wide HRESULT error category.
    inline const std::error_category& hresult_error_category() {
        static HResultErrorCategory category;
        return category;
    }

    /// \brief Converts an HRESULT without pretending every value is Win32.
    inline std::error_code make_error_code_from_hresult(const HRESULT hr) {
        if (HRESULT_FACILITY(hr) == FACILITY_WIN32) {
            return std::error_code(
                    static_cast<int>(HRESULT_CODE(hr)),
                    std::system_category());
        }
        return std::error_code(
                static_cast<int>(hr),
                hresult_error_category());
    }
#endif

    /// \enum MetaTraderTerminalKind
    /// \brief Identifies which MQL runtime directories exist in a terminal data directory.
    enum class MetaTraderTerminalKind {
        UNKNOWN = 0, ///< No recognizable MQL runtime directory was found.
        MT4,         ///< The terminal data directory contains MQL4.
        MT5,         ///< The terminal data directory contains MQL5.
        MIXED        ///< Both MQL4 and MQL5 directories are present.
    };

    /// \class MetaTraderTerminalDirectory
    /// \brief Describes one MetaTrader terminal data directory under MetaQuotes\Terminal.
    struct MetaTraderTerminalDirectory {
        std::filesystem::path data_root; ///< Terminal data directory.
        bool has_mql4 = false;           ///< True when MQL4 directory exists.
        bool has_mql5 = false;           ///< True when MQL5 directory exists.

        /// \brief Returns the detected terminal kind.
        MetaTraderTerminalKind kind() const noexcept {
            if (has_mql4 && has_mql5) return MetaTraderTerminalKind::MIXED;
            if (has_mql4) return MetaTraderTerminalKind::MT4;
            if (has_mql5) return MetaTraderTerminalKind::MT5;
            return MetaTraderTerminalKind::UNKNOWN;
        }

        /// \brief Returns `MQL4\Files` inside this terminal data directory.
        std::filesystem::path mql4_files_dir() const {
            return data_root / "MQL4" / "Files";
        }

        /// \brief Returns `MQL5\Files` inside this terminal data directory.
        std::filesystem::path mql5_files_dir() const {
            return data_root / "MQL5" / "Files";
        }
    };

    /// \class MetaTraderDiscoveryResult
    /// \brief Result of a terminal-directory discovery scan.
    struct MetaTraderDiscoveryResult {
        std::vector<MetaTraderTerminalDirectory> terminals; ///< Discovered terminal directories.
        std::error_code error; ///< Filesystem error when the scan could not complete.
        std::filesystem::path error_path; ///< Path that produced the first reported error.
        bool complete = true; ///< False when discovery stopped because of an error.
    };

    /// \class MetaTraderPathResolutionResult
    /// \brief Result of resolving a default MetaTrader-related root path.
    struct MetaTraderPathResolutionResult {
        std::filesystem::path path; ///< Resolved path.
        std::error_code error; ///< Resolution error when no path was resolved.
        bool resolved = false; ///< True when `path` contains a usable value.
    };

    /// \class MetaTraderMqlFilesDirectoriesResult
    /// \brief Result of locating existing MQL `Files` directories for a terminal.
    struct MetaTraderMqlFilesDirectoriesResult {
        std::vector<std::filesystem::path> files_directories; ///< Existing MQL Files directories.
        std::error_code error; ///< First filesystem error observed while checking directories.
        std::filesystem::path error_path; ///< Path that produced the first reported error.
        bool complete = true; ///< False when one or more checks failed.
    };

    /// \brief Records the first discovery error while allowing scanning to continue.
    inline void record_discovery_error(
            MetaTraderDiscoveryResult& result,
            const std::error_code& ec,
            const std::filesystem::path& path) {
        if (!ec) {
            return;
        }
        result.complete = false;
        if (!result.error) {
            result.error = ec;
            result.error_path = path;
        }
    }

    /// \brief Records the first MQL Files lookup error.
    inline void record_mql_files_error(
            MetaTraderMqlFilesDirectoriesResult& result,
            const std::error_code& ec,
            const std::filesystem::path& path) {
        if (!ec) {
            return;
        }
        result.complete = false;
        if (!result.error) {
            result.error = ec;
            result.error_path = path;
        }
    }

    /// \brief Returns true when the path is a symlink or Windows reparse point.
    inline bool is_reparse_or_symlink(
            const std::filesystem::path& path,
            std::error_code& ec) {
        ec.clear();
#if defined(_WIN32)
        const DWORD attributes = GetFileAttributesW(path.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES) {
            const DWORD error = GetLastError();
            if (error == ERROR_FILE_NOT_FOUND ||
                error == ERROR_PATH_NOT_FOUND ||
                error == ERROR_NOT_READY) {
                return false;
            }
            ec = std::error_code(
                    static_cast<int>(error),
                    std::system_category());
            return false;
        }
        return (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
#else
        const auto status = std::filesystem::symlink_status(path, ec);
        if (ec) {
            if (ec == std::errc::no_such_file_or_directory ||
                ec == std::errc::not_a_directory) {
                ec.clear();
            }
            return false;
        }
        return std::filesystem::is_symlink(status);
#endif
    }

    /// \brief Returns true when the path exists and is a directory.
    inline bool directory_exists(
            const std::filesystem::path& path,
            std::error_code& ec) {
        ec.clear();
        const bool exists = std::filesystem::is_directory(path, ec);
        if (ec == std::errc::no_such_file_or_directory ||
            ec == std::errc::not_a_directory) {
            ec.clear();
        }
        return !ec && exists;
    }

    /// \brief Returns true when the path is a directory and not a symlink/reparse point.
    inline bool trusted_directory_exists(
            const std::filesystem::path& path,
            std::error_code& ec) {
        if (is_reparse_or_symlink(path, ec)) {
            ec = std::make_error_code(std::errc::operation_not_permitted);
            return false;
        }
        if (ec) {
            return false;
        }
        return directory_exists(path, ec);
    }

    /// \brief Returns true when the path exists and is a directory.
    inline bool directory_exists(const std::filesystem::path& path) {
        std::error_code ec;
        return directory_exists(path, ec);
    }

    /// \brief Resolves the Windows roaming AppData directory.
    /// \details On Windows, the known-folder API is preferred and APPDATA is a
    /// fallback. On other platforms, APPDATA is used only as an explicit test or
    /// compatibility fallback because MetaTrader common files are Windows-specific.
    inline MetaTraderPathResolutionResult default_roaming_app_data_root_result() {
        MetaTraderPathResolutionResult result;
#if defined(_WIN32)
        PWSTR raw_path = nullptr;
        const HRESULT hr = SHGetKnownFolderPath(
                FOLDERID_RoamingAppData,
                KF_FLAG_DEFAULT,
                nullptr,
                &raw_path);
        std::unique_ptr<wchar_t, CoTaskMemDeleter> known_path(raw_path);
        if (SUCCEEDED(hr) && known_path) {
            result.path = std::filesystem::path(known_path.get());
            result.resolved = true;
            return result;
        }
        if (FAILED(hr)) {
            result.error = make_error_code_from_hresult(hr);
        }
#endif

#if defined(_WIN32)
        DWORD required = GetEnvironmentVariableW(L"APPDATA", nullptr, 0);
        for (int attempt = 0; attempt < 2 && required > 1; ++attempt) {
            std::wstring value(required, L'\0');
            const DWORD written = GetEnvironmentVariableW(
                L"APPDATA",
                value.data(),
                required);
            if (written > 0 && written < required) {
                value.resize(written);
                result.path = std::filesystem::path(value);
                result.resolved = true;
                result.error.clear();
                return result;
            }
            required = written;
        }
        if (!result.error) {
            const DWORD error = GetLastError();
            result.error = std::error_code(
                    static_cast<int>(error ? error : ERROR_ENVVAR_NOT_FOUND),
                    std::system_category());
        }
#else
        const char* appdata = std::getenv("APPDATA");
        if (!appdata || !*appdata) {
            result.error =
                std::make_error_code(std::errc::no_such_file_or_directory);
            return result;
        }
        result.path = std::filesystem::u8path(appdata);
        result.resolved = true;
        return result;
#endif
        return result;
    }

    /// \brief Resolves the Windows roaming AppData directory.
    inline std::filesystem::path default_roaming_app_data_root() {
        return default_roaming_app_data_root_result().path;
    }

    /// \brief Builds `%APPDATA%\MetaQuotes\Terminal` from a roaming AppData root.
    inline std::filesystem::path metaquotes_terminal_root_from_roaming(
            const std::filesystem::path& roaming_root) {
        if (roaming_root.empty()) {
            return {};
        }
        return roaming_root / "MetaQuotes" / "Terminal";
    }

    /// \brief Returns the default MetaQuotes terminal data root.
    inline MetaTraderPathResolutionResult default_metaquotes_terminal_root_result() {
        auto result = default_roaming_app_data_root_result();
        if (!result.resolved) {
            return result;
        }
        result.path = metaquotes_terminal_root_from_roaming(result.path);
        return result;
    }

    /// \brief Returns the default MetaQuotes terminal data root.
    inline std::filesystem::path default_metaquotes_terminal_root() {
        return default_metaquotes_terminal_root_result().path;
    }

    /// \brief Builds a MetaQuotes Common\Files directory from a terminal root.
    inline std::filesystem::path common_files_root_from_terminal_root(
            const std::filesystem::path& terminal_root) {
        if (terminal_root.empty()) {
            return {};
        }
        return terminal_root / "Common" / "Files";
    }

    /// \brief Returns the default MetaQuotes Common\Files root.
    inline MetaTraderPathResolutionResult default_common_files_root_result() {
        auto result = default_metaquotes_terminal_root_result();
        if (!result.resolved) {
            return result;
        }
        result.path = common_files_root_from_terminal_root(result.path);
        return result;
    }

    /// \brief Returns the default MetaQuotes Common\Files root.
    inline std::filesystem::path default_common_files_root() {
        return default_common_files_root_result().path;
    }

    /// \brief Inspects a possible terminal data directory.
    /// \details Classification is based on `MQL4` and `MQL5` directory presence.
    inline MetaTraderTerminalDirectory inspect_terminal_data_directory(
            const std::filesystem::path& data_root,
            std::error_code& ec) {
        MetaTraderTerminalDirectory result;
        result.data_root = data_root;
        ec.clear();

        std::error_code root_ec;
        if (!trusted_directory_exists(data_root, root_ec)) {
            if (root_ec) {
                ec = root_ec;
            }
            return result;
        }

        std::error_code mql_ec;
        result.has_mql4 = trusted_directory_exists(data_root / "MQL4", mql_ec);
        if (mql_ec) {
            ec = mql_ec;
            return result;
        }

        result.has_mql5 = trusted_directory_exists(data_root / "MQL5", mql_ec);
        if (mql_ec) {
            ec = mql_ec;
        }
        return result;
    }

    /// \brief Inspects a possible terminal data directory.
    inline MetaTraderTerminalDirectory inspect_terminal_data_directory(
            const std::filesystem::path& data_root) {
        std::error_code ec;
        return inspect_terminal_data_directory(data_root, ec);
    }

    /// \brief Returns true when a directory looks like a MetaTrader data directory.
    inline bool looks_like_terminal_data_directory(
            const std::filesystem::path& data_root) {
        return inspect_terminal_data_directory(data_root).kind() !=
               MetaTraderTerminalKind::UNKNOWN;
    }

    /// \brief Enumerates known terminal data directories under a MetaQuotes terminal root.
    /// \details The `Common` directory is ignored unless it unexpectedly contains
    /// MQL runtime directories. Returned entries are sorted by path for stable tests.
    inline MetaTraderDiscoveryResult discover_terminal_data_directories_result(
            const std::filesystem::path& terminal_root) {
        MetaTraderDiscoveryResult result;
        if (terminal_root.empty()) {
            return result;
        }

        std::error_code root_reparse_ec;
        if (is_reparse_or_symlink(terminal_root, root_reparse_ec)) {
            record_discovery_error(
                    result,
                    std::make_error_code(std::errc::operation_not_permitted),
                    terminal_root);
            return result;
        }
        if (root_reparse_ec) {
            record_discovery_error(result, root_reparse_ec, terminal_root);
            return result;
        }

        std::error_code root_status_ec;
        const auto root_status =
            std::filesystem::symlink_status(terminal_root, root_status_ec);
        if (root_status_ec) {
            if (root_status_ec == std::errc::no_such_file_or_directory ||
                root_status_ec == std::errc::not_a_directory) {
                root_status_ec.clear();
                return result;
            }
            record_discovery_error(result, root_status_ec, terminal_root);
            return result;
        }
        if (!std::filesystem::is_directory(root_status)) {
            record_discovery_error(
                    result,
                    std::make_error_code(std::errc::not_a_directory),
                    terminal_root);
            return result;
        }

        std::error_code ec;
        std::filesystem::directory_iterator it(terminal_root, ec);
        if (ec) {
            if (ec == std::errc::no_such_file_or_directory) {
                ec.clear();
                return result;
            }
            record_discovery_error(result, ec, terminal_root);
            return result;
        }

        const std::filesystem::directory_iterator end;
        while (it != end) {
            const auto path = it->path();
            std::error_code reparse_ec;
            if (is_reparse_or_symlink(path, reparse_ec)) {
                record_discovery_error(
                    result,
                    std::make_error_code(std::errc::operation_not_permitted),
                    path);
                it.increment(ec);
                if (ec) {
                    record_discovery_error(result, ec, path);
                    break;
                }
                continue;
            }
            if (reparse_ec) {
                record_discovery_error(result, reparse_ec, path);
                it.increment(ec);
                if (ec) {
                    record_discovery_error(result, ec, path);
                    break;
                }
                continue;
            }

            std::error_code type_ec;
            if (std::filesystem::is_directory(path, type_ec) && !type_ec) {
                std::error_code inspect_ec;
                const auto inspected =
                    inspect_terminal_data_directory(path, inspect_ec);
                if (inspect_ec) {
                    record_discovery_error(result, inspect_ec, path);
                    it.increment(ec);
                    if (ec) {
                        record_discovery_error(result, ec, path);
                        break;
                    }
                    continue;
                }
                if (inspected.kind() != MetaTraderTerminalKind::UNKNOWN) {
                    result.terminals.push_back(inspected);
                }
            } else if (type_ec) {
                record_discovery_error(result, type_ec, path);
            }

            it.increment(ec);
            if (ec) {
                record_discovery_error(result, ec, path);
                break;
            }
        }

        std::sort(
            result.terminals.begin(),
            result.terminals.end(),
            [](const MetaTraderTerminalDirectory& lhs,
               const MetaTraderTerminalDirectory& rhs) {
                return lhs.data_root.u8string() < rhs.data_root.u8string();
            });
        return result;
    }

    /// \brief Enumerates known terminal data directories under the default root.
    inline MetaTraderDiscoveryResult discover_terminal_data_directories_result() {
        const auto root = default_metaquotes_terminal_root_result();
        if (root.resolved) {
            return discover_terminal_data_directories_result(root.path);
        }

        MetaTraderDiscoveryResult result;
        result.complete = false;
        result.error = root.error;
        result.error_path = root.path;
        return result;
    }

    /// \brief Enumerates known terminal data directories under a MetaQuotes terminal root.
    /// \details Best-effort convenience overload. Use
    /// `discover_terminal_data_directories_result()` when partial scans and
    /// filesystem errors must be visible to the caller.
    inline std::vector<MetaTraderTerminalDirectory> discover_terminal_data_directories(
            const std::filesystem::path& terminal_root) {
        return discover_terminal_data_directories_result(terminal_root).terminals;
    }

    /// \brief Enumerates known terminal data directories under the default root.
    /// \details Best-effort convenience overload. Use
    /// `discover_terminal_data_directories_result()` when partial scans and
    /// filesystem errors must be visible to the caller.
    inline std::vector<MetaTraderTerminalDirectory> discover_terminal_data_directories() {
        return discover_terminal_data_directories_result().terminals;
    }

    /// \brief Enumerates terminal data directories and reports scan errors.
    inline std::vector<MetaTraderTerminalDirectory> discover_terminal_data_directories(
            const std::filesystem::path& terminal_root,
            std::error_code& ec) {
        const auto result = discover_terminal_data_directories_result(terminal_root);
        ec = result.error;
        return result.terminals;
    }

    /// \brief Returns existing `MQL4\Files` and `MQL5\Files` directories for one terminal.
    inline MetaTraderMqlFilesDirectoriesResult existing_mql_files_directories_result(
            const MetaTraderTerminalDirectory& terminal) {
        MetaTraderMqlFilesDirectoriesResult result;
        std::error_code ec;
        if (!trusted_directory_exists(terminal.data_root, ec)) {
            if (ec) {
                record_mql_files_error(result, ec, terminal.data_root);
            }
            return result;
        }
        if (terminal.has_mql4) {
            const auto mql_root = terminal.data_root / "MQL4";
            const bool mql_root_exists = trusted_directory_exists(mql_root, ec);
            if (ec) {
                record_mql_files_error(result, ec, mql_root);
                return result;
            }
            if (mql_root_exists) {
                const auto dir = terminal.mql4_files_dir();
                if (trusted_directory_exists(dir, ec)) {
                    result.files_directories.push_back(dir);
                } else if (ec) {
                    record_mql_files_error(result, ec, dir);
                }
            }
        }
        if (terminal.has_mql5) {
            const auto mql_root = terminal.data_root / "MQL5";
            const bool mql_root_exists = trusted_directory_exists(mql_root, ec);
            if (ec) {
                record_mql_files_error(result, ec, mql_root);
                return result;
            }
            if (mql_root_exists) {
                const auto dir = terminal.mql5_files_dir();
                if (trusted_directory_exists(dir, ec)) {
                    result.files_directories.push_back(dir);
                } else if (ec) {
                    record_mql_files_error(result, ec, dir);
                }
            }
        }
        return result;
    }

    /// \brief Returns existing `MQL4\Files` and `MQL5\Files` directories for one terminal.
    /// \details Best-effort convenience overload. Use
    /// `existing_mql_files_directories_result()` when partial results and
    /// filesystem errors must be visible to the caller.
    inline std::vector<std::filesystem::path> existing_mql_files_directories(
            const MetaTraderTerminalDirectory& terminal) {
        return existing_mql_files_directories_result(terminal).files_directories;
    }

    /// \brief Returns existing MQL Files directories and reports scan errors.
    inline std::vector<std::filesystem::path> existing_mql_files_directories(
            const MetaTraderTerminalDirectory& terminal,
            std::error_code& ec) {
        const auto result = existing_mql_files_directories_result(terminal);
        ec = result.error;
        return result.files_directories;
    }

} // namespace optionx::utils::metatrader

#endif // OPTIONX_HEADER_UTILS_METATRADER_PATHS_HPP_INCLUDED
