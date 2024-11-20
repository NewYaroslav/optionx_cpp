#pragma once
#ifndef  _OPTIONX_PATH_UTILS_HPP_INCLUDED
#define  _OPTIONX_PATH_UTILS_HPP_INCLUDED

/// \file PathUtils.hpp
/// \brief

namespace optionx {

    /// \brief Retrieves the directory of the executable file.
    /// \return A string containing the directory path of the executable.
    std::string get_exe_path() {
#       ifdef _WIN32
        std::vector<wchar_t> buffer(MAX_PATH);
        HMODULE hModule = GetModuleHandle(NULL);

        // Пробуем получить путь
        DWORD size = GetModuleFileNameW(hModule, buffer.data(), buffer.size());

        // Если путь слишком длинный, увеличиваем буфер
        while (size == buffer.size() && GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
            buffer.resize(buffer.size() * 2);  // Увеличиваем буфер в два раза
            size = GetModuleFileNameW(hModule, buffer.data(), buffer.size());
        }

        if (size == 0) throw std::runtime_error("Failed to get executable path.");
        std::wstring exe_path(buffer.begin(), buffer.begin() + size);

        // Обрезаем путь до директории (удаляем имя файла, оставляем только путь к папке)
        size_t pos = exe_path.find_last_of(L"\\/");
        if (pos != std::wstring::npos) {
            exe_path = exe_path.substr(0, pos);
        }

        // Преобразуем из std::wstring (UTF-16) в std::string (UTF-8)
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        return converter.to_bytes(exe_path);
#       else
        char result[PATH_MAX];
        ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);

        if (count == -1) {
            throw std::runtime_error("Failed to get executable path.");
        }

        std::string exe_path(result, count);

        // Обрезаем путь до директории (удаляем имя файла, оставляем только путь к папке)
        size_t pos = exe_path.find_last_of("\\/");
        if (pos != std::string::npos) {
            exe_path = exe_path.substr(0, pos);
        }

        return exe_path;
#   endif
    }

} // namespace optionx

#endif // PATHUTILS_HPP_INCLUDED
