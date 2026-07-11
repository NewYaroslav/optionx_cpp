#pragma once
#ifndef OPTIONX_HEADER_BRIDGES_TRADING_VIEW_DETAIL_TRADING_VIEW_EXTENSION_PROTOCOL_HPP_INCLUDED
#define OPTIONX_HEADER_BRIDGES_TRADING_VIEW_DETAIL_TRADING_VIEW_EXTENSION_PROTOCOL_HPP_INCLUDED

/// \file TradingViewExtensionProtocol.hpp
/// \brief Parses TradingView browser-extension messages into TradeSignal objects.

#include <algorithm>
#include <cctype>
#include <ctime>
#include <cstdint>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "utils/unicode_case.hpp"

namespace optionx::bridges::tradingview::detail {

    /// \struct TradingViewParseResult
    /// \brief Result of converting a TradingView extension payload to a trade signal.
    struct TradingViewParseResult {
        bool accepted = false;       ///< True when `signal` is populated.
        bool authorized = true;      ///< False when the payload secret is invalid.
        std::string reason;          ///< Machine-readable rejection or acceptance reason.
        std::string event_id;        ///< External event ID when available.
        std::string dedupe_key;      ///< Stable duplicate-detection key.
        nlohmann::json response;     ///< Compact HTTP/API response body.
        nlohmann::json raw_payload;  ///< Sanitized original payload.
        nlohmann::json parsed_payload; ///< Normalized parsed event fields.
        std::unique_ptr<TradeSignal> signal; ///< Parsed trade signal.
    };

    namespace protocol {

        struct NormalizedEvent {
            std::string source_kind = "tradingview_extension";
            std::string method;
            std::string fire_id;
            std::string alert_id;
            std::string event_id;
            std::string dedupe_key;
            std::string fingerprint;
            std::string symbol;
            std::string original_symbol;
            std::string action;
            std::string condition_type;
            std::string signal_name;
            std::string alert_name;
            std::string message;
            double price = 0.0;
            std::int64_t time = 0;
            bool is_level_alert = false;
            nlohmann::json raw;
        };

        inline std::string lower_copy(std::string value) {
            std::transform(
                value.begin(),
                value.end(),
                value.begin(),
                [](unsigned char ch) {
                    return static_cast<char>(std::tolower(ch));
                });
            return value;
        }

        inline std::string trim_copy(const std::string& value) {
            const auto first = value.find_first_not_of(" \t\r\n");
            if (first == std::string::npos) {
                return {};
            }
            const auto last = value.find_last_not_of(" \t\r\n");
            return value.substr(first, last - first + 1);
        }

        inline bool strict_number_string_to_double(
                const std::string& value,
                double& output) {
            auto text = trim_copy(value);
            if (text.empty()) {
                return false;
            }
            text.erase(
                std::remove(text.begin(), text.end(), ','),
                text.end());

            std::size_t position = 0;
            try {
                output = std::stod(text, &position);
            } catch (const std::exception&) {
                return false;
            }
            return position == text.size();
        }

        inline bool strict_integer_string_to_i64(
                const std::string& value,
                std::int64_t& output) {
            auto text = trim_copy(value);
            if (text.empty()) {
                return false;
            }
            text.erase(
                std::remove(text.begin(), text.end(), ','),
                text.end());

            const auto first_digit =
                (!text.empty() && (text.front() == '-' || text.front() == '+')) ? 1u : 0u;
            if (first_digit >= text.size()) {
                return false;
            }
            for (std::size_t index = first_digit; index < text.size(); ++index) {
                if (!std::isdigit(static_cast<unsigned char>(text[index]))) {
                    return false;
                }
            }

            std::size_t position = 0;
            try {
                output = std::stoll(text, &position);
            } catch (const std::exception&) {
                return false;
            }
            return position == text.size();
        }

        inline std::int64_t utc_timegm(std::tm& tm) {
#if defined(_WIN32)
            return static_cast<std::int64_t>(_mkgmtime(&tm));
#else
            return static_cast<std::int64_t>(timegm(&tm));
#endif
        }

        inline bool iso8601_utc_string_to_ms(
                const std::string& value,
                std::int64_t& output) {
            auto text = trim_copy(value);
            if (text.empty()) {
                return false;
            }
            if (!text.empty() && text.back() == 'Z') {
                text.pop_back();
            }

            std::int64_t millis = 0;
            const auto dot = text.find('.');
            if (dot != std::string::npos) {
                auto fraction = text.substr(dot + 1);
                text.erase(dot);
                if (fraction.size() > 3) {
                    fraction.resize(3);
                }
                while (fraction.size() < 3) {
                    fraction.push_back('0');
                }
                if (!fraction.empty()) {
                    std::int64_t parsed_fraction = 0;
                    if (!strict_integer_string_to_i64(fraction, parsed_fraction)) {
                        return false;
                    }
                    millis = parsed_fraction;
                }
            }

            std::tm tm{};
            std::istringstream input(text);
            input >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
            if (input.fail()) {
                input.clear();
                input.str(text);
                input >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
            }
            if (input.fail()) {
                return false;
            }

            const auto seconds = utc_timegm(tm);
            if (seconds < 0) {
                return false;
            }
            output = seconds * 1000 + millis;
            return true;
        }

        inline bool is_ascii_word_char(char ch) {
            const auto value = static_cast<unsigned char>(ch);
            return std::isalnum(value) != 0 || ch == '_';
        }

        inline bool is_ascii_text(const std::string& value) {
            return std::all_of(
                value.begin(),
                value.end(),
                [](unsigned char ch) {
                    return ch < 0x80;
                });
        }

        inline bool folded_text_contains_keyword(
                const std::string& folded_text,
                const std::string& keyword) {
            const auto folded_keyword = utils::unicode_case_fold(trim_copy(keyword));
            if (folded_keyword.empty()) {
                return false;
            }

            const bool ascii_keyword = is_ascii_text(folded_keyword);
            std::size_t position = 0;
            while ((position = folded_text.find(folded_keyword, position)) != std::string::npos) {
                if (!ascii_keyword) {
                    return true;
                }

                const bool left_ok =
                    position == 0 ||
                    !is_ascii_word_char(folded_text[position - 1]);
                const auto right_index = position + folded_keyword.size();
                const bool right_ok =
                    right_index >= folded_text.size() ||
                    !is_ascii_word_char(folded_text[right_index]);
                if (left_ok && right_ok) {
                    return true;
                }
                ++position;
            }
            return false;
        }

        inline std::vector<std::string> effective_buy_action_keywords(
                const TradingViewExtensionBridgeConfig& config) {
            std::vector<std::string> keywords;
            if (config.use_default_action_keywords) {
                const auto& defaults =
                    TradingViewExtensionBridgeConfig::default_buy_action_keywords();
                keywords.insert(keywords.end(), defaults.begin(), defaults.end());
            }
            keywords.insert(
                keywords.end(),
                config.buy_action_keywords.begin(),
                config.buy_action_keywords.end());
            return keywords;
        }

        inline std::vector<std::string> effective_sell_action_keywords(
                const TradingViewExtensionBridgeConfig& config) {
            std::vector<std::string> keywords;
            if (config.use_default_action_keywords) {
                const auto& defaults =
                    TradingViewExtensionBridgeConfig::default_sell_action_keywords();
                keywords.insert(keywords.end(), defaults.begin(), defaults.end());
            }
            keywords.insert(
                keywords.end(),
                config.sell_action_keywords.begin(),
                config.sell_action_keywords.end());
            return keywords;
        }

        inline bool contains_any_action_keyword(
                const std::string& text,
                const std::vector<std::string>& keywords) {
            const auto folded_text = utils::unicode_case_fold(text);
            return std::any_of(
                keywords.begin(),
                keywords.end(),
                [&folded_text](const std::string& keyword) {
                    return folded_text_contains_keyword(folded_text, keyword);
                });
        }

        inline std::string scalar_to_string(const nlohmann::json& value) {
            if (value.is_string()) {
                return value.get<std::string>();
            }
            if (value.is_number_integer()) {
                return std::to_string(value.get<std::int64_t>());
            }
            if (value.is_number_unsigned()) {
                return std::to_string(value.get<std::uint64_t>());
            }
            if (value.is_number_float()) {
                std::ostringstream out;
                out << value.get<double>();
                return out.str();
            }
            if (value.is_boolean()) {
                return value.get<bool>() ? "true" : "false";
            }
            return {};
        }

        inline std::string json_string(const nlohmann::json& object, const char* key) {
            if (!object.is_object() || !object.contains(key)) {
                return {};
            }
            return scalar_to_string(object.at(key));
        }

        inline std::string first_json_string(
                const nlohmann::json& object,
                std::initializer_list<const char*> keys) {
            for (const auto* key : keys) {
                auto value = json_string(object, key);
                if (!value.empty()) {
                    return value;
                }
            }
            return {};
        }

        inline double json_number(
                const nlohmann::json& object,
                std::initializer_list<const char*> keys,
                double fallback = 0.0) {
            if (!object.is_object()) {
                return fallback;
            }
            for (const auto* key : keys) {
                if (!object.contains(key)) {
                    continue;
                }
                const auto& value = object.at(key);
                if (value.is_number()) {
                    return value.get<double>();
                }
                if (value.is_string()) {
                    double parsed = 0.0;
                    if (strict_number_string_to_double(value.get<std::string>(), parsed)) {
                        return parsed;
                    }
                }
            }
            return fallback;
        }

        inline std::int64_t json_integer(
                const nlohmann::json& object,
                std::initializer_list<const char*> keys,
                std::int64_t fallback = 0) {
            if (!object.is_object()) {
                return fallback;
            }
            for (const auto* key : keys) {
                if (!object.contains(key)) {
                    continue;
                }
                const auto& value = object.at(key);
                if (value.is_number_integer()) {
                    return value.get<std::int64_t>();
                }
                if (value.is_number_unsigned()) {
                    return static_cast<std::int64_t>(value.get<std::uint64_t>());
                }
                if (value.is_number_float()) {
                    return static_cast<std::int64_t>(value.get<double>());
                }
                if (value.is_string()) {
                    std::int64_t parsed = 0;
                    const auto text = value.get<std::string>();
                    if (strict_integer_string_to_i64(text, parsed) ||
                        iso8601_utc_string_to_ms(text, parsed)) {
                        return parsed;
                    }
                }
            }
            return fallback;
        }

        inline bool is_action_buy_sell(const std::string& action) {
            const auto normalized = lower_copy(trim_copy(action));
            return normalized == "buy" || normalized == "sell";
        }

        inline OrderType order_type_from_action(const std::string& action) {
            const auto normalized = lower_copy(trim_copy(action));
            if (normalized == "buy") {
                return OrderType::BUY;
            }
            if (normalized == "sell") {
                return OrderType::SELL;
            }
            return OrderType::UNKNOWN;
        }

        inline OrderType order_type_from_action_keywords(
                const TradingViewExtensionBridgeConfig& config,
                const NormalizedEvent& event) {
            std::string text = event.message;
            if (!event.signal_name.empty()) {
                text += ' ';
                text += event.signal_name;
            }
            if (!event.alert_name.empty()) {
                text += ' ';
                text += event.alert_name;
            }
            if (!event.action.empty()) {
                text += ' ';
                text += event.action;
            }

            const auto buy_keywords = effective_buy_action_keywords(config);
            const auto sell_keywords = effective_sell_action_keywords(config);
            const bool has_buy =
                contains_any_action_keyword(text, buy_keywords);
            const bool has_sell =
                contains_any_action_keyword(text, sell_keywords);

            if (has_buy && !has_sell) {
                return OrderType::BUY;
            }
            if (has_sell && !has_buy) {
                return OrderType::SELL;
            }
            return OrderType::UNKNOWN;
        }

        inline std::string normalize_condition_type(std::string value) {
            value = lower_copy(trim_copy(std::move(value)));
            for (auto& ch : value) {
                if (ch == ' ' || ch == '-' || ch == '.') {
                    ch = '_';
                }
            }
            if (value == "cross_up" || value == "crossingup") {
                return "crossing_up";
            }
            if (value == "cross_down" || value == "crossingdown") {
                return "crossing_down";
            }
            if (value == "cross") {
                return "crossing";
            }
            return value;
        }

        inline std::string infer_condition_type_from_message(const std::string& message) {
            const auto text = lower_copy(message);
            if (text.find("crossing up") != std::string::npos ||
                text.find("cross up") != std::string::npos) {
                return "crossing_up";
            }
            if (text.find("crossing down") != std::string::npos ||
                text.find("cross down") != std::string::npos) {
                return "crossing_down";
            }
            if (text.find("crossing") != std::string::npos ||
                text.find("cross ") != std::string::npos) {
                return "crossing";
            }
            if (text.find("greater than") != std::string::npos ||
                text.find("above") != std::string::npos) {
                return "greater_than";
            }
            if (text.find("less than") != std::string::npos ||
                text.find("below") != std::string::npos) {
                return "less_than";
            }
            return {};
        }

        inline std::string normalize_symbol_value(std::string symbol) {
            symbol = trim_copy(symbol);
            if (symbol.size() > 1 && symbol.front() == '=') {
                try {
                    const auto parsed = nlohmann::json::parse(symbol.substr(1));
                    const auto parsed_symbol =
                        first_json_string(parsed, {"symbol", "tickerid", "ticker"});
                    if (!parsed_symbol.empty()) {
                        return parsed_symbol;
                    }
                } catch (const std::exception&) {
                }
            }
            return symbol;
        }

        inline std::string apply_symbol_map(
                const TradingViewExtensionBridgeConfig& config,
                const std::string& symbol) {
            const auto normalized = normalize_symbol_value(symbol);
            auto it = config.symbol_map.find(normalized);
            if (it != config.symbol_map.end()) {
                return it->second;
            }

            const auto colon = normalized.find(':');
            if (colon != std::string::npos && colon + 1 < normalized.size()) {
                const auto suffix = normalized.substr(colon + 1);
                it = config.symbol_map.find(suffix);
                if (it != config.symbol_map.end()) {
                    return it->second;
                }
            }
            return normalized;
        }

        inline std::string payload_secret(const nlohmann::json& payload) {
            auto secret = json_string(payload, "secret");
            if (!secret.empty()) {
                return secret;
            }
            if (payload.is_object() && payload.contains("auth")) {
                return json_string(payload.at("auth"), "secret");
            }
            return {};
        }

        inline bool constant_time_equals(
                const std::string& left,
                const std::string& right) noexcept {
            const auto max_size = std::max(left.size(), right.size());
            volatile std::uint8_t diff =
                static_cast<std::uint8_t>(left.size() ^ right.size());

            for (std::size_t index = 0; index < max_size; ++index) {
                const auto left_ch = index < left.size()
                    ? static_cast<std::uint8_t>(left[index])
                    : std::uint8_t{0};
                const auto right_ch = index < right.size()
                    ? static_cast<std::uint8_t>(right[index])
                    : std::uint8_t{0};
                diff = static_cast<std::uint8_t>(diff | (left_ch ^ right_ch));
            }

            return diff == 0;
        }

        inline nlohmann::json redact_payload_secrets(nlohmann::json value) {
            if (value.is_object()) {
                for (auto it = value.begin(); it != value.end(); ++it) {
                    const auto key = lower_copy(it.key());
                    if (key == "secret" || key == "authorization" ||
                        key.find("secret") != std::string::npos) {
                        it.value() = "[redacted]";
                    } else {
                        it.value() = redact_payload_secrets(it.value());
                    }
                }
            } else if (value.is_array()) {
                for (auto& item : value) {
                    item = redact_payload_secrets(std::move(item));
                }
            }
            return value;
        }

        inline const nlohmann::json& effective_payload(const nlohmann::json& payload) {
            if (payload.is_object() &&
                payload.contains("payload") &&
                payload.at("payload").is_object()) {
                return payload.at("payload");
            }
            return payload;
        }

        inline std::string make_event_key(const NormalizedEvent& event) {
            if (!event.dedupe_key.empty()) {
                return event.dedupe_key;
            }
            if (!event.event_id.empty()) {
                return event.event_id;
            }
            if (!event.fingerprint.empty()) {
                return event.fingerprint;
            }
            if (!event.fire_id.empty()) {
                return event.source_kind + ":fire:" + event.fire_id;
            }
            if (!event.alert_id.empty() && !event.method.empty()) {
                return event.source_kind + ":" + event.method + ":" + event.alert_id;
            }
            return event.source_kind + ":" +
                   event.symbol + ":" +
                   lower_copy(event.action) + ":" +
                   event.condition_type + ":" +
                   std::to_string(event.time) + ":" +
                   event.message;
        }

        inline bool matches_rule(
                const TradingViewLevelAlertRule& rule,
                const NormalizedEvent& event) {
            if (!rule.alert_id.empty() &&
                rule.alert_id != event.alert_id &&
                rule.alert_id != event.fire_id) {
                return false;
            }
            if (!rule.symbol.empty() &&
                normalize_symbol_value(rule.symbol) != event.original_symbol &&
                normalize_symbol_value(rule.symbol) != event.symbol) {
                return false;
            }
            if (!rule.condition_type.empty() &&
                normalize_condition_type(rule.condition_type) != event.condition_type) {
                return false;
            }
            if (!rule.message_equals.empty() && rule.message_equals != event.message) {
                return false;
            }
            if (!rule.message_contains.empty() &&
                event.message.find(rule.message_contains) == std::string::npos) {
                return false;
            }
            return true;
        }

        inline const TradingViewLevelAlertRule* find_level_rule(
                const TradingViewExtensionBridgeConfig& config,
                const NormalizedEvent& event) {
            for (const auto& rule : config.level_alert_rules) {
                if (matches_rule(rule, event)) {
                    return &rule;
                }
            }
            return nullptr;
        }

        inline NormalizedEvent parse_pricealerts_private_feed(const nlohmann::json& payload) {
            NormalizedEvent event;
            event.source_kind = "tradingview_private_pricealerts_ws";
            event.is_level_alert = true;
            event.raw = payload;

            const auto& text = payload.at("text");
            const auto channel = json_string(text, "channel");
            const auto& content = text.at("content");
            event.method = json_string(content, "m");
            event.action = json_string(payload, "action");
            event.condition_type =
                normalize_condition_type(first_json_string(
                    payload,
                    {"condition_type", "condition"}));

            if (channel != "pricealerts") {
                throw std::invalid_argument("Unsupported TradingView private feed channel.");
            }
            if (event.method != "alert_fired") {
                throw std::runtime_error("TradingView price alert state message ignored.");
            }
            if (!content.contains("p") || !content.at("p").is_object()) {
                throw std::invalid_argument("TradingView alert_fired message does not contain object p.");
            }

            const auto& data = content.at("p");
            event.fire_id = json_string(data, "fire_id");
            event.alert_id = first_json_string(data, {"alert_id", "id"});
            if (event.alert_id.empty()) {
                event.alert_id = event.fire_id;
            }
            event.event_id = first_json_string(payload, {"event_id", "dedupe_key"});
            if (event.event_id.empty() && !event.fire_id.empty()) {
                event.event_id = event.source_kind + ":" + event.fire_id;
            } else if (event.event_id.empty() && !event.alert_id.empty()) {
                event.event_id = event.source_kind + ":" + event.alert_id;
            }
            event.dedupe_key = json_string(payload, "dedupe_key");
            event.fingerprint = json_string(payload, "fingerprint");
            event.original_symbol =
                normalize_symbol_value(first_json_string(
                    data,
                    {"symbol", "tickerid", "ticker", "main_symbol"}));
            event.symbol = event.original_symbol;
            event.signal_name = first_json_string(data, {"signal_name", "name", "alert_name"});
            event.alert_name = first_json_string(data, {"alert_name", "name", "title"});
            event.message = first_json_string(data, {"message", "description", "title"});
            if (event.message.empty()) {
                event.message = first_json_string(payload, {"message", "description", "title"});
            }
            event.price =
                json_number(data, {"price", "trigger_price", "cross_price", "alert_value"});
            event.time =
                json_integer(data, {"time", "timestamp", "fire_time", "fired_at", "update_time"});
            if (event.condition_type.empty()) {
                event.condition_type =
                    normalize_condition_type(first_json_string(
                        data,
                        {"condition_type", "condition", "type"}));
            }
            if (event.condition_type.empty()) {
                event.condition_type = infer_condition_type_from_message(event.message);
            }
            return event;
        }

        inline NormalizedEvent parse_normalized_extension_payload(const nlohmann::json& payload) {
            NormalizedEvent event;
            event.raw = payload;
            event.source_kind =
                first_json_string(payload, {"source_kind", "source", "origin"});
            if (event.source_kind.empty()) {
                event.source_kind = "tradingview_extension";
            }
            event.method = first_json_string(payload, {"method", "message_type", "event_type"});
            event.fire_id = json_string(payload, "fire_id");
            event.alert_id = first_json_string(payload, {"alert_id", "id"});
            if (event.alert_id.empty()) {
                event.alert_id = event.fire_id;
            }
            event.event_id = first_json_string(payload, {"event_id", "dedupe_key"});
            event.dedupe_key = json_string(payload, "dedupe_key");
            event.fingerprint = json_string(payload, "fingerprint");
            event.original_symbol =
                normalize_symbol_value(first_json_string(
                    payload,
                    {"symbol", "tickerid", "ticker", "main_symbol"}));
            event.symbol = event.original_symbol;
            event.action = first_json_string(payload, {"action", "side", "direction"});
            event.condition_type =
                normalize_condition_type(first_json_string(
                    payload,
                    {"condition_type", "condition", "crossing_type"}));
            event.signal_name = first_json_string(payload, {"signal_name", "strategy", "name"});
            event.alert_name = first_json_string(payload, {"alert_name", "alert_title"});
            event.message = first_json_string(payload, {"message", "description", "text", "title"});
            event.price = json_number(payload, {"price", "close", "trigger_price", "alert_value"});
            event.time = json_integer(payload, {"time", "timestamp", "bar_time", "fire_time"});

            if (payload.contains("barInfo") && payload.at("barInfo").is_object()) {
                const auto& bar = payload.at("barInfo");
                if (event.price == 0.0) {
                    event.price = json_number(bar, {"close"});
                }
                if (event.time == 0) {
                    event.time = json_integer(bar, {"time", "updateTime"});
                }
            }

            if (event.event_id.empty() && !event.fire_id.empty()) {
                event.event_id = event.source_kind + ":" + event.fire_id;
            } else if (event.event_id.empty() && !event.alert_id.empty()) {
                event.event_id = event.source_kind + ":" + event.alert_id;
            }
            const auto source_kind = lower_copy(event.source_kind);
            event.is_level_alert =
                source_kind.find("level") != std::string::npos ||
                source_kind.find("pricealert") != std::string::npos ||
                event.method == "alert_fired" ||
                lower_copy(event.action) == "alert";
            if (event.condition_type.empty()) {
                event.condition_type = infer_condition_type_from_message(event.message);
            }
            return event;
        }

        inline NormalizedEvent parse_event(const nlohmann::json& payload) {
            const auto& effective = effective_payload(payload);
            if (effective.is_object() &&
                effective.contains("text") &&
                effective.at("text").is_object() &&
                effective.at("text").contains("content")) {
                return parse_pricealerts_private_feed(effective);
            }
            return parse_normalized_extension_payload(effective);
        }

        inline nlohmann::json normalized_event_to_json(const NormalizedEvent& event) {
            return nlohmann::json{
                {"source_kind", event.source_kind},
                {"method", event.method},
                {"fire_id", event.fire_id},
                {"alert_id", event.alert_id},
                {"event_id", event.event_id},
                {"dedupe_key", event.dedupe_key},
                {"fingerprint", event.fingerprint},
                {"symbol", event.symbol},
                {"original_symbol", event.original_symbol},
                {"action", event.action},
                {"condition_type", event.condition_type},
                {"signal_name", event.signal_name},
                {"alert_name", event.alert_name},
                {"message", event.message},
                {"price", event.price},
                {"time", event.time},
                {"is_level_alert", event.is_level_alert}
            };
        }

        inline nlohmann::json make_response(
                bool accepted,
                const std::string& reason,
                const std::string& event_id,
                const std::string& dedupe_key) {
            return nlohmann::json{
                {"ok", accepted},
                {"accepted", accepted},
                {"reason", reason},
                {"event_id", event_id},
                {"dedupe_key", dedupe_key}
            };
        }

        inline std::int64_t unique_id_from_event_ids(
                const std::string& fire_id,
                const std::string& alert_id) {
            const auto id = fire_id.empty() ? alert_id : fire_id;
            if (id.empty()) {
                return 0;
            }
            try {
                return std::stoll(id);
            } catch (const std::exception&) {
                return 0;
            }
        }

        inline nlohmann::json sizing_metadata(
                const TradingViewExtensionBridgeConfig& config) {
            return nlohmann::json{
                {"mode", config.sizing_mode},
                {"fixed_amount", config.fixed_amount},
                {"balance_percent", config.balance_percent},
                {"min_amount", config.min_amount},
                {"max_amount", config.max_amount}
            };
        }

        inline void apply_sizing(
                const TradingViewExtensionBridgeConfig& config,
                TradeSignal& signal) {
            const auto sizing_mode = lower_copy(trim_copy(config.sizing_mode));
            if (sizing_mode == "fixed_amount") {
                signal.amount = config.fixed_amount;
            } else {
                signal.amount = 0.0;
            }

            if (sizing_mode == "balance_percent") {
                signal.mm_type = MmSystemType::PERCENT;
            } else if (sizing_mode == "fixed_amount") {
                signal.mm_type = MmSystemType::FIXED;
            } else {
                signal.mm_type = MmSystemType::NONE;
            }
        }

        inline std::unique_ptr<TradeSignal> build_signal(
                const TradingViewExtensionBridgeConfig& config,
                const NormalizedEvent& event,
                OrderType order_type,
                std::string signal_name) {
            auto signal = std::make_unique<TradeSignal>();
            signal->bridge_id = config.bridge_id;
            signal->unique_id = unique_id_from_event_ids(event.fire_id, event.alert_id);
            signal->unique_hash = make_event_key(event);
            signal->symbol = event.symbol.empty()
                ? apply_symbol_map(config, event.original_symbol)
                : event.symbol;
            signal->signal_name = signal_name.empty() ? event.signal_name : std::move(signal_name);
            if (signal->signal_name.empty() && !event.alert_name.empty()) {
                signal->signal_name = event.alert_name;
            }
            if (signal->signal_name.empty()) {
                signal->signal_name = event.is_level_alert
                    ? "tradingview_level_alert"
                    : "tradingview_signal";
            }
            signal->comment = event.message;
            signal->option_type = config.option_type;
            signal->order_type = order_type;
            signal->min_payout = config.min_payout;
            signal->duration = config.duration;
            apply_sizing(config, *signal);

            nlohmann::json user_data{
                {"source", "tradingview"},
                {"source_kind", event.source_kind},
                {"method", event.method},
                {"event_id", event.event_id},
                {"dedupe_key", signal->unique_hash},
                {"fingerprint", event.fingerprint},
                {"fire_id", event.fire_id},
                {"alert_id", event.alert_id},
                {"alert_name", event.alert_name},
                {"original_symbol", event.original_symbol},
                {"normalized_symbol", event.symbol},
                {"condition_type", event.condition_type},
                {"action", to_str(order_type, 1)},
                {"price", event.price},
                {"time", event.time},
                {"sizing", sizing_metadata(config)}
            };
            signal->user_data = user_data.dump();
            return signal;
        }

    } // namespace protocol

    /// \brief Parses a TradingView extension payload into a trade signal.
    /// \param payload JSON payload received from the browser extension.
    /// \param config Bridge configuration.
    /// \return Parse result with signal or rejection reason.
    inline TradingViewParseResult parse_extension_payload(
            const nlohmann::json& payload,
            const std::string& request_secret,
            const TradingViewExtensionBridgeConfig& config) {
        TradingViewParseResult result;
        result.raw_payload = protocol::redact_payload_secrets(payload);

        if (!config.secret.empty()) {
            auto actual_secret = request_secret;
            if (actual_secret.empty() && config.allow_body_secret_fallback) {
                actual_secret = protocol::payload_secret(payload);
                if (actual_secret.empty() &&
                    payload.is_object() &&
                    payload.contains("payload") &&
                    payload.at("payload").is_object()) {
                    actual_secret = protocol::payload_secret(payload.at("payload"));
                }
            }
            if (!protocol::constant_time_equals(actual_secret, config.secret)) {
                result.authorized = false;
                result.reason = "invalid_secret";
                result.response =
                    protocol::make_response(false, result.reason, {}, {});
                return result;
            }
        }

        protocol::NormalizedEvent event;
        try {
            event = protocol::parse_event(payload);
        } catch (const std::runtime_error& ex) {
            result.reason = "ignored_state_message";
            result.response = protocol::make_response(false, result.reason, {}, {});
            result.response["message"] = ex.what();
            return result;
        } catch (const std::exception& ex) {
            result.reason = "invalid_payload";
            result.response = protocol::make_response(false, result.reason, {}, {});
            result.response["message"] = ex.what();
            return result;
        }

        event.symbol = protocol::apply_symbol_map(config, event.symbol);
        event.dedupe_key = protocol::make_event_key(event);
        result.event_id = event.event_id;
        result.dedupe_key = event.dedupe_key;
        result.parsed_payload = protocol::normalized_event_to_json(event);

        auto order_type = protocol::order_type_from_action(event.action);
        std::string signal_name = event.signal_name;
        const auto keyword_order_type = order_type == OrderType::UNKNOWN
            ? protocol::order_type_from_action_keywords(config, event)
            : OrderType::UNKNOWN;

        if (order_type == OrderType::UNKNOWN && event.is_level_alert) {
            if (const auto* rule = protocol::find_level_rule(config, event)) {
                const auto action = protocol::lower_copy(protocol::trim_copy(rule->action));
                if (action == "reject" || action == "ignore") {
                    result.reason = "level_alert_rejected_by_rule";
                    result.response =
                        protocol::make_response(false, result.reason, result.event_id, result.dedupe_key);
                    return result;
                }
                order_type = protocol::order_type_from_action(action);
                if (!rule->signal_name.empty()) {
                    signal_name = rule->signal_name;
                }
            } else if (keyword_order_type != OrderType::UNKNOWN) {
                order_type = keyword_order_type;
            } else {
                const auto action =
                    protocol::lower_copy(protocol::trim_copy(config.default_level_action));
                if (action == "reject" || action == "ignore") {
                    result.reason = "unmapped_level_alert";
                    result.response =
                        protocol::make_response(false, result.reason, result.event_id, result.dedupe_key);
                    return result;
                }
                order_type = protocol::order_type_from_action(action);
            }
        }

        if (order_type == OrderType::UNKNOWN &&
            keyword_order_type != OrderType::UNKNOWN) {
            order_type = keyword_order_type;
        }

        if (order_type == OrderType::UNKNOWN) {
            result.reason = "unsupported_action";
            result.response =
                protocol::make_response(false, result.reason, result.event_id, result.dedupe_key);
            return result;
        }
        if (event.symbol.empty()) {
            result.reason = "missing_symbol";
            result.response =
                protocol::make_response(false, result.reason, result.event_id, result.dedupe_key);
            return result;
        }

        result.signal = protocol::build_signal(config, event, order_type, std::move(signal_name));
        result.accepted = true;
        result.reason = "accepted";
        result.response =
            protocol::make_response(true, result.reason, result.event_id, result.dedupe_key);
        return result;
    }

    /// \brief Parses a payload without an HTTP header secret.
    /// \details JSON body secret fallback is disabled unless the config opts in.
    inline TradingViewParseResult parse_extension_payload(
            const nlohmann::json& payload,
            const TradingViewExtensionBridgeConfig& config) {
        return parse_extension_payload(payload, std::string(), config);
    }

} // namespace optionx::bridges::tradingview::detail

#endif // OPTIONX_HEADER_BRIDGES_TRADING_VIEW_DETAIL_TRADING_VIEW_EXTENSION_PROTOCOL_HPP_INCLUDED
