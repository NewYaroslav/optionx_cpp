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

    /// \brief Converts an internal OptionX account ID into protocol text.
    inline std::string account_id_string(const std::int64_t account_id) {
        return account_id != 0 ? std::to_string(account_id) : std::string();
    }

    /// \brief Reads the best available broker/platform user ID as protocol text.
    inline std::string user_id_string(const BaseAccountInfoData& account) {
        try {
            auto id = account.get_info<std::string>(AccountInfoType::USER_ID);
            if (!id.empty() && id != "0") {
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
        return {};
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
    inline nlohmann::json account_snapshot_json(
            const BaseAccountInfoData& account,
            const std::int64_t account_id = 0) {
        nlohmann::json snapshot = {
            {"connection", connection_string(account)},
            {"balance", make_money_value(
                safe_account_balance(account),
                safe_account_currency(account),
                2)}
        };
        if (account_id != 0) {
            snapshot["account_id"] = account_id_string(account_id);
        }
        const auto user_id = user_id_string(account);
        if (!user_id.empty()) {
            snapshot["user_id"] = user_id;
        }
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
    inline PlatformType platform_type_value(const std::string& value);
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

    inline double strict_decimal_text_to_double(
            const std::string& value,
            const char* field_name) {
        const auto trimmed = trim_ascii_copy(value);
        if (trimmed.empty()) {
            throw std::invalid_argument(std::string(field_name) + " must not be empty.");
        }
        std::size_t consumed = 0;
        const auto parsed = std::stod(trimmed, &consumed);
        if (consumed != trimmed.size() || !std::isfinite(parsed)) {
            throw std::invalid_argument(std::string(field_name) + " must be a finite decimal value.");
        }
        return parsed;
    }

    inline std::int64_t strict_text_to_int64(
            const std::string& value,
            const char* field_name) {
        const auto trimmed = trim_ascii_copy(value);
        if (trimmed.empty()) {
            throw std::invalid_argument(std::string(field_name) + " must not be empty.");
        }
        std::size_t consumed = 0;
        const auto parsed = std::stoll(trimmed, &consumed);
        if (consumed != trimmed.size()) {
            throw std::invalid_argument(std::string(field_name) + " must be an integer value.");
        }
        return parsed;
    }

    inline std::uint32_t checked_positive_duration_seconds(
            const std::int64_t seconds,
            const char* field_name) {
        if (seconds <= 0) {
            return 0;
        }
        if (seconds > static_cast<std::int64_t>(std::numeric_limits<std::uint32_t>::max())) {
            throw std::out_of_range(std::string(field_name) + " exceeds uint32 seconds.");
        }
        return static_cast<std::uint32_t>(seconds);
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
            const auto parsed = value.get<double>();
            if (!std::isfinite(parsed)) {
                throw std::invalid_argument(std::string(key) + " must be a finite decimal value.");
            }
            return parsed;
        }
        if (value.is_string()) {
            return strict_decimal_text_to_double(value.get<std::string>(), key);
        }
        throw std::invalid_argument(std::string(key) + " must be a decimal value.");
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
            const auto parsed = value.get<std::uint64_t>();
            if (parsed > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
                throw std::out_of_range(std::string(key) + " exceeds int64.");
            }
            return static_cast<std::int64_t>(parsed);
        }
        if (value.is_string()) {
            return strict_text_to_int64(value.get<std::string>(), key);
        }
        throw std::invalid_argument(std::string(key) + " must be an integer value.");
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
            if (signed_value < 0) {
                throw std::invalid_argument(std::string(key) + " must be non-negative.");
            }
            return static_cast<std::uint64_t>(signed_value);
        }
        if (value.is_string()) {
            const auto parsed = strict_text_to_int64(value.get<std::string>(), key);
            if (parsed < 0) {
                throw std::invalid_argument(std::string(key) + " must be non-negative.");
            }
            return static_cast<std::uint64_t>(parsed);
        }
        throw std::invalid_argument(std::string(key) + " must be an unsigned integer value.");
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

    /// \brief Converts optional protocol text to a platform enum.
    inline PlatformType platform_type_value(const std::string& value) {
        if (value.empty()) {
            return PlatformType::UNKNOWN;
        }
        auto normalized = upper_ascii_copy(trim_ascii_copy(value));
        std::replace(normalized.begin(), normalized.end(), '.', '_');
        std::replace(normalized.begin(), normalized.end(), '-', '_');
        std::replace(normalized.begin(), normalized.end(), ' ', '_');
        return to_enum<PlatformType>(normalized);
    }

    /// \brief Converts optional protocol text to a money-management enum.
    inline MmSystemType mm_system_type_value(const std::string& value) {
        if (value.empty()) {
            return MmSystemType::NONE;
        }
        return to_enum<MmSystemType>(value);
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
            if (!std::isfinite(signal.amount)) {
                throw std::invalid_argument("amount must be a finite decimal value.");
            }
        } else if (amount.is_string()) {
            signal.amount = strict_decimal_text_to_double(amount.get<std::string>(), "amount");
        } else {
            throw std::invalid_argument("amount must be a decimal value or object.");
        }
    }

    /// \brief Applies duration or absolute expiration fields to a signal.
    inline void apply_expiry(TradeSignal& signal, const nlohmann::json& trade) {
        int expiry_forms = 0;
        const auto mark_expiry_form = [&expiry_forms]() {
            ++expiry_forms;
            if (expiry_forms > 1) {
                throw std::invalid_argument("Exactly one expiry form must be provided.");
            }
        };
        const auto validate_absolute_seconds = [](const std::int64_t seconds, const char* field) {
            if (seconds <= 0) {
                throw std::invalid_argument(std::string(field) + " must be positive.");
            }
            if (seconds <= unix_time_ms() / 1000) {
                throw std::invalid_argument(std::string(field) + " must be in the future.");
            }
        };

        if (trade.is_object() && trade.contains("expiry") && trade.at("expiry").is_object()) {
            const auto& expiry = trade.at("expiry");
            const auto kind = string_value(expiry, "kind");
            const bool has_duration = expiry.contains("duration_ms");
            const bool has_absolute = expiry.contains("expires_at_ms");
            if (has_duration && has_absolute) {
                throw std::invalid_argument("expiry must not mix duration and absolute fields.");
            }
            if (!kind.empty() && kind != "duration" && kind != "absolute") {
                throw std::invalid_argument("expiry.kind is unsupported.");
            }
            if (kind == "duration" && has_absolute) {
                throw std::invalid_argument("expiry.kind does not match expires_at_ms.");
            }
            if (kind == "absolute" && has_duration) {
                throw std::invalid_argument("expiry.kind does not match duration_ms.");
            }
            if (kind == "duration" || has_duration) {
                mark_expiry_form();
                if (!has_duration) {
                    throw std::invalid_argument("expiry.duration_ms is required.");
                }
                const auto duration_ms = int64_value(expiry, "duration_ms", 0);
                if (duration_ms <= 0) {
                    throw std::invalid_argument("expiry.duration_ms must be positive.");
                }
                if (duration_ms < 1000 || duration_ms % 1000 != 0) {
                    throw std::invalid_argument("expiry.duration_ms must be whole seconds.");
                }
                signal.duration = checked_positive_duration_seconds(
                    duration_ms / 1000,
                    "expiry.duration_ms");
            } else if (kind == "absolute" || has_absolute) {
                mark_expiry_form();
                if (!has_absolute) {
                    throw std::invalid_argument("expiry.expires_at_ms is required.");
                }
                const auto expires_at_ms = int64_value(expiry, "expires_at_ms", 0);
                if (expires_at_ms <= 0) {
                    throw std::invalid_argument("expiry.expires_at_ms must be positive.");
                }
                if (expires_at_ms < 1000 || expires_at_ms % 1000 != 0) {
                    throw std::invalid_argument("expiry.expires_at_ms must be whole seconds.");
                }
                signal.expiry_time = expires_at_ms / 1000;
                validate_absolute_seconds(signal.expiry_time, "expiry.expires_at_ms");
            }
        }
        if (trade.contains("duration_ms")) {
            mark_expiry_form();
            const auto duration_ms = int64_value(trade, "duration_ms", 0);
            if (duration_ms <= 0) {
                throw std::invalid_argument("duration_ms must be positive.");
            }
            if (duration_ms < 1000 || duration_ms % 1000 != 0) {
                throw std::invalid_argument("duration_ms must be whole seconds.");
            }
            signal.duration = checked_positive_duration_seconds(duration_ms / 1000, "duration_ms");
        }
        if (trade.contains("duration")) {
            mark_expiry_form();
            const auto duration = int64_value(trade, "duration", 0);
            if (duration <= 0) {
                throw std::invalid_argument("duration must be positive.");
            }
            signal.duration = checked_positive_duration_seconds(duration, "duration");
        }
        if (trade.contains("duration_sec")) {
            mark_expiry_form();
            const auto duration_sec = int64_value(trade, "duration_sec", 0);
            if (duration_sec <= 0) {
                throw std::invalid_argument("duration_sec must be positive.");
            }
            signal.duration = checked_positive_duration_seconds(duration_sec, "duration_sec");
        }
        if (trade.contains("expires_at_ms")) {
            mark_expiry_form();
            const auto expires_at_ms = int64_value(trade, "expires_at_ms", 0);
            if (expires_at_ms <= 0) {
                throw std::invalid_argument("expires_at_ms must be positive.");
            }
            if (expires_at_ms < 1000 || expires_at_ms % 1000 != 0) {
                throw std::invalid_argument("expires_at_ms must be whole seconds.");
            }
            signal.expiry_time = expires_at_ms / 1000;
            validate_absolute_seconds(signal.expiry_time, "expires_at_ms");
        }
        if (trade.contains("expiry_time")) {
            mark_expiry_form();
            const auto expiry_time = int64_value(trade, "expiry_time", 0);
            validate_absolute_seconds(expiry_time, "expiry_time");
            signal.expiry_time = expiry_time;
        }
    }

    /// \brief Applies routing selector fields to a signal.
    inline void apply_routing(TradeSignal& signal, const nlohmann::json& params) {
        const auto& routing = object_member_or_empty(params, "routing");
        const auto platform_type = string_value(routing, "platform_type");
        if (!platform_type.empty()) {
            signal.platform_type = platform_type_value(platform_type);
        }
        const auto& selector = object_member_or_empty(routing, "selector");
        const auto account_id = int64_value(selector, "account_id", 0);
        if (account_id != 0) {
            signal.account_id = account_id;
        }
    }

    /// \brief Applies optional protocol sizing fields to a signal.
    inline void apply_sizing(TradeSignal& signal, const nlohmann::json& params) {
        const auto& sizing = object_member_or_empty(params, "sizing");
        if (!sizing.is_object() || sizing.empty()) {
            return;
        }

        const auto mode = string_value(sizing, "mode", string_value(sizing, "type"));
        if (!mode.empty()) {
            signal.mm_type = mm_system_type_value(mode);
        }
        const auto& trade_payload = params.contains("trade")
            ? object_member_or_empty(params, "trade")
            : object_member_or_empty(params, "signal");
        if (sizing.contains("amount") && !trade_payload.contains("amount")) {
            nlohmann::json trade = nlohmann::json::object();
            trade["amount"] = sizing.at("amount");
            if (sizing.contains("currency")) {
                trade["currency"] = sizing.at("currency");
            }
            apply_amount(signal, trade);
        }
        const auto currency = string_value(sizing, "currency");
        if (!currency.empty() && signal.currency == CurrencyType::UNKNOWN) {
            signal.currency = currency_value(currency);
        }
        const auto step = int64_value(sizing, "step", signal.mm_step);
        if (step < std::numeric_limits<std::int32_t>::min() ||
            step > std::numeric_limits<std::int32_t>::max()) {
            throw std::invalid_argument("sizing.step is out of range.");
        }
        signal.mm_step = static_cast<std::int32_t>(step);
        signal.mm_group_id = int64_value(sizing, "group_id", signal.mm_group_id);
        signal.mm_group_hash = string_value(sizing, "group_hash", signal.mm_group_hash);
        signal.mm_group_name = string_value(sizing, "group_name", signal.mm_group_name);
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
        auto command = protocol_v1::detail::parse_canonical_trade_command(
            params,
            direct_trade_open);
        return std::move(command.signal);
    }

} // namespace optionx::bridges::metatrader_file::detail

#endif // OPTIONX_HEADER_BRIDGES_METATRADER_FILE_DETAIL_META_TRADER_FILE_BRIDGE_UTILS_HPP_INCLUDED
