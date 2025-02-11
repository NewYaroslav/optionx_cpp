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

#endif // _OPTIONX_HTTP_UTILS_HPP_INCLUDED
