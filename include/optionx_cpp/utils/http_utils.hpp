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
#include <logit_cpp/LogIt.hpp>

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
    
    /// \brief Validates the HTTP response for an unexpected status code.
    /// \details Logs an error if the response is null or the status code is not 200.
    /// \param response The HTTP response object.
    /// \param log_message A message to log in case of an unexpected status code.
    /// \return True if the response status code is 200; otherwise, false.
    bool validate_status(
            const kurlyk::HttpResponsePtr& response,
            const std::string& log_message) {
        if (!response || response->status_code != 200) {
#           ifdef OPTIONX_LOG_UNIQUE_FILE_INDEX
            const int log_index = OPTIONX_LOG_UNIQUE_FILE_INDEX;
            LOGIT_STREAM_ERROR_TO(log_index) << (response ? response->content : "Empty response");
            LOGIT_PRINT_ERROR(
                log_message,
                (response ? response->status_code : -1),
                "; Content log was written to file: ",
                LOGIT_GET_LAST_FILE_NAME(log_index)
            );
#           else
            LOGIT_STREAM_INFO() << response->error_code;
            LOGIT_PRINT_ERROR(
                log_message,
                (response ? response->status_code : -1)
            );
#           endif
            return false;
        }
        return true;
    }
    
    /// \brief Validates the HTTP response for an unexpected status code.
    /// \details Logs an error if the response is null or the status code is not 200.
    /// \param response The HTTP response object.
    /// \return True if the response status code is 200; otherwise, false.
    bool validate_status(const kurlyk::HttpResponsePtr& response) {
        return validate_status(response, "Unexpected status code: ");
    }
    
    /// \brief Checks if the HTTP response content indicates DDoS protection.
    /// \details Logs an error if "DDoS-GUARD" is detected in the response content.
    /// \param response The HTTP response object.
    /// \return True if no DDoS protection is detected; otherwise, false.
    bool validate_ddos_protection(const kurlyk::HttpResponsePtr& response) {
        const std::string ddos_marker = "DDoS-GUARD";
        if (response && response->content.find(ddos_marker) != std::string::npos) {
#           ifdef OPTIONX_LOG_UNIQUE_FILE_INDEX
            const int log_index = OPTIONX_LOG_UNIQUE_FILE_INDEX;
            LOGIT_STREAM_ERROR_TO(log_index) << response->content;
            LOGIT_PRINT_ERROR(
                "DDoS protection detected. Content log was written to file: ",
                LOGIT_GET_LAST_FILE_NAME(log_index)
            );
#           else
            LOGIT_ERROR("DDoS protection detected.");
#           endif
            return false;
        }
        return true;
    }

    /// \brief Validates the HTTP response for status and DDoS protection.
    /// \tparam Callback The type of callback to be invoked on failure.
    /// \param response The HTTP response to validate. Must not be nullptr.
    /// \param callback The callback to invoke if validation fails. It receives an error message as a parameter.
    /// \return True if the response is valid; otherwise, false. If false, the callback is invoked.
    template <typename Callback>
    bool validate_response(
            const kurlyk::HttpResponsePtr& response,
            Callback callback) {
        if (!response) {
            LOGIT_ERROR("No response received from the server.");
            callback("No response received from the server.");
            return false;
        }
        if (!validate_status(response)) {
            callback("Invalid status code received from the server.");
            return false;
        }
        if (!validate_ddos_protection(response)) {
            callback("DDoS protection detected.");
            return false;
        }
        return true;
    }
    
    /// \brief Validates the HTTP response for status and DDoS protection.
    /// \param response The HTTP response to validate.
    /// \return True if the response is valid; otherwise, false. If false, the callback is invoked.
    bool validate_response(
        const kurlyk::HttpResponsePtr& response) {
        if (!response) {
            LOGIT_ERROR("No response received from the server.");
            return false;
        }
        if (!validate_status(response)) {
            return false;
        }
        if (!validate_ddos_protection(response)) {
            return false;
        }
        return true;
    }

}

#endif // _OPTIONX_HTTP_UTILS_HPP_INCLUDED
