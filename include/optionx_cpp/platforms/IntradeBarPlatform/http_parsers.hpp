#pragma once
#ifndef _OPTIONX_PLATFORMS_INTRADERBAR_HTTP_CLIENT_MODULE_STRING_PARSERS_HPP_INCLUDED
#define _OPTIONX_PLATFORMS_INTRADERBAR_HTTP_CLIENT_MODULE_STRING_PARSERS_HPP_INCLUDED

/// \file http_parsers.hpp
/// \brief Helper functions for parsing HTTP and WebSocket responses for the
///        IntradeBar trading platform.

#include <algorithm>
#include <optional>
#include <regex>
#include <stdexcept>

#include "optionx_cpp/utils/response_parse_utils.hpp"
#include "ApiResponses.hpp"

namespace optionx::platforms::intrade_bar {

    /// \brief Parses the login response and extracts user ID and hash.
    /// \param content The HTTP response content to parse.
    /// \return A pair of optional values for user_id and user_hash. If parsing fails, both values will be std::nullopt.
    std::optional<std::pair<std::string, std::string>> parse_login(const std::string& content) {
        try {
            std::string user_id, user_hash, fragment;
            // Extract "/auth/" fragment
            if (utils::extract_between(content, "/auth/", "'", fragment) == std::string::npos || fragment.empty()) {
                LOGIT_ERROR("Failed to extract auth fragment.");
                return std::nullopt;
            }

            // Extract user ID
            if (utils::extract_between(fragment, "id=", "&", user_id) == std::string::npos || user_id.empty()) {
                LOGIT_ERROR("Failed to extract user ID.");
                return std::nullopt;
            }

            // Extract user hash
            if (utils::extract_after(fragment, "hash=", user_hash) == std::string::npos || user_hash.empty()) {
                LOGIT_ERROR("Failed to extract user hash.");
                return std::nullopt;
            }

            return {{user_id, user_hash}};
        } catch (...) {
            return std::nullopt;
        }
    }

    /// \brief Parses balance information and detects the currency.
    /// \param content Raw HTML fragment containing the balance value and currency symbol.
    /// \return Optional pair of balance amount and detected currency type. Returns
    ///         std::nullopt if the content cannot be parsed.
    std::optional<std::pair<double, CurrencyType>> parse_balance(const std::string& content) {
        try {
            const std::string STR_RUB = u8"₽";
            const std::string STR_USD = u8"$";

            CurrencyType currency = CurrencyType::UNKNOWN;
            if (content.find(STR_RUB) != std::string::npos || content.find("RUB") != std::string::npos) {
                currency = CurrencyType::RUB;
            } else if (content.find(STR_USD) != std::string::npos || content.find("USD") != std::string::npos) {
                currency = CurrencyType::USD;
            } else {
                LOGIT_ERROR("Unsupported currency type detected.");
                return std::nullopt;
            }

            std::string cleaned_content = content;
            cleaned_content.replace(cleaned_content.find(","), 1, ".");
            const std::string marker = (currency == CurrencyType::RUB) ? STR_RUB : STR_USD;
            size_t pos = cleaned_content.find(marker);
            if (pos != std::string::npos) {
                cleaned_content = cleaned_content.substr(0, pos);
            }

            cleaned_content.erase(std::remove(cleaned_content.begin(), cleaned_content.end(), ' '), cleaned_content.end());
            return {{std::stod(cleaned_content), currency}};
        } catch (...) {
            return std::nullopt;
        }
    }

    /// \brief Parses the response content to extract account and currency information.
    /// \param content The HTTP response content to parse.
    /// \return A pair of optional values for CurrencyType and AccountType. If parsing fails, both values will be std::nullopt.
    std::pair<CurrencyType, AccountType> parse_profile_response(const std::string& content) {
        // Strings for matching
        const char str_demo_ru[] = u8"Демо";
        const char str_real_ru[] = u8"Реал";
        const char str_demo_en[] = u8"Demo";
        const char str_real_en[] = u8"Real";
        const char str_rub[] = u8"RUB";
        const char str_usd[] = u8"USD";

        CurrencyType currency = CurrencyType::UNKNOWN;
        AccountType account = AccountType::UNKNOWN;

        // Offset for parsing
        size_t offset = 0;
        while (true) {
            std::string temp;
            size_t new_offset = utils::extract_between(content, "<div class=\"radio\">", "</div>", temp, offset);
            if (new_offset == std::string::npos) break;
            offset = new_offset;

            // Determine account type
            if ((temp.find(str_demo_ru) != std::string::npos ||
                 temp.find(str_demo_en) != std::string::npos) &&
                temp.find("checked=\"checked\"") != std::string::npos) {
                account = AccountType::DEMO;
            } else if ((temp.find(str_real_ru) != std::string::npos ||
                        temp.find(str_real_en) != std::string::npos) &&
                       temp.find("checked=\"checked\"") != std::string::npos) {
                account = AccountType::REAL;
            }

            // Determine currency
            if (temp.find(str_rub) != std::string::npos &&
                temp.find("checked=\"checked\"") != std::string::npos) {
                currency = CurrencyType::RUB;
            } else if (temp.find(str_usd) != std::string::npos &&
                       temp.find("checked=\"checked\"") != std::string::npos) {
                currency = CurrencyType::USD;
            }
        }

        return {currency, account};
    }

    /// \brief Parses the platform's main page to extract request identifiers and cookies.
    /// \param content The raw HTML of the response body.
    /// \param headers The HTTP headers returned with the response.
    /// \return Optional tuple containing the request identifier, request value
    ///         and a cookie string to be used in subsequent requests. Returns
    ///         std::nullopt if any of the required fields cannot be found.
    std::optional<std::tuple<std::string, std::string, std::string>> parse_main_page_response(
            const std::string& content,
            const kurlyk::Headers& headers) {
        std::string req_id, req_value, cookies;

        // Extract hidden input fields
        std::string fragment;
        if (utils::extract_between(content, "<input type=\"hidden\" name=\"g-rec-res-l\"", "\">", fragment) == std::string::npos) {
            LOGIT_ERROR("Failed to extract hidden input fragment.");
            return std::nullopt;
        }
        if (utils::extract_between(fragment, "id=\"", "\"", req_id) == std::string::npos) {
            LOGIT_ERROR("Failed to extract request ID.");
            return std::nullopt;
        }
        if (utils::extract_after(fragment, "value=\"", req_value) == std::string::npos) {
            LOGIT_ERROR("Failed to extract request value.");
            return std::nullopt;
        }

        // Parse cookies
        kurlyk::Cookies set_cookie;
        for (const auto& item : headers) {
            if (item.first == "set-cookie") {
                kurlyk::Cookies cookie(kurlyk::utils::parse_cookie(item.second));
                set_cookie.insert(cookie.begin(), cookie.end());
            }
        }
        cookies = kurlyk::utils::to_cookie_string(set_cookie);

        return std::make_tuple(req_id, req_value, cookies);
    }

    namespace detail {

        inline std::optional<int64_t> parse_active_trade_close_time_ms(
                const std::string& row,
                int64_t trade_id) {
            std::smatch match;
            static const std::regex close_time_regex(
                R"(setInterval\s*\(\s*showRemaining[^;]*'([0-9]+)'\s*\))");
            if (std::regex_search(row, match, close_time_regex) && match.size() >= 2) {
                if (auto close_time = utils::parse_i64_strict(match[1].str())) {
                    return time_shield::sec_to_ms(*close_time);
                }
                return std::nullopt;
            }

            const std::regex remaining_regex(
                "time_time_" + std::to_string(trade_id) + R"(\s*=\s*([0-9]+)\s*(?:;|$))");
            if (std::regex_search(row, match, remaining_regex) && match.size() >= 2) {
                if (auto remaining_sec = utils::parse_i64_strict(match[1].str())) {
                    return OPTIONX_TIMESTAMP_MS + time_shield::sec_to_ms(*remaining_sec);
                }
                return std::nullopt;
            }

            return std::nullopt;
        }

        inline void log_trade_open_response(
                const std::string& content,
                const std::string& reason) {
#           ifdef OPTIONX_LOG_UNIQUE_FILE_INDEX
            const int log_index = OPTIONX_LOG_UNIQUE_FILE_INDEX;
            LOGIT_STREAM_ERROR_TO(log_index) << content;
            LOGIT_PRINT_ERROR(
                reason,
                " Content log was written to file: ",
                LOGIT_GET_LAST_FILE_NAME(log_index)
            );
#           else
            LOGIT_PRINT_ERROR(reason);
#           endif
        }

        inline std::string empty_trade_open_response_error() {
            return "Trade open failed. Server returned an empty response; instrument may be closed or unavailable.";
        }

    } // namespace detail

    /// \brief Parses active trades from the authenticated main page.
    /// \param content Raw HTML of the authenticated main page.
    /// \return Active trades found in the trade_active block.
    std::vector<ActiveTradeInfo> parse_active_trades_snapshot(const std::string& content) {
        std::vector<ActiveTradeInfo> trades;

        std::string active_html;
        const std::size_t active_id = content.find("id=\"trade_active\"");
        if (active_id == std::string::npos) {
            throw std::runtime_error("Authenticated active trades block not found.");
        }
        std::size_t block_start = content.rfind("<tbody", active_id);
        if (block_start == std::string::npos) block_start = active_id;
        std::size_t block_end = content.find("</tbody>", active_id);
        if (block_end == std::string::npos) block_end = content.size();
        active_html = content.substr(block_start, block_end - block_start);

        std::size_t pos = 0;
        for (;;) {
            const std::size_t row_start = active_html.find("<tr id=\"trade_inv_", pos);
            if (row_start == std::string::npos) break;

            const std::size_t row_end = active_html.find("</tr>", row_start);
            if (row_end == std::string::npos) break;
            const std::string row = active_html.substr(row_start, row_end - row_start);
            pos = row_end + 5;

            auto id = utils::parse_i64_attr(row, "data-id");
            if (!id || *id <= 0) continue;

            ActiveTradeInfo trade;
            trade.id = *id;
            if (auto symbol = utils::extract_html_attr(row, "data-option")) {
                trade.symbol = *symbol;
            }
            if (auto open_price = utils::parse_double_attr(row, "data-rate")) {
                trade.open_price = *open_price;
            }
            if (auto open_time = utils::parse_i64_attr(row, "data-timeopen")) {
                trade.open_time_ms = time_shield::sec_to_ms(*open_time);
            }
            if (auto status = utils::parse_int_attr(row, "data-status")) {
                trade.status = *status;
            }
            if (auto contract = utils::parse_int_attr(row, "data-contract")) {
                trade.contract = *contract;
            }
            if (auto close_time = detail::parse_active_trade_close_time_ms(row, trade.id)) {
                trade.close_time_ms = *close_time;
            }

            trades.push_back(std::move(trade));
        }

        return trades;
    }

    /// \brief Extracts user_id and user_hash from a cookies string.
    /// \param cookies The input string containing cookies.
    /// \param user_id [out] The extracted user_id.
    /// \param user_hash [out] The extracted user_hash.
    /// \return True if both user_id and user_hash are found, otherwise false.
    bool parse_cookies(const std::string& cookies, std::string& user_id, std::string& user_hash) {
        std::unordered_map<std::string, std::string> cookie_map;
        std::istringstream cookie_stream(cookies);
        std::string cookie;

        // Parse the cookies string
        while (std::getline(cookie_stream, cookie, ';')) {
            size_t pos = cookie.find('=');
            if (pos != std::string::npos) {
                std::string key = cookie.substr(0, pos);
                std::string value = cookie.substr(pos + 1);

                // Trim leading and trailing whitespace
                key.erase(0, key.find_first_not_of(" \t"));
                key.erase(key.find_last_not_of(" \t") + 1);
                value.erase(0, value.find_first_not_of(" \t"));
                value.erase(value.find_last_not_of(" \t") + 1);

                cookie_map[key] = value;
            }
        }

        // Extract user_id and user_hash
        auto id_it = cookie_map.find("user_id");
        auto hash_it = cookie_map.find("user_hash");

        if (id_it != cookie_map.end() && hash_it != cookie_map.end()) {
            user_id = id_it->second;
            user_hash = hash_it->second;
            return true;
        }

        return false;
    }

    std::optional<std::tuple<std::string, std::string>> parse_cookies(const std::string& cookies) {
        std::unordered_map<std::string, std::string> cookie_map;
        std::istringstream cookie_stream(cookies);
        std::string cookie;

        // Parse the cookies string
        while (std::getline(cookie_stream, cookie, ';')) {
            size_t pos = cookie.find('=');
            if (pos != std::string::npos) {
                std::string key = cookie.substr(0, pos);
                std::string value = cookie.substr(pos + 1);

                // Trim leading and trailing whitespace
                key.erase(0, key.find_first_not_of(" \t"));
                key.erase(key.find_last_not_of(" \t") + 1);
                value.erase(0, value.find_first_not_of(" \t"));
                value.erase(value.find_last_not_of(" \t") + 1);

                cookie_map[key] = value;
            }
        }

        // Extract user_id and user_hash
        auto id_it = cookie_map.find("user_id");
        auto hash_it = cookie_map.find("user_hash");

        if (id_it != cookie_map.end() && hash_it != cookie_map.end()) {
            return std::make_tuple(id_it->second, hash_it->second);
        }

        return std::nullopt;
    }

    /// \brief Parses the response content of a trade open request.
    /// \param content The server response content as a string.
    /// \param request A shared pointer to the TradeRequest containing request details.
    /// \param result A shared pointer to the TradeResult where the parsed result will be stored.
    /// \return True if the response was successfully parsed, false otherwise.
    bool parse_execute_trade(
            const std::string& content,
            std::shared_ptr<TradeRequest> request,
            std::shared_ptr<TradeResult> result) {
        if (!result || !request) {
            LOGIT_ERROR("TradeResult or TradeRequest is null.");
            return false;
        }
        const int64_t timestamp = OPTIONX_TIMESTAMP_MS;
        try {
            if (utils::is_blank_response(content)) {
                const std::string error_desc = detail::empty_trade_open_response_error();
                detail::log_trade_open_response(content, error_desc);
                result->trade_state = result->live_state = TradeState::OPEN_ERROR;
                result->error_code = TradeErrorCode::PARSING_ERROR;
                result->error_desc = error_desc;
                result->delay = timestamp - result->send_date;
                result->ping = result->delay / 2;
                result->open_date = timestamp;
                result->close_date = request->option_type == OptionType::SPRINT
                                     ? timestamp + time_shield::sec_to_ms(request->duration)
                                     : time_shield::sec_to_ms(request->expiry_time);
                return false;
            }

            // Check for error indicators in the response
            if (content.find("error") != std::string::npos) {
#               ifdef OPTIONX_LOG_UNIQUE_FILE_INDEX
                const int log_index = OPTIONX_LOG_UNIQUE_FILE_INDEX;
                LOGIT_STREAM_ERROR_TO(log_index) << content;
                LOGIT_PRINT_ERROR(
                    "Trade open failed. Response contains 'error'. Content log was written to file: ",
                    LOGIT_GET_LAST_FILE_NAME(log_index)
                );
#               else
                LOGIT_PRINT_ERROR("Trade open failed. Response contains 'error'.");
#               endif
                result->trade_state = result->live_state = TradeState::OPEN_ERROR;
                result->error_code = TradeErrorCode::PARSING_ERROR;
                result->error_desc = "Trade open failed. Response contains 'error'.";
                result->delay = timestamp - result->send_date;
                result->ping = result->delay / 2;
                result->open_date = timestamp;
                result->close_date = request->option_type == OptionType::SPRINT
                                     ? timestamp + time_shield::sec_to_ms(request->duration)
                                     : time_shield::sec_to_ms(request->expiry_time);
                return false;
            }

            if (content.find("alert") != std::string::npos) {
#               ifdef OPTIONX_LOG_UNIQUE_FILE_INDEX
                const int log_index = OPTIONX_LOG_UNIQUE_FILE_INDEX;
                LOGIT_STREAM_ERROR_TO(log_index) << content;
                LOGIT_PRINT_ERROR(
                    "Trade open failed. Response contains 'alert'. Content log was written to file: ",
                    LOGIT_GET_LAST_FILE_NAME(log_index)
                );
#               else
                LOGIT_PRINT_ERROR("Trade open failed. Response contains 'alert'.");
#               endif
                result->trade_state = result->live_state = TradeState::OPEN_ERROR;
                result->error_code = TradeErrorCode::PARSING_ERROR;
                result->error_desc = "Trade open failed. Response contains 'alert'.";
                result->delay = timestamp - result->send_date;
                result->ping = result->delay / 2;
                result->open_date = timestamp;
                result->close_date = request->option_type == OptionType::SPRINT
                                     ? timestamp + time_shield::sec_to_ms(request->duration)
                                     : time_shield::sec_to_ms(request->expiry_time);
                return false;
            }

            // Extract data fields
            std::string str_data_id, str_data_timeopen, str_data_rate;

            if (utils::extract_between(content, "data-id=\"", "\"", str_data_id) == std::string::npos) {
                detail::log_trade_open_response(content, "Failed to extract id from trade open response.");
                result->trade_state = result->live_state = TradeState::OPEN_ERROR;
                result->error_code = TradeErrorCode::PARSING_ERROR;
                result->error_desc = "Failed to extract id.";
                result->delay = timestamp - result->send_date;
                result->ping = result->delay / 2;
                result->open_date = timestamp;
                result->close_date = request->option_type == OptionType::SPRINT
                                     ? timestamp + time_shield::sec_to_ms(request->duration)
                                     : time_shield::sec_to_ms(request->expiry_time);
                return false;
            }
            result->option_id = std::stoull(str_data_id);

            if (utils::extract_between(content, "data-timeopen=\"", "\"", str_data_timeopen) == std::string::npos) {
                detail::log_trade_open_response(content, "Failed to extract timeopen from trade open response.");
                result->trade_state = result->live_state = TradeState::OPEN_ERROR;
                result->error_code = TradeErrorCode::PARSING_ERROR;
                result->error_desc = "Failed to extract timeopen.";
                result->delay = timestamp - result->send_date;
                result->ping = result->delay / 2;
                result->open_date = timestamp;
                result->close_date = request->option_type == OptionType::SPRINT
                                     ? timestamp + time_shield::sec_to_ms(request->duration)
                                     : time_shield::sec_to_ms(request->expiry_time);
                return false;
            }
            result->open_date = time_shield::sec_to_ms(std::stoull(str_data_timeopen));
            result->delay = timestamp - result->open_date;
            result->ping = result->delay / 2;

            if (utils::extract_between(content, "data-rate=\"", "\"", str_data_rate) == std::string::npos) {
                detail::log_trade_open_response(content, "Failed to extract rate from trade open response.");
                result->trade_state = result->live_state = TradeState::OPEN_ERROR;
                result->error_code = TradeErrorCode::PARSING_ERROR;
                result->error_desc = "Failed to extract rate.";
                result->open_date = timestamp;
                result->close_date = request->option_type == OptionType::SPRINT
                 ? timestamp + time_shield::sec_to_ms(request->duration)
                 : time_shield::sec_to_ms(request->expiry_time);
                return false;
            }

            // Parse extracted values
            result->open_price = result->close_price = std::stod(str_data_rate);
            result->trade_state = result->live_state = TradeState::OPEN_SUCCESS;
            result->error_code = TradeErrorCode::SUCCESS;
            return true;

        } catch (const std::exception& ex) {
            LOGIT_PRINT_ERROR("Exception while parsing trade response: ", ex.what());
            result->trade_state = result->live_state = TradeState::OPEN_ERROR;
            result->error_code = TradeErrorCode::PARSING_ERROR;
            result->error_desc = "Exception while parsing trade response: " + std::string(ex.what());
            result->delay = timestamp - result->open_date;
            result->ping = result->delay / 2;
            result->open_date = timestamp;
            result->close_date = request->option_type == OptionType::SPRINT
             ? timestamp + time_shield::sec_to_ms(request->duration)
             : time_shield::sec_to_ms(request->expiry_time);
        }
        return false;
    }

    /// \brief Parses the response content of a trade open request.
    /// \param content The server response content as a string.
    /// \param status_code The HTTP status code returned by the server.
    /// \param result_callback A callback function that receives the parsing result:
    ///        - success: True if the response was successfully parsed, false otherwise.
    ///        - status_code: The HTTP status code returned by the server.
    ///        - option_id: The option ID extracted from the response.
    ///        - open_date: The trade open date as a timestamp.
    ///        - open_price: The trade open price.
    ///        - error_desc: A description of the error if parsing fails.
    void parse_execute_trade(
            const std::string& content,
            long status_code,
            std::function<void(
                bool success,
                long status_code,
                int64_t option_id,
                int64_t open_date,
                double open_price,
                const std::string& error_desc)> result_callback) {
        try {
            if (utils::is_blank_response(content)) {
                const std::string error_desc = detail::empty_trade_open_response_error();
                detail::log_trade_open_response(content, error_desc);
                result_callback(false, status_code, 0, 0, 0, error_desc);
                return;
            }

            // Check for error indicators in the response
            if (content.find("error") != std::string::npos) {
#               ifdef OPTIONX_LOG_UNIQUE_FILE_INDEX
                const int log_index = OPTIONX_LOG_UNIQUE_FILE_INDEX;
                LOGIT_STREAM_ERROR_TO(log_index) << content;
                LOGIT_PRINT_ERROR(
                    "Trade open failed. Response contains 'error'. Content log was written to file: ",
                    LOGIT_GET_LAST_FILE_NAME(log_index)
                );
#               else
                LOGIT_PRINT_ERROR("Trade open failed. Response contains 'error'.");
#               endif
                result_callback(false, status_code,
                    0, 0, 0, "Trade open failed. Response contains 'error'.");
                return;
            }

            if (content.find("alert") != std::string::npos) {
#               ifdef OPTIONX_LOG_UNIQUE_FILE_INDEX
                const int log_index = OPTIONX_LOG_UNIQUE_FILE_INDEX;
                LOGIT_STREAM_ERROR_TO(log_index) << content;
                LOGIT_PRINT_ERROR(
                    "Trade open failed. Response contains 'alert'. Content log was written to file: ",
                    LOGIT_GET_LAST_FILE_NAME(log_index)
                );
#               else
                LOGIT_PRINT_ERROR("Trade open failed. Response contains 'alert'.");
#               endif
                result_callback(false, status_code,
                    0, 0, 0, "Trade open failed. Response contains 'alert'.");
                return;
            }

            // Extract data fields
            std::string str_data_id, str_data_timeopen, str_data_rate;

            if (utils::extract_between(content, "data-id=\"", "\"", str_data_id) == std::string::npos) {
                detail::log_trade_open_response(content, "Failed to extract id from trade open response.");
                result_callback(false, status_code,
                    0, 0, 0, "Failed to extract id.");
                return;
            }

            if (utils::extract_between(content, "data-timeopen=\"", "\"", str_data_timeopen) == std::string::npos) {
                detail::log_trade_open_response(content, "Failed to extract timeopen from trade open response.");
                result_callback(false, status_code,
                    0, 0, 0, "Failed to extract timeopen.");
                return;
            }

            if (utils::extract_between(content, "data-rate=\"", "\"", str_data_rate) == std::string::npos) {
                detail::log_trade_open_response(content, "Failed to extract rate from trade open response.");
                result_callback(false, status_code,
                    0, 0, 0, "Failed to extract rate.");
                return;
            }

            result_callback(
                true,
                status_code,
                std::stoll(str_data_id),
                time_shield::sec_to_ms(std::stoll(str_data_timeopen)),
                std::stod(str_data_rate),
                std::string());
        } catch (const std::exception& ex) {
            LOGIT_ERROR("Exception while parsing trade response: ", ex.what());
            result_callback(false, status_code,
                0, 0, 0, "Exception while parsing trade response: " + std::string(ex.what()));
        }
    }

    /// \brief Parses a BTCUSDT tick message from the WebSocket and updates the provided TickData structure.
    /// \param message The JSON-formatted string containing the tick data.
    /// \param tick_data Reference to the TickData structure to be updated.
    /// \return true if parsing is successful and the symbol matches BTCUSDT; false otherwise.
    bool parse_btcusdt_tick(const std::string& message, TickData& tick_data) {
        auto j = nlohmann::json::parse(message);

        if (j.contains("data")) {
            const auto& j_data = j["data"];
            if (j_data.value("s", "") != "BTCUSDT") return false;
            tick_data.tick.ask = tick_data.tick.bid = std::stod(j_data.value("p", "0.0"));
            tick_data.tick.volume = std::stod(j_data.value("q", "0.0"));
            tick_data.tick.time_ms = j_data.value("T", 0);
            tick_data.tick.received_ms = OPTIONX_TIMESTAMP_MS;
            tick_data.tick.set_flag(TickUpdateFlags::ASK_UPDATED);
            tick_data.tick.set_flag(TickUpdateFlags::BID_UPDATED);
            tick_data.tick.set_flag(TickUpdateFlags::VOLUME_UPDATED);
            tick_data.set_flag(TickStatusFlags::INITIALIZED);
            tick_data.set_flag(TickStatusFlags::REALTIME);
            return true;
        }

        return false;
    }

}

#endif // _OPTIONX_PLATFORMS_INTRADERBAR_HTTP_CLIENT_MODULE_STRING_PARSERS_HPP_INCLUDED
