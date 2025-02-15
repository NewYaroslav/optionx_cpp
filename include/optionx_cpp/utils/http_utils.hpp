#pragma once
#ifndef _OPTIONX_HTTP_UTILS_HPP_INCLUDED
#define _OPTIONX_HTTP_UTILS_HPP_INCLUDED

/// \file http_utils.hpp
/// \brief Utility functions and dependencies for HTTP requests.

/// \note Ensure correct inclusion order for Windows socket headers.
#ifdef _WIN32
#   ifndef WIN32_LEAN_AND_MEAN
#   define WIN32_LEAN_AND_MEAN
#   endif
    #include <windows.h>
    #include <wincrypt.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
#endif

#include <kurlyk.hpp>

namespace optionx::utils {

    /// \brief Removes the first occurrence of "https://" or "http://" from the given URL.
    /// \param url The URL from which to remove the substring.
    /// \return std::string The modified URL with the first occurrence of "https://" or "http://" removed.
    std::string remove_http_prefix(const std::string& url) {
        const std::string https_prefix = "https://";
        const std::string http_prefix = "http://";

        std::string modified_url = url;

        // Find the position of "https://" or "http://"
        std::size_t pos = modified_url.find(https_prefix);
        if (pos == std::string::npos) {
            pos = modified_url.find(http_prefix);
        }

        // If found, erase the substring
        if (pos != std::string::npos) {
            if (modified_url.compare(pos, https_prefix.length(), https_prefix) == 0) {
                modified_url.erase(pos, https_prefix.length());
            } else if (modified_url.compare(pos, http_prefix.length(), http_prefix) == 0) {
                modified_url.erase(pos, http_prefix.length());
            }
        }

        return modified_url;
    }

}

#endif // _OPTIONX_HTTP_UTILS_HPP_INCLUDED
