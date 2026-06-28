#pragma once
#ifndef _OPTIONX_PLATFORMS_TRADEUP_HTTP_PARSERS_HPP_INCLUDED
#define _OPTIONX_PLATFORMS_TRADEUP_HTTP_PARSERS_HPP_INCLUDED

/// \file http_parsers.hpp
/// \brief JSON parsers for TradeUp HTTP responses.

#include <optional>
#include <string>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace optionx::platforms::tradeup {
    
    /// \brief Parses the sign-in HTTP response to extract credentials and cookies.
    /// \details Extracts token, user ID, affs ID from the JSON response body,
    ///          and combines cookies from the "set-cookie" headers.
    /// \param content The raw JSON response body.
    /// \param headers The HTTP headers returned with the response (used to extract cookies).
    /// \return Optional tuple (user_id, token, affs_id, cookie_string); std::nullopt if parsing fails.
    inline std::optional<std::tuple<std::string, std::string, std::string, std::string>>
        parse_signin_response(
            const std::string& content,
            const kurlyk::Headers& headers) {

        std::string user_id, token, affs_id, cookies;

        try {
            auto j = nlohmann::json::parse(content);
            if (!j.value("success", false)) {
                LOGIT_ERROR("Sign-in response indicates failure.");
                return std::nullopt;
            }
            const auto& data = j.at("data");
            token = data.value("token", std::string());
            user_id = data.value("userId", std::string());
            affs_id = data.value("affsId", std::string());
        } catch (const nlohmann::json::parse_error& e) {
#           ifdef OPTIONX_LOG_UNIQUE_FILE_INDEX
            const int log_index = OPTIONX_LOG_UNIQUE_FILE_INDEX;
            LOGIT_STREAM_ERROR_TO(log_index) << content;
            LOGIT_PRINT_ERROR(
                "JSON parse error: ", e.what(),
                ". Content log was written to file: ",
                LOGIT_GET_LAST_FILE_NAME(log_index)
            );
#           else
            LOGIT_PRINT_ERROR("JSON parse error: ", e.what());
#           endif
        } catch (...) {
            LOGIT_ERROR("Unknown error.");
            return std::nullopt;
        }

        // Parse "set-cookie" headers
        ::kurlyk::Cookies set_cookie;
		collect_set_cookie(headers, set_cookie);
		if (set_cookie.empty()) {
            LOGIT_PRINT_ERROR("No cookies were found in the sign-in response.");
        }
		// Add nip-auth-token header from parsed token
		if (!token.empty()) {
			Cookie cookie_obj{"nip-auth-token", token};
            set_cookie.emplace(cookie_obj.name, cookie_obj);
        }

        cookies = kurlyk::utils::to_cookie_string(set_cookie);
        return std::make_tuple(user_id, token, affs_id, cookies);
    }

    inline bool parse_success_response(const std::string& content) {
        try {
            auto j = nlohmann::json::parse(content);
            if (!j.value("success", false)) { 
                LOGIT_PRINT_ERROR("Sign-in response is missing 'success' field.");
                return false;
            }
            return true;
        } catch (const nlohmann::json::parse_error& e) {
#           ifdef OPTIONX_LOG_UNIQUE_FILE_INDEX
            const int log_index = OPTIONX_LOG_UNIQUE_FILE_INDEX;
            LOGIT_STREAM_ERROR_TO(log_index) << content;
            LOGIT_PRINT_ERROR(
                "JSON parse error: ", e.what(),
                ". Content log was written to file: ",
                LOGIT_GET_LAST_FILE_NAME(log_index)
            );
#           else
            LOGIT_PRINT_ERROR("JSON parse error: ", e.what());
#           endif
        } catch (const std::exception& e) {
            LOGIT_PRINT_ERROR("Unexpected error: ", e.what());
        } catch (...) {
            LOGIT_PRINT_ERROR("Unknown error.");
        }
        return false;
    }

    /// \brief Parses /api/v1/session/extension response.
    /// \param content Raw JSON body (e.g., {"return":"true","success":true,"message":"OK","data":{"expire":1754682885}}).
    /// \return Optional SessionExtension; std::nullopt on failure.
    inline std::optional<std::tuple<bool, std::string, std::string, std::string, int64_t>
        parse_session_extension_response(
            const std::string& token,
            const std::string& content, 
            const kurlyk::Headers& headers) {
        try {
            const auto j = nlohmann::json::parse(content);

            // Basic checks
            const bool success = j.value("success", false);
            const std::string ret = j.value("return", std::string{});
            const std::string message = j.value("message", std::string{});

            if (!success && ret != "true") {
                LOGIT_PRINT_ERROR("session/extension indicates failure (success:", success ,"; return:", ret,"; message:", message, ").");
                return std::nullopt;
            }

            // Extract expire
            int64_t expire = data["data"]["expire"].get<int64_t>();
			
			// Parse "set-cookie" headers
			::kurlyk::Cookies set_cookie;
			collect_set_cookie(headers, set_cookie);
			if (set_cookie.empty()) {
				LOGIT_PRINT_ERROR("No cookies were found in the sign-in response.");
			}

			// Add nip-auth-token header from parsed token
			if (!token.empty()) {
				Cookie cookie_obj{"nip-auth-token", token};
				set_cookie.emplace(cookie_obj.name, cookie_obj);
			}
            
            cookies = kurlyk::utils::to_cookie_string(set_cookie);
            if (cookies.empty()) {
                LOGIT_PRINT_ERROR("No cookies were found in the sign-in response.");
            }

            return std::make_tuple(success, ret, message, cookies, expire);
        } catch (const nlohmann::json::parse_error& e) {
#           ifdef OPTIONX_LOG_UNIQUE_FILE_INDEX
            const int log_index = OPTIONX_LOG_UNIQUE_FILE_INDEX;
            LOGIT_STREAM_ERROR_TO(log_index) << content;
            LOGIT_PRINT_ERROR(
                "JSON parse error: ", e.what(),
                ". Content log was written to file: ",
                LOGIT_GET_LAST_FILE_NAME(log_index)
            );
#           else
            LOGIT_PRINT_ERROR("JSON parse error: ", e.what());
#           endif
        } catch (const std::exception& e) {
            LOGIT_PRINT_ERROR("Unexpected error: ", e.what());
        } catch (...) {
            LOGIT_PRINT_ERROR("Unknown error.");
        }
        return std::nullopt;
    }

} // namespace optionx::platforms::tradeup

#endif // _OPTIONX_PLATFORMS_TRADEUP_HTTP_PARSERS_HPP_INCLUDED
