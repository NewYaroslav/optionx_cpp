#pragma once
#ifndef OPTIONX_HEADER_BRIDGES_METATRADER_FILE_DETAIL_META_TRADER_FILE_BRIDGE_UTILS_HPP_INCLUDED
#define OPTIONX_HEADER_BRIDGES_METATRADER_FILE_DETAIL_META_TRADER_FILE_BRIDGE_UTILS_HPP_INCLUDED

/// \file MetaTraderFileBridgeUtils.hpp
/// \brief Defines internal helpers for the MetaTrader file bridge runtime.
///
/// These helpers belong to the bridge implementation layer. They are included
/// through `bridges/metatrader_file.hpp` and are not a standalone public include
/// entry point.

namespace optionx::bridges::metatrader_file::detail {

    /// \brief Returns whether an account update should produce balance events.
    inline bool should_emit_balance_update(const AccountUpdateStatus status) noexcept {
        switch (status) {
        case AccountUpdateStatus::CONNECTED:
        case AccountUpdateStatus::BALANCE_UPDATED:
        case AccountUpdateStatus::ACCOUNT_TYPE_CHANGED:
        case AccountUpdateStatus::CURRENCY_CHANGED:
            return true;
        default:
            return false;
        }
    }

    /// \brief Builds the protocol event source URI for this bridge instance.
    inline std::string source_uri(const MetaTraderFileBridgeConfig& config) {
        return "optionx://bridge/metatrader_file/" + std::to_string(config.bridge_id);
    }

    /// \brief Builds the event stream identity for this bridge/client pair.
    inline std::string stream_id(const MetaTraderFileBridgeConfig& config) {
        return "metatrader-file-" +
               std::to_string(config.bridge_id) +
               "-" +
               config.client_id;
    }

    /// \brief Reads the best available account ID as an opaque protocol string.
    inline std::string account_id_string(const BaseAccountInfoData& account) {
        try {
            auto id = account.get_info<std::string>(AccountInfoType::USER_ID);
            if (!id.empty()) {
                return id;
            }
        } catch (...) {
        }
        try {
            const auto id = account.get_info<std::int64_t>(AccountInfoType::USER_ID);
            if (id != 0) {
                return std::to_string(id);
            }
        } catch (...) {
        }
        return "0";
    }

    /// \brief Reads the account balance or returns the transport default.
    inline double safe_account_balance(const BaseAccountInfoData& account) {
        try {
            return account.get_info<double>(AccountInfoType::BALANCE);
        } catch (...) {
            return 0.0;
        }
    }

    /// \brief Reads the account currency or returns `CurrencyType::UNKNOWN`.
    inline CurrencyType safe_account_currency(const BaseAccountInfoData& account) {
        try {
            return account.get_info<CurrencyType>(AccountInfoType::CURRENCY);
        } catch (...) {
            return CurrencyType::UNKNOWN;
        }
    }

    /// \brief Reads the account type or returns `AccountType::UNKNOWN`.
    inline AccountType safe_account_type(const BaseAccountInfoData& account) {
        try {
            return account.get_info<AccountType>(AccountInfoType::ACCOUNT_TYPE);
        } catch (...) {
            return AccountType::UNKNOWN;
        }
    }

    /// \brief Reads the connection state as a protocol string.
    inline std::string connection_string(const BaseAccountInfoData& account) {
        try {
            return account.get_info<bool>(AccountInfoType::CONNECTION_STATUS)
                ? "connected"
                : "disconnected";
        } catch (...) {
            return "unknown";
        }
    }

    /// \brief Converts an account snapshot into the bridge protocol shape.
    inline nlohmann::json account_snapshot_json(const BaseAccountInfoData& account) {
        nlohmann::json snapshot = {
            {"account_id", account_id_string(account)},
            {"connection", connection_string(account)},
            {"balance", make_money_value(
                safe_account_balance(account),
                safe_account_currency(account),
                2)}
        };
        const auto account_type = safe_account_type(account);
        if (account_type != AccountType::UNKNOWN) {
            snapshot["account_type"] = to_str(account_type);
        }
        return snapshot;
    }

    /// \brief Clones a pending signal for immutable diagnostics.
    inline std::shared_ptr<const TradeSignal> clone_candidate_signal(
            const std::unique_ptr<TradeSignal>& signal) {
        if (!signal) {
            return {};
        }
        return std::shared_ptr<const TradeSignal>(signal->clone());
    }

    /// \brief Builds a standard bridge signal report from file-transport data.
    inline BridgeSignalReport make_signal_report(
            const MetaTraderFileBridgeConfig& config,
            BridgeSignalReportStatus status,
            std::string reason_code,
            std::string message,
            nlohmann::json raw_payload = nlohmann::json(),
            nlohmann::json parsed_payload = nlohmann::json(),
            std::string event_id = {},
            std::string dedupe_key = {},
            std::shared_ptr<const TradeSignal> candidate_signal = {},
            nlohmann::json context = nlohmann::json::object()) {
        BridgeSignalReport report;
        report.bridge_id = config.bridge_id;
        report.bridge_type = config.bridge_type();
        report.status = status;
        report.reason_code = std::move(reason_code);
        report.message = std::move(message);
        report.raw_payload = std::move(raw_payload);
        report.parsed_payload = std::move(parsed_payload);
        report.event_id = std::move(event_id);
        report.dedupe_key = std::move(dedupe_key);
        report.candidate_signal = std::move(candidate_signal);
        report.context = std::move(context);
        report.received_time_ms = unix_time_ms();
        if (report.candidate_signal) {
            report.symbol = report.candidate_signal->symbol;
            report.signal_name = report.candidate_signal->signal_name;
            if (report.dedupe_key.empty()) {
                report.dedupe_key = report.candidate_signal->unique_hash;
            }
        }
        return report;
    }

    /// \brief Formats any JSON-RPC ID as an opaque diagnostic string.
    inline std::string json_id_to_string(const nlohmann::json& id) {
        if (id.is_string()) {
            return id.get<std::string>();
        }
        if (id.is_null()) {
            return {};
        }
        return id.dump(-1);
    }

    /// \brief Returns a nested object when present, otherwise the input object.
    inline const nlohmann::json& object_member_or_self(
            const nlohmann::json& object,
            const char* key) {
        if (object.is_object() &&
            object.contains(key) &&
            object.at(key).is_object()) {
            return object.at(key);
        }
        return object;
    }

    /// \brief Returns a nested object when present, otherwise an empty object.
    inline const nlohmann::json& object_member_or_empty(
            const nlohmann::json& object,
            const char* key) {
        static const nlohmann::json empty = nlohmann::json::object();
        if (object.is_object() &&
            object.contains(key) &&
            object.at(key).is_object()) {
            return object.at(key);
        }
        return empty;
    }

    /// \brief Reads a string-like JSON member.
    inline std::string string_value(
            const nlohmann::json& object,
            const char* key,
            std::string fallback = {}) {
        if (!object.is_object() || !object.contains(key)) {
            return fallback;
        }
        const auto& value = object.at(key);
        if (value.is_string()) {
            return value.get<std::string>();
        }
        if (value.is_number_integer()) {
            return std::to_string(value.get<std::int64_t>());
        }
        if (value.is_number_unsigned()) {
            return std::to_string(value.get<std::uint64_t>());
        }
        return fallback;
    }

    inline std::int64_t int64_value(
            const nlohmann::json& object,
            const char* key,
            std::int64_t fallback);
    inline CurrencyType currency_value(const std::string& value);
    inline AccountType account_type_value(const std::string& value);
    inline OptionType option_type_value(const std::string& value);
    inline OrderType order_type_value(const std::string& value);

    /// \brief Trims ASCII whitespace without changing protocol text otherwise.
    inline std::string trim_ascii_copy(const std::string& value) {
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

    /// \brief Uppercases ASCII protocol tokens.
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

    /// \brief Lowercases ASCII protocol tokens.
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

    /// \brief Normalizes a decimal text by removing non-semantic scale.
    inline std::string canonical_decimal_text(std::string text) {
        text = trim_ascii_copy(text);
        if (text.empty()) {
            return text;
        }

        if (text.find_first_of("eE") != std::string::npos) {
            try {
                std::ostringstream out;
                out.imbue(std::locale::classic());
                out << std::fixed << std::setprecision(12) << std::stold(text);
                return canonical_decimal_text(out.str());
            } catch (...) {
                return text;
            }
        }

        bool negative = false;
        if (text.front() == '+' || text.front() == '-') {
            negative = text.front() == '-';
            text.erase(text.begin());
        }

        const auto dot = text.find('.');
        auto integer = dot == std::string::npos ? text : text.substr(0, dot);
        auto fraction = dot == std::string::npos ? std::string() : text.substr(dot + 1);

        const auto is_digits = [](const std::string& part) {
            return std::all_of(part.begin(), part.end(), [](const unsigned char ch) {
                return std::isdigit(ch) != 0;
            });
        };
        if ((!integer.empty() && !is_digits(integer)) ||
            (!fraction.empty() && !is_digits(fraction)) ||
            (integer.empty() && fraction.empty())) {
            return (negative ? "-" : "") + text;
        }

        const auto non_zero = integer.find_first_not_of('0');
        integer = non_zero == std::string::npos ? "0" : integer.substr(non_zero);
        while (!fraction.empty() && fraction.back() == '0') {
            fraction.pop_back();
        }

        if (integer == "0" && fraction.empty()) {
            negative = false;
        }

        auto normalized = (negative ? "-" : "") + integer;
        if (!fraction.empty()) {
            normalized += "." + fraction;
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
            std::ostringstream out;
            out.imbue(std::locale::classic());
            out << std::fixed << std::setprecision(12) << numeric;
            return canonical_decimal_text(out.str());
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
                seconds = std::stoll(trim_ascii_copy(value.get<std::string>()));
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

    /// \brief Converts order direction aliases to the canonical protocol spelling.
    inline void normalize_order_type_member(nlohmann::json& object, const char* key) {
        if (!object.is_object() || !object.contains(key) || !object.at(key).is_string()) {
            return;
        }

        try {
            const auto value = order_type_value(trim_ascii_copy(object.at(key).get<std::string>()));
            object[key] = to_str(value);
        } catch (...) {
            object[key] = upper_ascii_copy(trim_ascii_copy(object.at(key).get<std::string>()));
        }
    }

    /// \brief Converts option type aliases to the canonical protocol spelling.
    inline void normalize_option_type_member(nlohmann::json& object, const char* key) {
        if (!object.is_object() || !object.contains(key) || !object.at(key).is_string()) {
            return;
        }

        try {
            const auto value = option_type_value(trim_ascii_copy(object.at(key).get<std::string>()));
            object[key] = to_str(value);
        } catch (...) {
            object[key] = upper_ascii_copy(trim_ascii_copy(object.at(key).get<std::string>()));
        }
    }

    /// \brief Converts known currency aliases to the canonical protocol spelling.
    inline void normalize_currency_member(nlohmann::json& object, const char* key) {
        if (!object.is_object() || !object.contains(key) || !object.at(key).is_string()) {
            return;
        }

        try {
            const auto value = currency_value(trim_ascii_copy(object.at(key).get<std::string>()));
            object[key] = to_str(value);
        } catch (...) {
            object[key] = upper_ascii_copy(trim_ascii_copy(object.at(key).get<std::string>()));
        }
    }

    /// \brief Normalizes an amount object/scalar without depending on input scale.
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

    /// \brief Normalizes expiry aliases while keeping duration and absolute forms distinct.
    inline void normalize_expiry_member(nlohmann::json& trade) {
        if (!trade.is_object()) {
            return;
        }

        nlohmann::json expiry = trade.contains("expiry") && trade.at("expiry").is_object()
            ? trade.at("expiry")
            : nlohmann::json::object();

        const auto kind = expiry.contains("kind") && expiry.at("kind").is_string()
            ? lower_ascii_copy(trim_ascii_copy(expiry.at("kind").get<std::string>()))
            : std::string();

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

        if (has_duration && !has_absolute) {
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

        trade.erase("duration_ms");
        trade.erase("duration");
        trade.erase("duration_sec");
        trade.erase("expires_at_ms");
        trade.erase("expiry_time");
    }

    /// \brief Moves trade identity aliases to a single canonical identity object.
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

    /// \brief Normalizes routing and account identity aliases.
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

        if (selector.empty()) {
            routing.erase("selector");
        }
        if (routing.empty()) {
            canonical.erase("routing");
        }
    }

    /// \brief Normalizes a trade/signal business object for idempotency comparison.
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
                const auto value = account_type_value(trim_ascii_copy(trade.at("account_type").get<std::string>()));
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

    /// \brief Reads a decimal-like JSON member.
    inline double double_value(
            const nlohmann::json& object,
            const char* key,
            const double fallback = 0.0) {
        if (!object.is_object() || !object.contains(key)) {
            return fallback;
        }
        const auto& value = object.at(key);
        if (value.is_number()) {
            return value.get<double>();
        }
        if (value.is_string()) {
            return std::stod(value.get<std::string>());
        }
        return fallback;
    }

    /// \brief Reads a signed integer-like JSON member.
    inline std::int64_t int64_value(
            const nlohmann::json& object,
            const char* key,
            const std::int64_t fallback = 0) {
        if (!object.is_object() || !object.contains(key)) {
            return fallback;
        }
        const auto& value = object.at(key);
        if (value.is_number_integer()) {
            return value.get<std::int64_t>();
        }
        if (value.is_number_unsigned()) {
            return static_cast<std::int64_t>(value.get<std::uint64_t>());
        }
        if (value.is_string()) {
            return std::stoll(value.get<std::string>());
        }
        return fallback;
    }

    /// \brief Reads an unsigned integer-like JSON member.
    inline std::uint64_t uint64_value(
            const nlohmann::json& object,
            const char* key,
            const std::uint64_t fallback = 0) {
        if (!object.is_object() || !object.contains(key)) {
            return fallback;
        }
        const auto& value = object.at(key);
        if (value.is_number_unsigned()) {
            return value.get<std::uint64_t>();
        }
        if (value.is_number_integer()) {
            const auto signed_value = value.get<std::int64_t>();
            return signed_value >= 0 ? static_cast<std::uint64_t>(signed_value) : fallback;
        }
        if (value.is_string()) {
            const auto parsed = std::stoll(value.get<std::string>());
            return parsed >= 0 ? static_cast<std::uint64_t>(parsed) : fallback;
        }
        return fallback;
    }

    /// \brief Converts optional protocol text to a currency enum.
    inline CurrencyType currency_value(const std::string& value) {
        if (value.empty()) {
            return CurrencyType::UNKNOWN;
        }
        return to_enum<CurrencyType>(value);
    }

    /// \brief Converts optional protocol text to an account type enum.
    inline AccountType account_type_value(const std::string& value) {
        if (value.empty()) {
            return AccountType::UNKNOWN;
        }
        return to_enum<AccountType>(value);
    }

    /// \brief Converts optional protocol text to an option type enum.
    inline OptionType option_type_value(const std::string& value) {
        if (value.empty()) {
            return OptionType::UNKNOWN;
        }
        return to_enum<OptionType>(value);
    }

    /// \brief Converts optional protocol text to an order type enum.
    inline OrderType order_type_value(const std::string& value) {
        if (value.empty()) {
            return OrderType::UNKNOWN;
        }
        return to_enum<OrderType>(value);
    }

    /// \brief Applies amount and optional currency fields to a signal.
    inline void apply_amount(TradeSignal& signal, const nlohmann::json& trade) {
        if (!trade.is_object() || !trade.contains("amount")) {
            return;
        }
        const auto& amount = trade.at("amount");
        if (amount.is_object()) {
            signal.amount = double_value(amount, "value", signal.amount);
            const auto currency = string_value(amount, "currency");
            if (!currency.empty()) {
                signal.currency = currency_value(currency);
            }
        } else if (amount.is_number()) {
            signal.amount = amount.get<double>();
        } else if (amount.is_string()) {
            signal.amount = std::stod(amount.get<std::string>());
        }
    }

    /// \brief Applies duration or absolute expiration fields to a signal.
    inline void apply_expiry(TradeSignal& signal, const nlohmann::json& trade) {
        if (trade.is_object() && trade.contains("expiry") && trade.at("expiry").is_object()) {
            const auto& expiry = trade.at("expiry");
            const auto kind = string_value(expiry, "kind");
            if (kind == "duration" || expiry.contains("duration_ms")) {
                const auto duration_ms = int64_value(expiry, "duration_ms", 0);
                if (duration_ms > 0) {
                    signal.duration = static_cast<std::uint32_t>(duration_ms / 1000);
                }
            } else if (kind == "absolute" || expiry.contains("expires_at_ms")) {
                const auto expires_at_ms = int64_value(expiry, "expires_at_ms", 0);
                if (expires_at_ms > 0) {
                    signal.expiry_time = expires_at_ms / 1000;
                }
            }
        }
        const auto duration_ms = int64_value(trade, "duration_ms", 0);
        if (duration_ms > 0) {
            signal.duration = static_cast<std::uint32_t>(duration_ms / 1000);
        }
        const auto duration = int64_value(trade, "duration", 0);
        if (duration > 0) {
            signal.duration = static_cast<std::uint32_t>(duration);
        }
        const auto duration_sec = int64_value(trade, "duration_sec", 0);
        if (duration_sec > 0) {
            signal.duration = static_cast<std::uint32_t>(duration_sec);
        }
        const auto expires_at_ms = int64_value(trade, "expires_at_ms", 0);
        if (expires_at_ms > 0) {
            signal.expiry_time = expires_at_ms / 1000;
        }
        const auto expiry_time = int64_value(trade, "expiry_time", 0);
        if (expiry_time > 0) {
            signal.expiry_time = expiry_time;
        }
    }

    /// \brief Applies routing selector fields to a signal.
    inline void apply_routing(TradeSignal& signal, const nlohmann::json& params) {
        const auto& routing = object_member_or_empty(params, "routing");
        const auto& selector = object_member_or_empty(routing, "selector");
        const auto account_id = int64_value(selector, "account_id", 0);
        if (account_id != 0) {
            signal.account_id = account_id;
        }
    }

    /// \brief Reads the optional command deadline from context.
    inline std::int64_t context_valid_until_ms(const nlohmann::json& params) {
        const auto& context = object_member_or_empty(params, "context");
        return int64_value(context, "valid_until_ms", 0);
    }

    /// \brief Reads the optional command idempotency key from context.
    inline std::string context_idempotency_key(const nlohmann::json& params) {
        const auto& context = object_member_or_empty(params, "context");
        return string_value(context, "idempotency_key");
    }

    /// \brief Converts `signal.submit` or `trade.open` params into a TradeSignal.
    inline std::unique_ptr<TradeSignal> parse_signal_params(
            const nlohmann::json& params,
            const bool direct_trade_open) {
        if (!params.is_object()) {
            throw std::invalid_argument("Command params must be an object.");
        }

        const auto& trade = direct_trade_open
            ? object_member_or_self(params, "trade")
            : object_member_or_self(params, "signal");
        const auto& identity = object_member_or_empty(params, "identity");
        const auto& context = object_member_or_empty(params, "context");

        auto signal = std::make_unique<TradeSignal>();
        signal->symbol = string_value(trade, "symbol");
        signal->signal_name = string_value(
            identity,
            "signal_name",
            string_value(trade, "signal_name", direct_trade_open ? "direct_trade_open" : ""));
        signal->unique_hash = string_value(
            identity,
            "unique_hash",
            string_value(trade, "unique_hash", string_value(context, "idempotency_key")));
        signal->unique_id = int64_value(identity, "unique_id", int64_value(trade, "unique_id", 0));
        signal->user_data = string_value(identity, "user_data", string_value(trade, "user_data"));
        signal->comment = string_value(trade, "comment");
        signal->account_id = int64_value(trade, "account_id", 0);
        signal->account_type = account_type_value(string_value(trade, "account_type"));
        signal->currency = currency_value(string_value(trade, "currency"));
        signal->option_type = option_type_value(string_value(trade, "option_type"));
        signal->order_type = order_type_value(
            string_value(
                trade,
                "order_type",
                string_value(trade, "direction", string_value(trade, "action"))));
        signal->refund = double_value(trade, "refund", 0.0);
        signal->min_payout = double_value(trade, "min_payout", 0.0);

        apply_amount(*signal, trade);
        apply_expiry(*signal, trade);
        apply_routing(*signal, params);

        if (signal->symbol.empty()) {
            throw std::invalid_argument("Command trade symbol is required.");
        }
        if (signal->order_type == OrderType::UNKNOWN) {
            throw std::invalid_argument("Command order_type is required.");
        }
        if (direct_trade_open && signal->amount <= 0.0) {
            throw std::invalid_argument("trade.open amount must be positive.");
        }
        return signal;
    }

} // namespace optionx::bridges::metatrader_file::detail

#endif // OPTIONX_HEADER_BRIDGES_METATRADER_FILE_DETAIL_META_TRADER_FILE_BRIDGE_UTILS_HPP_INCLUDED
