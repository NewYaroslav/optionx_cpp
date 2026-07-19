#pragma once
#ifndef OPTIONX_HEADER_BRIDGES_BOT_BINARY_DETAIL_BOT_BINARY_PROTOCOL_HPP_INCLUDED
#define OPTIONX_HEADER_BRIDGES_BOT_BINARY_DETAIL_BOT_BINARY_PROTOCOL_HPP_INCLUDED

/// \file BotBinaryProtocol.hpp
/// \brief Formats OptionX trade commands for BotBinary/BinaryBot automation surfaces.

namespace optionx::bridges::bot_binary {

    /// \enum BotBinaryExpiryKind
    /// \brief Expiry shape supported by BotBinary command strings.
    enum class BotBinaryExpiryKind {
        DURATION, ///< Relative duration.
        END_TIME  ///< Absolute Unix expiry timestamp in seconds.
    };

    /// \enum BotBinaryTimeUnit
    /// \brief Time unit token used by BotBinary duration/endtime fields.
    enum class BotBinaryTimeUnit {
        SECONDS, ///< `s`.
        MINUTES, ///< `m`.
        HOURS    ///< `h`.
    };

    /// \struct BotBinaryAdapterConfig
    /// \brief Formatting settings for BotBinary HTTP and file signal surfaces.
    struct BotBinaryAdapterConfig {
        /// Base URL of the BotBinary WebRequest listener.
        std::string http_base_url = "http://127.0.0.2/";

        /// Whether to append the stable transport suffix to HTTP request values.
        bool include_suffix_in_http_request = false;
    };

    /// \struct BotBinaryTradeCommand
    /// \brief Broker-neutral trade command data required by the BotBinary adapter.
    struct BotBinaryTradeCommand {
        std::string symbol; ///< BotBinary symbol token, for example `frxEURAUD` or `R_25`.
        OrderType order_type = OrderType::UNKNOWN; ///< `BUY` becomes `CALL`, `SELL` becomes `PUT`.
        std::string amount_value; ///< Decimal stake string; kept as provided by the caller.
        BotBinaryExpiryKind expiry_kind = BotBinaryExpiryKind::DURATION; ///< Expiry field kind.
        std::uint64_t expiry_value = 0; ///< Duration value or Unix endtime in seconds.
        BotBinaryTimeUnit expiry_unit = BotBinaryTimeUnit::MINUTES; ///< Duration/endtime unit token.
        std::string idempotency_key; ///< Stable logical operation key owned by the caller.
        std::string transport_suffix; ///< Optional exact BotBinary suffix to reuse on retry.
    };

    /// \struct BotBinaryPreparedCommand
    /// \brief Stable BotBinary wire command values prepared before delivery.
    struct BotBinaryPreparedCommand {
        std::string idempotency_key; ///< Logical operation key used to derive default suffixes.
        std::string request_query_value; ///< Raw value for BotBinary `request=...`.
        std::string http_url; ///< Convenience URL using `BotBinaryAdapterConfig::http_base_url`.
        std::string file_name; ///< BotBinary file-signal filename with `.txt` extension.
        std::string transport_suffix; ///< Effective stable suffix embedded in the file name.
    };

    /// \brief Returns the BotBinary direction token for an OptionX order type.
    inline std::string bot_binary_direction_token(const OrderType order_type) {
        switch (order_type) {
        case OrderType::BUY:
            return "CALL";
        case OrderType::SELL:
            return "PUT";
        default:
            throw std::invalid_argument("BotBinary command requires BUY or SELL order_type.");
        }
    }

    /// \brief Returns the BotBinary time unit token.
    inline std::string bot_binary_time_unit_token(const BotBinaryTimeUnit unit) {
        switch (unit) {
        case BotBinaryTimeUnit::SECONDS:
            return "s";
        case BotBinaryTimeUnit::MINUTES:
            return "m";
        case BotBinaryTimeUnit::HOURS:
            return "h";
        default:
            throw std::invalid_argument("Invalid BotBinary time unit.");
        }
    }

    /// \brief Builds a duration command using the largest exact BotBinary unit.
    inline BotBinaryTradeCommand bot_binary_duration_command(
            std::string symbol,
            const OrderType order_type,
            std::string amount_value,
            const std::uint64_t duration_seconds,
            std::string idempotency_key) {
        if (duration_seconds == 0) {
            throw std::invalid_argument("BotBinary duration must be positive.");
        }

        BotBinaryTradeCommand command;
        command.symbol = std::move(symbol);
        command.order_type = order_type;
        command.amount_value = std::move(amount_value);
        command.expiry_kind = BotBinaryExpiryKind::DURATION;
        command.idempotency_key = std::move(idempotency_key);

        if (duration_seconds % 3600 == 0) {
            command.expiry_value = duration_seconds / 3600;
            command.expiry_unit = BotBinaryTimeUnit::HOURS;
        } else if (duration_seconds % 60 == 0) {
            command.expiry_value = duration_seconds / 60;
            command.expiry_unit = BotBinaryTimeUnit::MINUTES;
        } else {
            command.expiry_value = duration_seconds;
            command.expiry_unit = BotBinaryTimeUnit::SECONDS;
        }
        return command;
    }

    /// \brief Builds an absolute-endtime command.
    inline BotBinaryTradeCommand bot_binary_end_time_command(
            std::string symbol,
            const OrderType order_type,
            std::string amount_value,
            const std::uint64_t unix_end_time_seconds,
            std::string idempotency_key) {
        if (unix_end_time_seconds == 0) {
            throw std::invalid_argument("BotBinary endtime must be positive.");
        }

        BotBinaryTradeCommand command;
        command.symbol = std::move(symbol);
        command.order_type = order_type;
        command.amount_value = std::move(amount_value);
        command.expiry_kind = BotBinaryExpiryKind::END_TIME;
        command.expiry_value = unix_end_time_seconds;
        command.expiry_unit = BotBinaryTimeUnit::SECONDS;
        command.idempotency_key = std::move(idempotency_key);
        return command;
    }

    namespace detail {

        inline bool has_forbidden_bot_binary_char(
                const std::string& value,
                const bool allow_equals) {
            for (const unsigned char ch : value) {
                if (ch < 0x20 || ch == 0x7f) {
                    return true;
                }
                switch (ch) {
                case '=':
                    if (!allow_equals) {
                        return true;
                    }
                    break;
                case '/':
                case '\\':
                case ':':
                case '*':
                case '?':
                case '"':
                case '<':
                case '>':
                case '|':
                case '&':
                case '#':
                    return true;
                default:
                    break;
                }
            }
            return false;
        }

        inline void validate_bot_binary_token(
                const std::string& value,
                const char* field,
                const bool allow_equals = false) {
            if (value.empty()) {
                throw std::invalid_argument(std::string("BotBinary ") + field + " must not be empty.");
            }
            if (has_forbidden_bot_binary_char(value, allow_equals)) {
                throw std::invalid_argument(std::string("BotBinary ") + field + " contains unsafe characters.");
            }
        }

        inline std::string default_bot_binary_suffix(const std::string& idempotency_key) {
            return "ox_" + utils::Base36::encode_string(idempotency_key);
        }

        inline std::string append_bot_binary_request_query(
                std::string base_url,
                const std::string& request_query_value) {
            if (base_url.empty()) {
                throw std::invalid_argument("BotBinary HTTP base URL must not be empty.");
            }
            const auto separator = base_url.find('?') == std::string::npos
                ? '?'
                : (base_url.back() == '?' || base_url.back() == '&' ? '\0' : '&');
            if (separator != '\0') {
                base_url.push_back(separator);
            }
            base_url += "request=";
            base_url += request_query_value;
            return base_url;
        }

        inline std::string expiry_field_token(const BotBinaryExpiryKind kind) {
            switch (kind) {
            case BotBinaryExpiryKind::DURATION:
                return "duration";
            case BotBinaryExpiryKind::END_TIME:
                return "endtime";
            default:
                throw std::invalid_argument("Invalid BotBinary expiry kind.");
            }
        }

    } // namespace detail

    /// \brief Prepares stable BotBinary HTTP and file-signal command strings.
    /// \details The returned `file_name` and `request_query_value` are the
    /// exact transport values that a durable runtime bridge should persist for
    /// retries of the same logical operation.
    inline BotBinaryPreparedCommand prepare_bot_binary_command(
            BotBinaryTradeCommand command,
            BotBinaryAdapterConfig config = {}) {
        detail::validate_bot_binary_token(command.symbol, "symbol");
        detail::validate_bot_binary_token(command.amount_value, "amount");
        detail::validate_bot_binary_token(command.idempotency_key, "idempotency_key", true);
        if (command.expiry_value == 0) {
            throw std::invalid_argument("BotBinary expiry value must be positive.");
        }
        if (command.expiry_kind == BotBinaryExpiryKind::END_TIME &&
            command.expiry_unit != BotBinaryTimeUnit::SECONDS) {
            throw std::invalid_argument("BotBinary endtime expiry must use seconds.");
        }

        auto suffix = command.transport_suffix.empty()
            ? detail::default_bot_binary_suffix(command.idempotency_key)
            : std::move(command.transport_suffix);
        detail::validate_bot_binary_token(suffix, "transport_suffix", true);

        const auto direction = bot_binary_direction_token(command.order_type);
        const auto expiry_field = detail::expiry_field_token(command.expiry_kind);
        const auto unit = bot_binary_time_unit_token(command.expiry_unit);

        std::string base =
            command.symbol + "=" +
            direction + "=" +
            command.amount_value + "=" +
            expiry_field + "=" +
            std::to_string(command.expiry_value) + "=" +
            unit + "=";

        BotBinaryPreparedCommand prepared;
        prepared.idempotency_key = std::move(command.idempotency_key);
        prepared.transport_suffix = std::move(suffix);
        prepared.request_query_value = base;
        if (config.include_suffix_in_http_request) {
            prepared.request_query_value += prepared.transport_suffix;
        }
        prepared.http_url = detail::append_bot_binary_request_query(
            std::move(config.http_base_url),
            prepared.request_query_value);
        prepared.file_name = base + prepared.transport_suffix + ".txt";
        return prepared;
    }

    /// \brief Prepares BotBinary values from a basic `TradeRequest` snapshot.
    /// \param request Source domain trade request.
    /// \param amount_value Decimal stake string to preserve wire scale.
    /// \param idempotency_key Stable logical operation key to use on retries.
    inline BotBinaryPreparedCommand prepare_bot_binary_command(
            const TradeRequest& request,
            std::string amount_value,
            std::string idempotency_key,
            BotBinaryAdapterConfig config = {}) {
        if (request.duration == 0 && request.expiry_time <= 0) {
            throw std::invalid_argument("BotBinary TradeRequest requires duration or expiry_time.");
        }

        auto command = request.expiry_time > 0
            ? bot_binary_end_time_command(
                request.symbol,
                request.order_type,
                std::move(amount_value),
                static_cast<std::uint64_t>(request.expiry_time),
                std::move(idempotency_key))
            : bot_binary_duration_command(
                request.symbol,
                request.order_type,
                std::move(amount_value),
                request.duration,
                std::move(idempotency_key));
        return prepare_bot_binary_command(std::move(command), std::move(config));
    }

} // namespace optionx::bridges::bot_binary

#endif // OPTIONX_HEADER_BRIDGES_BOT_BINARY_DETAIL_BOT_BINARY_PROTOCOL_HPP_INCLUDED
