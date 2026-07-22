#pragma once
#ifndef OPTIONX_HEADER_BRIDGES_PROTOCOL_V1_DETAIL_BRIDGE_PROTOCOL_CANONICALIZATION_HPP_INCLUDED
#define OPTIONX_HEADER_BRIDGES_PROTOCOL_V1_DETAIL_BRIDGE_PROTOCOL_CANONICALIZATION_HPP_INCLUDED

/// \file BridgeProtocolCanonicalization.hpp
/// \brief Canonicalizes Bridge Protocol v1 business payloads for idempotency.

namespace optionx::bridges::protocol_v1::detail {

    inline std::string trim_ascii_copy(std::string value) {
        auto first = value.begin();
        while (first != value.end() &&
               std::isspace(static_cast<unsigned char>(*first)) != 0) {
            ++first;
        }

        auto last = value.end();
        while (last != first &&
               std::isspace(static_cast<unsigned char>(*(last - 1))) != 0) {
            --last;
        }

        return std::string(first, last);
    }

    inline std::string upper_ascii_copy(std::string value) {
        std::transform(
            value.begin(),
            value.end(),
            value.begin(),
            [](const unsigned char ch) {
                return static_cast<char>(std::toupper(ch));
            });
        return value;
    }

    inline std::string lower_ascii_copy(std::string value) {
        std::transform(
            value.begin(),
            value.end(),
            value.begin(),
            [](const unsigned char ch) {
                return static_cast<char>(std::tolower(ch));
            });
        return value;
    }

    inline CurrencyType canonical_currency_value(const std::string& value) {
        if (value.empty()) {
            return CurrencyType::UNKNOWN;
        }
        return to_enum<CurrencyType>(value);
    }

    inline AccountType canonical_account_type_value(const std::string& value) {
        if (value.empty()) {
            return AccountType::UNKNOWN;
        }
        return to_enum<AccountType>(value);
    }

    inline PlatformType canonical_platform_type_value(const std::string& value) {
        if (value.empty()) {
            return PlatformType::UNKNOWN;
        }
        auto normalized = upper_ascii_copy(trim_ascii_copy(value));
        std::replace(normalized.begin(), normalized.end(), '.', '_');
        std::replace(normalized.begin(), normalized.end(), '-', '_');
        std::replace(normalized.begin(), normalized.end(), ' ', '_');
        return to_enum<PlatformType>(normalized);
    }

    inline OptionType canonical_option_type_value(const std::string& value) {
        if (value.empty()) {
            return OptionType::UNKNOWN;
        }
        return to_enum<OptionType>(value);
    }

    inline OrderType canonical_order_type_value(const std::string& value) {
        if (value.empty()) {
            return OrderType::UNKNOWN;
        }
        return to_enum<OrderType>(value);
    }

    /// \brief Converts string/number identity values into canonical strings.
    inline nlohmann::json canonical_identifier_value(const nlohmann::json& value) {
        if (value.is_string()) {
            return value.get<std::string>();
        }
        if (value.is_number_integer()) {
            return std::to_string(value.get<std::int64_t>());
        }
        if (value.is_number_unsigned()) {
            return std::to_string(value.get<std::uint64_t>());
        }
        return value;
    }

    /// \brief Normalizes decimal text by removing non-semantic scale.
    inline std::string canonical_decimal_text(std::string text) {
        text = trim_ascii_copy(std::move(text));
        if (text.empty()) {
            return text;
        }

        const auto original = text;
        bool negative = false;
        std::size_t pos = 0;
        if (text[pos] == '+' || text[pos] == '-') {
            negative = text[pos] == '-';
            ++pos;
        }

        std::string digits;
        std::int64_t scale = 0;
        bool saw_digit = false;
        bool saw_dot = false;
        for (; pos < text.size(); ++pos) {
            const auto ch = static_cast<unsigned char>(text[pos]);
            if (std::isdigit(ch) != 0) {
                digits.push_back(static_cast<char>(ch));
                saw_digit = true;
                if (saw_dot) {
                    ++scale;
                }
                continue;
            }
            if (text[pos] == '.' && !saw_dot) {
                saw_dot = true;
                continue;
            }
            break;
        }
        if (!saw_digit) {
            return original;
        }

        if (pos < text.size() && (text[pos] == 'e' || text[pos] == 'E')) {
            ++pos;
            bool exponent_negative = false;
            if (pos < text.size() && (text[pos] == '+' || text[pos] == '-')) {
                exponent_negative = text[pos] == '-';
                ++pos;
            }
            if (pos == text.size()) {
                return original;
            }
            std::int64_t exponent = 0;
            for (; pos < text.size(); ++pos) {
                const auto ch = static_cast<unsigned char>(text[pos]);
                if (std::isdigit(ch) == 0) {
                    return original;
                }
                if (exponent < 10000) {
                    exponent = exponent * 10 + (text[pos] - '0');
                } else {
                    return original;
                }
            }
            scale += exponent_negative ? exponent : -exponent;
        }
        if (pos != text.size()) {
            return original;
        }

        const auto first_non_zero = digits.find_first_not_of('0');
        if (first_non_zero == std::string::npos) {
            return "0";
        }
        digits.erase(0, first_non_zero);

        while (scale > 0 && !digits.empty() && digits.back() == '0') {
            digits.pop_back();
            --scale;
        }
        if (digits.empty()) {
            return "0";
        }

        std::string normalized;
        if (negative) {
            normalized.push_back('-');
        }
        if (scale <= 0) {
            normalized += digits;
            normalized.append(static_cast<std::size_t>(-scale), '0');
        } else if (static_cast<std::uint64_t>(scale) >= digits.size()) {
            normalized += "0.";
            normalized.append(
                static_cast<std::size_t>(scale) - digits.size(),
                '0');
            normalized += digits;
        } else {
            const auto split = digits.size() - static_cast<std::size_t>(scale);
            normalized += digits.substr(0, split);
            normalized.push_back('.');
            normalized += digits.substr(split);
        }
        return normalized;
    }

    /// \brief Converts string/number decimal values into canonical decimal text.
    inline nlohmann::json canonical_decimal_value(const nlohmann::json& value) {
        if (value.is_string()) {
            return canonical_decimal_text(value.get<std::string>());
        }
        if (value.is_number_integer()) {
            return std::to_string(value.get<std::int64_t>());
        }
        if (value.is_number_unsigned()) {
            return std::to_string(value.get<std::uint64_t>());
        }
        if (value.is_number_float()) {
            const auto numeric = value.get<double>();
            if (!std::isfinite(numeric)) {
                return value;
            }
            std::array<char, 64> buffer{};
            const auto converted = std::to_chars(
                buffer.data(),
                buffer.data() + buffer.size(),
                numeric,
                std::chars_format::general);
            if (converted.ec != std::errc{}) {
                return value;
            }
            return canonical_decimal_text(std::string(buffer.data(), converted.ptr));
        }
        return value;
    }

    /// \brief Converts string/number millisecond fields into canonical strings.
    inline nlohmann::json canonical_integer_text_value(const nlohmann::json& value) {
        if (value.is_string()) {
            return canonical_decimal_text(value.get<std::string>());
        }
        if (value.is_number_integer()) {
            return std::to_string(value.get<std::int64_t>());
        }
        if (value.is_number_unsigned()) {
            return std::to_string(value.get<std::uint64_t>());
        }
        return value;
    }

    /// \brief Converts second-based expiry aliases to millisecond text without throwing.
    inline nlohmann::json canonical_milliseconds_from_seconds_value(
            const nlohmann::json& value) {
        try {
            std::int64_t seconds = 0;
            if (value.is_number_integer()) {
                seconds = value.get<std::int64_t>();
            } else if (value.is_number_unsigned()) {
                const auto unsigned_seconds = value.get<std::uint64_t>();
                if (unsigned_seconds >
                    static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max() / 1000)) {
                    return canonical_integer_text_value(value);
                }
                return std::to_string(unsigned_seconds * 1000);
            } else if (value.is_string()) {
                const auto trimmed = trim_ascii_copy(value.get<std::string>());
                if (trimmed.empty()) {
                    return canonical_integer_text_value(value);
                }
                std::size_t consumed = 0;
                seconds = std::stoll(trimmed, &consumed);
                if (consumed != trimmed.size()) {
                    return canonical_integer_text_value(value);
                }
            } else {
                return value;
            }

            if (seconds > std::numeric_limits<std::int64_t>::max() / 1000 ||
                seconds < std::numeric_limits<std::int64_t>::min() / 1000) {
                return canonical_integer_text_value(value);
            }
            return std::to_string(seconds * 1000);
        } catch (...) {
            return canonical_integer_text_value(value);
        }
    }

    inline void normalize_order_type_member(nlohmann::json& object, const char* key) {
        if (!object.is_object() || !object.contains(key) || !object.at(key).is_string()) {
            return;
        }

        try {
            const auto value = canonical_order_type_value(
                trim_ascii_copy(object.at(key).get<std::string>()));
            object[key] = to_str(value);
        } catch (...) {
            object[key] = upper_ascii_copy(trim_ascii_copy(object.at(key).get<std::string>()));
        }
    }

    inline void normalize_option_type_member(nlohmann::json& object, const char* key) {
        if (!object.is_object() || !object.contains(key) || !object.at(key).is_string()) {
            return;
        }

        try {
            const auto value = canonical_option_type_value(
                trim_ascii_copy(object.at(key).get<std::string>()));
            object[key] = to_str(value);
        } catch (...) {
            object[key] = upper_ascii_copy(trim_ascii_copy(object.at(key).get<std::string>()));
        }
    }

    inline void normalize_currency_member(nlohmann::json& object, const char* key) {
        if (!object.is_object() || !object.contains(key) || !object.at(key).is_string()) {
            return;
        }

        try {
            const auto value = canonical_currency_value(
                trim_ascii_copy(object.at(key).get<std::string>()));
            object[key] = to_str(value);
        } catch (...) {
            object[key] = upper_ascii_copy(trim_ascii_copy(object.at(key).get<std::string>()));
        }
    }

    inline void normalize_platform_type_member(nlohmann::json& object, const char* key) {
        if (!object.is_object() || !object.contains(key) || !object.at(key).is_string()) {
            return;
        }

        auto normalized = upper_ascii_copy(trim_ascii_copy(object.at(key).get<std::string>()));
        std::replace(normalized.begin(), normalized.end(), '.', '_');
        std::replace(normalized.begin(), normalized.end(), '-', '_');
        std::replace(normalized.begin(), normalized.end(), ' ', '_');
        try {
            const auto value = canonical_platform_type_value(normalized);
            object[key] = to_str(value);
        } catch (...) {
            object[key] = std::move(normalized);
        }
    }

    inline void normalize_amount_member(nlohmann::json& trade) {
        if (!trade.is_object() || !trade.contains("amount")) {
            return;
        }

        nlohmann::json amount = nlohmann::json::object();
        if (trade.at("amount").is_object()) {
            amount = trade.at("amount");
        } else {
            amount["value"] = trade.at("amount");
        }

        if (amount.contains("value")) {
            amount["value"] = canonical_decimal_value(amount.at("value"));
        }

        if (amount.contains("currency")) {
            normalize_currency_member(amount, "currency");
        } else if (trade.contains("currency")) {
            amount["currency"] = trade.at("currency");
            normalize_currency_member(amount, "currency");
        }

        trade["amount"] = std::move(amount);
        trade.erase("currency");
    }

    inline void normalize_expiry_member(nlohmann::json& trade) {
        if (!trade.is_object()) {
            return;
        }

        const bool has_top_level_expiry_alias =
            trade.contains("duration_ms") ||
            trade.contains("duration") ||
            trade.contains("duration_sec") ||
            trade.contains("expires_at_ms") ||
            trade.contains("expiry_time");
        nlohmann::json expiry = trade.contains("expiry") && trade.at("expiry").is_object()
            ? trade.at("expiry")
            : nlohmann::json::object();

        const auto kind = expiry.contains("kind") && expiry.at("kind").is_string()
            ? lower_ascii_copy(trim_ascii_copy(expiry.at("kind").get<std::string>()))
            : std::string();
        const bool nested_expiry_has_semantics =
            expiry.contains("duration_ms") ||
            expiry.contains("expires_at_ms") ||
            !kind.empty();

        const bool has_duration =
            expiry.contains("duration_ms") ||
            trade.contains("duration_ms") ||
            trade.contains("duration") ||
            trade.contains("duration_sec") ||
            kind == "duration";
        const bool has_absolute =
            expiry.contains("expires_at_ms") ||
            trade.contains("expires_at_ms") ||
            trade.contains("expiry_time") ||
            kind == "absolute";

        int expiry_source_count = 0;
        expiry_source_count += expiry.contains("duration_ms") ? 1 : 0;
        expiry_source_count += expiry.contains("expires_at_ms") ? 1 : 0;
        expiry_source_count += trade.contains("duration_ms") ? 1 : 0;
        expiry_source_count += trade.contains("duration") ? 1 : 0;
        expiry_source_count += trade.contains("duration_sec") ? 1 : 0;
        expiry_source_count += trade.contains("expires_at_ms") ? 1 : 0;
        expiry_source_count += trade.contains("expiry_time") ? 1 : 0;
        if (expiry_source_count > 1) {
            if (expiry.contains("kind") && expiry.at("kind").is_string()) {
                expiry["kind"] = kind;
            }
            if (expiry.contains("duration_ms")) {
                expiry["duration_ms"] =
                    canonical_integer_text_value(expiry.at("duration_ms"));
            }
            if (expiry.contains("expires_at_ms")) {
                expiry["expires_at_ms"] =
                    canonical_integer_text_value(expiry.at("expires_at_ms"));
            }
            if (!expiry.empty()) {
                trade["expiry"] = std::move(expiry);
            }
            if (trade.contains("duration_ms")) {
                trade["duration_ms"] =
                    canonical_integer_text_value(trade.at("duration_ms"));
            }
            if (trade.contains("duration")) {
                trade["duration"] =
                    canonical_integer_text_value(trade.at("duration"));
            }
            if (trade.contains("duration_sec")) {
                trade["duration_sec"] =
                    canonical_integer_text_value(trade.at("duration_sec"));
            }
            if (trade.contains("expires_at_ms")) {
                trade["expires_at_ms"] =
                    canonical_integer_text_value(trade.at("expires_at_ms"));
            }
            if (trade.contains("expiry_time")) {
                trade["expiry_time"] =
                    canonical_integer_text_value(trade.at("expiry_time"));
            }
            return;
        }

        if (has_duration && has_absolute) {
            nlohmann::json duration_ms;
            if (expiry.contains("duration_ms")) {
                duration_ms = expiry.at("duration_ms");
            } else if (trade.contains("duration_ms")) {
                duration_ms = trade.at("duration_ms");
            } else if (trade.contains("duration")) {
                duration_ms = canonical_milliseconds_from_seconds_value(trade.at("duration"));
            } else if (trade.contains("duration_sec")) {
                duration_ms = canonical_milliseconds_from_seconds_value(trade.at("duration_sec"));
            }

            nlohmann::json expires_at_ms;
            if (expiry.contains("expires_at_ms")) {
                expires_at_ms = expiry.at("expires_at_ms");
            } else if (trade.contains("expires_at_ms")) {
                expires_at_ms = trade.at("expires_at_ms");
            } else if (trade.contains("expiry_time")) {
                expires_at_ms = canonical_milliseconds_from_seconds_value(trade.at("expiry_time"));
            }

            expiry = nlohmann::json::object();
            if (!kind.empty()) {
                expiry["kind"] = kind;
            }
            if (!duration_ms.is_null()) {
                expiry["duration_ms"] = canonical_integer_text_value(duration_ms);
            }
            if (!expires_at_ms.is_null()) {
                expiry["expires_at_ms"] = canonical_integer_text_value(expires_at_ms);
            }
            trade["expiry"] = std::move(expiry);
        } else if (has_duration) {
            nlohmann::json duration_ms;
            if (expiry.contains("duration_ms")) {
                duration_ms = expiry.at("duration_ms");
            } else if (trade.contains("duration_ms")) {
                duration_ms = trade.at("duration_ms");
            } else if (trade.contains("duration")) {
                duration_ms = canonical_milliseconds_from_seconds_value(trade.at("duration"));
            } else if (trade.contains("duration_sec")) {
                duration_ms = canonical_milliseconds_from_seconds_value(trade.at("duration_sec"));
            }

            expiry = nlohmann::json::object();
            expiry["kind"] = "duration";
            if (!duration_ms.is_null()) {
                expiry["duration_ms"] = canonical_integer_text_value(duration_ms);
            }
            trade["expiry"] = std::move(expiry);
        } else if (has_absolute) {
            nlohmann::json expires_at_ms;
            if (expiry.contains("expires_at_ms")) {
                expires_at_ms = expiry.at("expires_at_ms");
            } else if (trade.contains("expires_at_ms")) {
                expires_at_ms = trade.at("expires_at_ms");
            } else if (trade.contains("expiry_time")) {
                expires_at_ms = canonical_milliseconds_from_seconds_value(trade.at("expiry_time"));
            }

            expiry = nlohmann::json::object();
            expiry["kind"] = "absolute";
            if (!expires_at_ms.is_null()) {
                expiry["expires_at_ms"] = canonical_integer_text_value(expires_at_ms);
            }
            trade["expiry"] = std::move(expiry);
        } else if (trade.contains("expiry") && trade.at("expiry").is_object()) {
            if (trade.at("expiry").contains("kind") &&
                trade.at("expiry").at("kind").is_string()) {
                trade["expiry"]["kind"] =
                    lower_ascii_copy(trim_ascii_copy(trade.at("expiry").at("kind").get<std::string>()));
            }
        }

        if (!(nested_expiry_has_semantics && has_top_level_expiry_alias)) {
            trade.erase("duration_ms");
            trade.erase("duration");
            trade.erase("duration_sec");
            trade.erase("expires_at_ms");
            trade.erase("expiry_time");
        }
    }

    inline void normalize_identity_aliases(nlohmann::json& canonical, nlohmann::json& trade) {
        if (!canonical.is_object() || !trade.is_object()) {
            return;
        }

        if (!canonical.contains("identity") || !canonical.at("identity").is_object()) {
            canonical["identity"] = nlohmann::json::object();
        }
        auto& identity = canonical["identity"];

        const char* text_keys[] = {"unique_hash", "signal_name", "user_data"};
        for (const auto* key : text_keys) {
            if (!identity.contains(key) && trade.contains(key)) {
                identity[key] = canonical_identifier_value(trade.at(key));
            } else if (identity.contains(key)) {
                identity[key] = canonical_identifier_value(identity.at(key));
            }
            trade.erase(key);
        }

        if (!identity.contains("unique_id") && trade.contains("unique_id")) {
            identity["unique_id"] = canonical_identifier_value(trade.at("unique_id"));
        } else if (identity.contains("unique_id")) {
            identity["unique_id"] = canonical_identifier_value(identity.at("unique_id"));
        }
        trade.erase("unique_id");

        if (identity.empty()) {
            canonical.erase("identity");
        }
    }

    inline void normalize_routing_aliases(nlohmann::json& canonical, nlohmann::json& trade) {
        if (!canonical.is_object() || !trade.is_object()) {
            return;
        }

        if (!canonical.contains("routing") || !canonical.at("routing").is_object()) {
            canonical["routing"] = nlohmann::json::object();
        }
        auto& routing = canonical["routing"];
        if (!routing.contains("selector") || !routing.at("selector").is_object()) {
            routing["selector"] = nlohmann::json::object();
        }
        auto& selector = routing["selector"];

        if (selector.contains("kind") && selector.at("kind").is_string()) {
            selector["kind"] = lower_ascii_copy(trim_ascii_copy(selector.at("kind").get<std::string>()));
        }

        if (selector.contains("account_id")) {
            selector["account_id"] = canonical_identifier_value(selector.at("account_id"));
        } else if (trade.contains("account_id")) {
            selector["account_id"] = canonical_identifier_value(trade.at("account_id"));
        }
        trade.erase("account_id");

        normalize_platform_type_member(routing, "platform_type");

        if (selector.empty()) {
            routing.erase("selector");
        }
        if (routing.empty()) {
            canonical.erase("routing");
        }
    }

    inline void normalize_trade_business_object(nlohmann::json& trade) {
        if (!trade.is_object()) {
            return;
        }

        if (!trade.contains("order_type")) {
            if (trade.contains("direction")) {
                trade["order_type"] = trade.at("direction");
            } else if (trade.contains("action")) {
                trade["order_type"] = trade.at("action");
            }
        }
        trade.erase("direction");
        trade.erase("action");

        if (trade.contains("symbol")) {
            trade["symbol"] = canonical_identifier_value(trade.at("symbol"));
        }
        if (trade.contains("account_type") && trade.at("account_type").is_string()) {
            try {
                const auto value = canonical_account_type_value(
                    trim_ascii_copy(trade.at("account_type").get<std::string>()));
                trade["account_type"] = to_str(value);
            } catch (...) {
                trade["account_type"] =
                    upper_ascii_copy(trim_ascii_copy(trade.at("account_type").get<std::string>()));
            }
        }

        normalize_order_type_member(trade, "order_type");
        normalize_option_type_member(trade, "option_type");
        normalize_currency_member(trade, "currency");
        normalize_amount_member(trade);
        normalize_expiry_member(trade);

        if (trade.contains("refund")) {
            trade["refund"] = canonical_decimal_value(trade.at("refund"));
        }
        if (trade.contains("min_payout")) {
            trade["min_payout"] = canonical_decimal_value(trade.at("min_payout"));
        }
    }

    /// \brief Builds canonical business JSON for trade-command idempotency.
    inline nlohmann::json canonical_trade_command_payload(
            const nlohmann::json& params,
            const bool direct_trade_open) {
        auto canonical = params.is_object() ? params : nlohmann::json::object();

        if (canonical.contains("context") && canonical.at("context").is_object()) {
            auto& context = canonical["context"];
            context.erase("idempotency_key");
            context.erase("valid_until_ms");
            context.erase("client_created_at_ms");
            context.erase("file_seq");
            context.erase("auth");
            context.erase("authorization");
            context.erase("api_key");
            context.erase("client_secret");
            context.erase("transport");
            if (context.empty()) {
                canonical.erase("context");
            }
        }

        const auto trade_key = direct_trade_open ? "trade" : "signal";
        if (canonical.contains(trade_key) && canonical.at(trade_key).is_object()) {
            auto& trade = canonical[trade_key];
            normalize_identity_aliases(canonical, trade);
            normalize_routing_aliases(canonical, trade);
            normalize_trade_business_object(trade);
        } else {
            normalize_trade_business_object(canonical);
        }

        return canonical;
    }

} // namespace optionx::bridges::protocol_v1::detail

#endif // OPTIONX_HEADER_BRIDGES_PROTOCOL_V1_DETAIL_BRIDGE_PROTOCOL_CANONICALIZATION_HPP_INCLUDED
