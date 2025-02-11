#pragma once
#ifndef _OPTIONX_PATH_UTILS_HPP_INCLUDED
#define _OPTIONX_PATH_UTILS_HPP_INCLUDED

/// \file path_utils.hpp
/// \brief Utilities for filesystem path manipulation and directory operations.

#include <string>
#include <vector>
#include <filesystem>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#endif

namespace optionx::utils {
    namespace fs = std::filesystem;

    /// \brief Retrieves the directory containing the current executable.
    /// \details Uses OS-specific APIs to determine executable location:
    ///          - Windows: GetModuleFileNameW + path manipulation
    ///          - Linux/Unix: readlink("/proc/self/exe") 
    /// \return UTF-8 encoded path to executable directory
    /// \throw std::runtime_error If path resolution fails
    std::string get_exec_dir() {
#       ifdef _WIN32
        std::vector<wchar_t> buffer(MAX_PATH);
        HMODULE hModule = GetModuleHandle(NULL);

        DWORD size = GetModuleFileNameW(hModule, buffer.data(), (DWORD)buffer.size());

        while (size == buffer.size() && GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
            buffer.resize(buffer.size() * 2);
            size = GetModuleFileNameW(hModule, buffer.data(), (DWORD)buffer.size());
        }

        if (size == 0) {
            throw std::runtime_error("Failed to get executable path.");
        }

        std::wstring exe_path(buffer.begin(), buffer.begin() + size);
        std::wstring::size_type pos = exe_path.find_last_of(L"\\/");
        if (pos != std::wstring::npos) {
            exe_path = exe_path.substr(0, pos);
        }
        return std::filesystem::path(exe_path).string();
#       else
        char result[PATH_MAX];
        ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
        if (count == -1) {
            throw std::runtime_error("Failed to get executable path.");
        }
        std::string exe_path(result, count);
        std::string::size_type pos = exe_path.find_last_of("\\/");
        if (pos != std::string::npos) {
            exe_path = exe_path.substr(0, pos);
        }
        return exe_path;
#       endif
    }

    /// \brief Extracts filename component from a path string.
    /// \details Handles both forward and backward slashes as separators.
    /// \param file_path Full path to process (UTF-8 encoded)
    /// \return Filename component including extension. Returns input string if
    ///         no directory separators found.
    std::string get_file_name(const std::string& file_path) {
        std::size_t pos = file_path.find_last_of("/\\");
        return (pos == std::string::npos) ? file_path : file_path.substr(pos + 1);
    }

    /// \brief Computes relative path between two locations.
    /// \param file_path Target absolute path (UTF-8)
    /// \param base_path Reference absolute path (UTF-8)
    /// \return Relative path from base_path to file_path. Returns original file_path
    ///         if relative computation fails.
    /// \note Uses std::filesystem::relative with error handling
    inline std::string make_relative(const std::string& file_path, const std::string& base_path) {
        if (base_path.empty()) return file_path;
        std::filesystem::path fileP = std::filesystem::u8path(file_path);
        std::filesystem::path baseP = std::filesystem::u8path(base_path);
        std::error_code ec;
        std::filesystem::path relativeP = std::filesystem::relative(fileP, baseP, ec);
        return ec ? file_path : relativeP.u8string();
    }

    /// \brief Resolves path relative to executable directory.
    /// \param relative_path Path component to append to executable directory
    /// \return Absolute path constructed as: exec_dir/relative_path
    /// \see get_exec_dir()
    std::string resolve_exec_path(const std::string& relative_path) {
        return std::filesystem::absolute(get_exec_dir() + "/" + relative_path).string();
    }

    /// \brief Creates directory structure recursively.
    /// \param path Directory path to create (UTF-8 encoded)
    /// \throw std::runtime_error If directory creation fails
    /// \note No-op if directory already exists
    void create_directories(const std::string& path) {
        std::filesystem::path dir(path);
        if (!std::filesystem::exists(dir)) {
            std::error_code ec;
            if (!std::filesystem::create_directories(dir, ec)) {
                throw std::runtime_error("Failed to create directories for path: " + path);
            }
        }
    }

} // namespace optionx::utils

#endif // _OPTIONX_PATH_UTILS_HPP_INCLUDED
