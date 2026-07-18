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
