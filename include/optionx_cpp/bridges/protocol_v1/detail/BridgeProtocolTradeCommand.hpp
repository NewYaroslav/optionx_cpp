#pragma once
#ifndef OPTIONX_HEADER_BRIDGES_PROTOCOL_V1_DETAIL_BRIDGE_PROTOCOL_TRADE_COMMAND_HPP_INCLUDED
#define OPTIONX_HEADER_BRIDGES_PROTOCOL_V1_DETAIL_BRIDGE_PROTOCOL_TRADE_COMMAND_HPP_INCLUDED

/// \file BridgeProtocolTradeCommand.hpp
/// \brief Defines canonical Bridge Protocol v1 trade-command parsing helpers.

namespace optionx::bridges::protocol_v1::detail {

    /// \brief Canonical business DTO shared by fingerprinting and dispatch.
    struct CanonicalTradeCommand {
        nlohmann::json payload;
        std::string fingerprint;
        std::unique_ptr<TradeSignal> signal;
    };

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

    inline MmSystemType mm_system_type_value(const std::string& value) {
        if (value.empty()) {
            return MmSystemType::NONE;
        }
        return to_enum<MmSystemType>(value);
    }

    inline std::int64_t unix_time_ms() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    inline void apply_amount(TradeSignal& signal, const nlohmann::json& trade) {
        if (!trade.is_object() || !trade.contains("amount")) {
            return;
        }
        const auto& amount = trade.at("amount");
        if (amount.is_object()) {
            signal.amount = double_value(amount, "value", signal.amount);
            const auto currency = string_value(amount, "currency");
            if (!currency.empty()) {
                signal.currency = canonical_currency_value(currency);
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

    inline void apply_routing(TradeSignal& signal, const nlohmann::json& params) {
        const auto& routing = object_member_or_empty(params, "routing");
        const auto platform_type = string_value(routing, "platform_type");
        if (!platform_type.empty()) {
            signal.platform_type = canonical_platform_type_value(platform_type);
        }
        const auto& selector = object_member_or_empty(routing, "selector");
        const auto account_id = int64_value(selector, "account_id", 0);
        if (account_id != 0) {
            signal.account_id = account_id;
        }
    }

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
            signal.currency = canonical_currency_value(currency);
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

    inline std::unique_ptr<TradeSignal> parse_trade_signal_from_canonical_payload(
            const nlohmann::json& payload,
            const bool direct_trade_open) {
        if (!payload.is_object()) {
            throw std::invalid_argument("Command params must be an object.");
        }

        const auto& trade = direct_trade_open
            ? object_member_or_self(payload, "trade")
            : object_member_or_self(payload, "signal");
        const auto& identity = object_member_or_empty(payload, "identity");

        auto signal = std::make_unique<TradeSignal>();
        signal->symbol = string_value(trade, "symbol");
        signal->signal_name = string_value(
            identity,
            "signal_name",
            string_value(trade, "signal_name", direct_trade_open ? "direct_trade_open" : ""));
        signal->unique_hash = string_value(
            identity,
            "unique_hash",
            string_value(trade, "unique_hash"));
        signal->unique_id = int64_value(identity, "unique_id", int64_value(trade, "unique_id", 0));
        signal->user_data = string_value(identity, "user_data", string_value(trade, "user_data"));
        signal->comment = string_value(trade, "comment");
        signal->account_id = int64_value(trade, "account_id", 0);
        signal->account_type = canonical_account_type_value(string_value(trade, "account_type"));
        signal->currency = canonical_currency_value(string_value(trade, "currency"));
        signal->option_type = canonical_option_type_value(string_value(trade, "option_type"));
        signal->order_type = canonical_order_type_value(string_value(trade, "order_type"));
        signal->refund = double_value(trade, "refund", 0.0);
        signal->min_payout = double_value(trade, "min_payout", 0.0);

        apply_amount(*signal, trade);
        apply_expiry(*signal, trade);
        apply_routing(*signal, payload);
        apply_sizing(*signal, payload);

        if (signal->symbol.empty()) {
            throw std::invalid_argument("Command trade symbol is required.");
        }
        if (signal->order_type == OrderType::UNKNOWN) {
            throw std::invalid_argument("Command order_type is required.");
        }
        if (!std::isfinite(signal->amount)) {
            throw std::invalid_argument("Command amount must be finite.");
        }
        if (direct_trade_open && signal->amount <= 0.0) {
            throw std::invalid_argument("trade.open amount must be positive.");
        }
        if (direct_trade_open && signal->option_type == OptionType::UNKNOWN) {
            throw std::invalid_argument("trade.open option_type is required.");
        }
        if (direct_trade_open && signal->duration == 0 && signal->expiry_time <= 0) {
            throw std::invalid_argument("trade.open expiry is required.");
        }
        return signal;
    }

    inline CanonicalTradeCommand parse_canonical_trade_command(
            const nlohmann::json& params,
            const bool direct_trade_open) {
        auto payload = canonical_trade_command_payload(params, direct_trade_open);
        auto fingerprint = payload.dump(-1);
        auto signal = parse_trade_signal_from_canonical_payload(payload, direct_trade_open);
        return CanonicalTradeCommand{
            std::move(payload),
            std::move(fingerprint),
            std::move(signal)
        };
    }

    inline CanonicalTradeCommand canonical_trade_command(
            const nlohmann::json& params,
            const bool direct_trade_open) {
        auto payload = canonical_trade_command_payload(params, direct_trade_open);
        auto fingerprint = payload.dump(-1);
        return CanonicalTradeCommand{
            std::move(payload),
            std::move(fingerprint),
            nullptr
        };
    }

    inline void attach_trade_signal(
            CanonicalTradeCommand& command,
            const bool direct_trade_open) {
        command.signal =
            parse_trade_signal_from_canonical_payload(command.payload, direct_trade_open);
    }

} // namespace optionx::bridges::protocol_v1::detail

#endif // OPTIONX_HEADER_BRIDGES_PROTOCOL_V1_DETAIL_BRIDGE_PROTOCOL_TRADE_COMMAND_HPP_INCLUDED
