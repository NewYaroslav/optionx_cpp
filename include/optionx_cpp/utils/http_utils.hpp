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

#ifndef KURLYK_OAUTH_SUPPORT
#   define KURLYK_OAUTH_SUPPORT 0
#endif

#include <kurlyk.hpp>
#include <logit_cpp/logit.hpp>

namespace optionx::utils {

    /// \brief Removes an "https://" or "http://" prefix from the given URL.
    /// \param url The URL from which to remove the scheme prefix.
    /// \return The URL without a leading "https://" or "http://" scheme.
    inline std::string remove_http_prefix(const std::string& url) {
        const std::string https_prefix = "https://";
        const std::string http_prefix = "http://";

        if (url.compare(0, https_prefix.length(), https_prefix) == 0) {
            return url.substr(https_prefix.length());
        }
        if (url.compare(0, http_prefix.length(), http_prefix) == 0) {
            return url.substr(http_prefix.length());
        }
        return url;
    }

    /// \brief Builds a user-facing error string from an HTTP response.
    /// \param response The HTTP response object.
    /// \param fallback Fallback text used when the response has no detailed error.
    /// \return Detailed error text suitable for callbacks and logs.
    inline std::string describe_response_error(
            const kurlyk::HttpResponsePtr& response,
            const std::string& fallback = "HTTP request failed.") {
        if (!response) {
            return "No response received from the server.";
        }
        if (response->error_code) {
            const std::string details = !response->error_message.empty()
                ? response->error_message
                : response->error_code.message();
            return fallback + " Error: " + details;
        }
        if (!response->ready) {
            return fallback + " Response is not ready.";
        }
        if (response->status_code != 200) {
            if (fallback.find("status code") != std::string::npos) {
                return fallback + std::to_string(response->status_code) + ".";
            }
            return fallback + " Unexpected status code: " + std::to_string(response->status_code) + ".";
        }
        return fallback;
    }
    
    /// \brief Validates the HTTP response for an unexpected status code.
    /// \details Logs an error if the response is null or the status code is not 200.
    /// \param response The HTTP response object.
    /// \param log_message A message to log in case of an unexpected status code.
    /// \return True if the response status code is 200; otherwise, false.
    inline bool validate_status(
            const kurlyk::HttpResponsePtr& response,
            const std::string& log_message) {
        if (!response || response->status_code != 200) {
            const std::string error_text = describe_response_error(response, log_message);
#           ifdef OPTIONX_LOG_UNIQUE_FILE_INDEX
            const int log_index = OPTIONX_LOG_UNIQUE_FILE_INDEX;
            LOGIT_STREAM_ERROR_TO(log_index) << (response ? response->content : "Empty response");
            LOGIT_PRINT_ERROR(
                error_text,
                "; Content log was written to file: ",
                LOGIT_GET_LAST_FILE_NAME(log_index)
            );
#           else
            if (response && response->error_code) {
                LOGIT_PRINT_ERROR(error_text, "; error_code: ", response->error_code);
            } else {
                LOGIT_PRINT_ERROR(error_text);
            }
#           endif
            return false;
        }
        return true;
    }
    
    /// \brief Validates the HTTP response for an unexpected status code.
    /// \details Logs an error if the response is null or the status code is not 200.
    /// \param response The HTTP response object.
    /// \return True if the response status code is 200; otherwise, false.
    inline bool validate_status(const kurlyk::HttpResponsePtr& response) {
        return validate_status(response, "HTTP request failed.");
    }
    
    /// \brief Checks if the HTTP response content indicates DDoS protection.
    /// \details Logs an error if "DDoS-GUARD" is detected in the response content.
    /// \param response The HTTP response object.
    /// \return True if no DDoS protection is detected; otherwise, false.
    inline bool validate_ddos_protection(const kurlyk::HttpResponsePtr& response) {
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
    inline bool validate_response(
            const kurlyk::HttpResponsePtr& response,
            Callback callback) {
        if (!response) {
            LOGIT_ERROR("No response received from the server.");
            callback("No response received from the server.");
            return false;
        }
        if (!validate_status(response)) {
            callback(describe_response_error(response, "Invalid HTTP response."));
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
    inline bool validate_response(
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
