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
        std::string request_query_value; ///< Raw BotBinary `request` value before URL encoding.
        std::string http_url; ///< Convenience URL with percent-encoded `request` query value.
        std::string file_name; ///< BotBinary file-signal filename with `.txt` extension.
        std::string transport_suffix; ///< Effective stable suffix embedded in the file name.
    };

    /// \struct BotBinaryParsedCommand
    /// \brief Parsed BotBinary/BinaryBot command accepted by legacy indicators.
    struct BotBinaryParsedCommand {
        std::string symbol; ///< BotBinary symbol token, for example `frxEURAUD` or `R_25`.
        OrderType order_type = OrderType::UNKNOWN; ///< `CALL` becomes `BUY`, `PUT` becomes `SELL`.
        std::string amount_value; ///< Original positive decimal stake token.
        BotBinaryExpiryKind expiry_kind = BotBinaryExpiryKind::DURATION; ///< Expiry field kind.
        std::uint64_t expiry_value = 0; ///< Duration value or Unix endtime in seconds.
        BotBinaryTimeUnit expiry_unit = BotBinaryTimeUnit::MINUTES; ///< Duration/endtime unit token.
        std::string transport_suffix; ///< Optional trailing BotBinary suffix from file signals or HTTP request.
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

    /// \brief Parses a BotBinary direction token into an OptionX order type.
    inline OrderType bot_binary_order_type_from_direction(const std::string& direction) {
        if (direction == "CALL") {
            return OrderType::BUY;
        }
        if (direction == "PUT") {
            return OrderType::SELL;
        }
        throw std::invalid_argument("BotBinary command requires CALL or PUT direction.");
    }

    /// \brief Parses a BotBinary time unit token.
    inline BotBinaryTimeUnit bot_binary_time_unit_from_token(const std::string& unit) {
        if (unit == "s") {
            return BotBinaryTimeUnit::SECONDS;
        }
        if (unit == "m") {
            return BotBinaryTimeUnit::MINUTES;
        }
        if (unit == "h") {
            return BotBinaryTimeUnit::HOURS;
        }
        throw std::invalid_argument("Invalid BotBinary time unit token.");
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

        inline bool is_ascii_digit(const char ch) noexcept {
            return ch >= '0' && ch <= '9';
        }

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

        inline void validate_optionx_idempotency_key(const std::string& value) {
            if (value.empty()) {
                throw std::invalid_argument("BotBinary idempotency_key must not be empty.");
            }
            if (value.size() > 512) {
                throw std::invalid_argument("BotBinary idempotency_key is too long.");
            }
            for (const unsigned char ch : value) {
                if (ch < 0x20 || ch == 0x7f) {
                    throw std::invalid_argument("BotBinary idempotency_key contains control characters.");
                }
            }
        }

        inline void validate_positive_decimal_amount(const std::string& value) {
            if (value.empty()) {
                throw std::invalid_argument("BotBinary amount must not be empty.");
            }

            bool seen_dot = false;
            bool seen_digit = false;
            bool seen_fraction_digit = false;
            bool seen_nonzero_digit = false;
            for (const char ch : value) {
                if (is_ascii_digit(ch)) {
                    seen_digit = true;
                    if (seen_dot) {
                        seen_fraction_digit = true;
                    }
                    if (ch != '0') {
                        seen_nonzero_digit = true;
                    }
                    continue;
                }
                if (ch == '.' && !seen_dot) {
                    if (!seen_digit) {
                        throw std::invalid_argument("BotBinary amount must have a digit before decimal point.");
                    }
                    seen_dot = true;
                    continue;
                }
                throw std::invalid_argument("BotBinary amount must be a positive ASCII decimal string.");
            }

            if (!seen_digit || !seen_nonzero_digit || (seen_dot && !seen_fraction_digit)) {
                throw std::invalid_argument("BotBinary amount must be positive.");
            }
        }

        inline std::string base36_uint64(std::uint64_t value) {
            static constexpr char alphabet[] = "0123456789abcdefghijklmnopqrstuvwxyz";
            std::string result;
            do {
                result.insert(result.begin(), alphabet[value % 36u]);
                value /= 36u;
            } while (value > 0u);
            return result;
        }

        inline std::uint64_t fnv1a64(const std::string& value) noexcept {
            std::uint64_t hash = 14695981039346656037ull;
            for (const unsigned char ch : value) {
                hash ^= ch;
                hash *= 1099511628211ull;
            }
            return hash;
        }

        inline std::string default_bot_binary_suffix(const std::string& idempotency_key) {
            return "ox_" + base36_uint64(fnv1a64(idempotency_key));
        }

        inline bool is_unreserved_query_char(const unsigned char ch) noexcept {
            return (ch >= 'A' && ch <= 'Z') ||
                   (ch >= 'a' && ch <= 'z') ||
                   (ch >= '0' && ch <= '9') ||
                   ch == '-' ||
                   ch == '.' ||
                   ch == '_' ||
                   ch == '~';
        }

        inline std::string percent_encode_query_value(const std::string& value) {
            static constexpr char hex[] = "0123456789ABCDEF";
            std::string result;
            result.reserve(value.size());
            for (const unsigned char ch : value) {
                if (is_unreserved_query_char(ch)) {
                    result.push_back(static_cast<char>(ch));
                    continue;
                }
                result.push_back('%');
                result.push_back(hex[(ch >> 4u) & 0x0fu]);
                result.push_back(hex[ch & 0x0fu]);
            }
            return result;
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
            base_url += percent_encode_query_value(request_query_value);
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

        inline BotBinaryExpiryKind expiry_kind_from_token(const std::string& token) {
            if (token == "duration") {
                return BotBinaryExpiryKind::DURATION;
            }
            if (token == "endtime") {
                return BotBinaryExpiryKind::END_TIME;
            }
            throw std::invalid_argument("Invalid BotBinary expiry field.");
        }

        inline std::uint64_t parse_positive_uint64(
                const std::string& value,
                const char* field) {
            if (value.empty()) {
                throw std::invalid_argument(std::string("BotBinary ") + field + " must not be empty.");
            }

            std::uint64_t result = 0;
            for (const char ch : value) {
                if (!is_ascii_digit(ch)) {
                    throw std::invalid_argument(std::string("BotBinary ") + field + " must be an unsigned integer.");
                }
                const auto digit = static_cast<std::uint64_t>(ch - '0');
                if (result > ((std::numeric_limits<std::uint64_t>::max)() - digit) / 10u) {
                    throw std::invalid_argument(std::string("BotBinary ") + field + " is too large.");
                }
                result = result * 10u + digit;
            }
            if (result == 0u) {
                throw std::invalid_argument(std::string("BotBinary ") + field + " must be positive.");
            }
            return result;
        }

        inline std::vector<std::string> split_bot_binary_tokens(const std::string& value) {
            std::vector<std::string> tokens;
            std::size_t start = 0;
            while (start <= value.size()) {
                const auto pos = value.find('=', start);
                if (pos == std::string::npos) {
                    tokens.push_back(value.substr(start));
                    break;
                }
                tokens.push_back(value.substr(start, pos - start));
                start = pos + 1;
            }
            return tokens;
        }

        inline std::string join_suffix_tokens(
                const std::vector<std::string>& tokens,
                const std::size_t start) {
            std::string suffix;
            for (std::size_t i = start; i < tokens.size(); ++i) {
                if (i != start) {
                    suffix.push_back('=');
                }
                suffix += tokens[i];
            }
            return suffix;
        }

        inline BotBinaryParsedCommand parse_bot_binary_value(
                const std::string& value,
                const bool require_suffix) {
            const auto tokens = split_bot_binary_tokens(value);
            if (tokens.size() < 7u) {
                throw std::invalid_argument("BotBinary command has too few fields.");
            }

            BotBinaryParsedCommand command;
            command.symbol = tokens[0];
            command.order_type = bot_binary_order_type_from_direction(tokens[1]);
            command.amount_value = tokens[2];
            command.expiry_kind = expiry_kind_from_token(tokens[3]);
            command.expiry_value = parse_positive_uint64(tokens[4], "expiry value");
            command.expiry_unit = bot_binary_time_unit_from_token(tokens[5]);
            command.transport_suffix = join_suffix_tokens(tokens, 6u);

            validate_bot_binary_token(command.symbol, "symbol");
            validate_positive_decimal_amount(command.amount_value);
            if (command.expiry_kind == BotBinaryExpiryKind::END_TIME &&
                command.expiry_unit != BotBinaryTimeUnit::SECONDS) {
                throw std::invalid_argument("BotBinary endtime expiry must use seconds.");
            }
            if (require_suffix && command.transport_suffix.empty()) {
                throw std::invalid_argument("BotBinary file signal requires a transport suffix.");
            }
            if (!command.transport_suffix.empty()) {
                validate_bot_binary_token(command.transport_suffix, "transport_suffix", true);
            }
            return command;
        }

        inline int hex_value(const char ch) noexcept {
            if (ch >= '0' && ch <= '9') return ch - '0';
            if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
            if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
            return -1;
        }

        inline std::string percent_decode_query_value(const std::string& value) {
            std::string result;
            result.reserve(value.size());
            for (std::size_t i = 0; i < value.size(); ++i) {
                const char ch = value[i];
                if (ch != '%') {
                    result.push_back(ch);
                    continue;
                }
                if (i + 2u >= value.size()) {
                    throw std::invalid_argument("BotBinary request contains an incomplete percent escape.");
                }
                const int hi = hex_value(value[i + 1u]);
                const int lo = hex_value(value[i + 2u]);
                if (hi < 0 || lo < 0) {
                    throw std::invalid_argument("BotBinary request contains an invalid percent escape.");
                }
                result.push_back(static_cast<char>((hi << 4) | lo));
                i += 2u;
            }
            return result;
        }

        inline std::string extract_request_query_value(const std::string& value) {
            const auto query_start = value.find('?');
            auto query = query_start == std::string::npos
                ? value
                : value.substr(query_start + 1u);
            const auto fragment_start = query.find('#');
            if (fragment_start != std::string::npos) {
                query = query.substr(0, fragment_start);
            }

            static const std::string key = "request=";
            std::size_t part_start = 0;
            while (part_start <= query.size()) {
                const auto part_end = query.find('&', part_start);
                const auto part = part_end == std::string::npos
                    ? query.substr(part_start)
                    : query.substr(part_start, part_end - part_start);
                if (part.compare(0u, key.size(), key) == 0) {
                    return percent_decode_query_value(part.substr(key.size()));
                }
                if (part_end == std::string::npos) {
                    break;
                }
                part_start = part_end + 1u;
            }

            return value;
        }

        inline std::string file_leaf_name(const std::string& path) {
            const auto slash = path.find_last_of("/\\");
            return slash == std::string::npos ? path : path.substr(slash + 1u);
        }

        inline std::string strip_txt_extension(const std::string& name) {
            static const std::string extension = ".txt";
            if (name.size() < extension.size() ||
                name.compare(name.size() - extension.size(), extension.size(), extension) != 0) {
                throw std::invalid_argument("BotBinary file signal name must end with .txt.");
            }
            return name.substr(0, name.size() - extension.size());
        }

    } // namespace detail

    /// \brief Parses a raw BotBinary `request` query value.
    inline BotBinaryParsedCommand parse_bot_binary_request_value(
            const std::string& request_query_value) {
        return detail::parse_bot_binary_value(request_query_value, false);
    }

    /// \brief Parses a full BotBinary HTTP URL or query string containing `request=`.
    /// \details If no `request=` parameter is found, the input is treated as a raw
    /// request value. This keeps the helper usable for both HTTP servers and tests.
    inline BotBinaryParsedCommand parse_bot_binary_http_request(
            const std::string& url_or_query) {
        return parse_bot_binary_request_value(
            detail::extract_request_query_value(url_or_query));
    }

    /// \brief Parses a BotBinary file-signal `.txt` filename.
    inline BotBinaryParsedCommand parse_bot_binary_file_signal_name(
            const std::string& file_name) {
        return detail::parse_bot_binary_value(
            detail::strip_txt_extension(detail::file_leaf_name(file_name)),
            true);
    }

    /// \brief Returns duration in seconds for a parsed duration command.
    inline std::uint64_t bot_binary_duration_seconds(
            const BotBinaryParsedCommand& command) {
        if (command.expiry_kind != BotBinaryExpiryKind::DURATION) {
            throw std::invalid_argument("BotBinary command is not a duration command.");
        }

        std::uint64_t multiplier = 1u;
        switch (command.expiry_unit) {
        case BotBinaryTimeUnit::SECONDS:
            multiplier = 1u;
            break;
        case BotBinaryTimeUnit::MINUTES:
            multiplier = 60u;
            break;
        case BotBinaryTimeUnit::HOURS:
            multiplier = 3600u;
            break;
        default:
            throw std::invalid_argument("Invalid BotBinary time unit.");
        }

        if (command.expiry_value > (std::numeric_limits<std::uint64_t>::max)() / multiplier) {
            throw std::invalid_argument("BotBinary duration is too large.");
        }
        return command.expiry_value * multiplier;
    }

    /// \brief Converts a parsed BotBinary command to an OptionX trade signal snapshot.
    inline TradeSignal bot_binary_to_trade_signal(
            const BotBinaryParsedCommand& command,
            std::string signal_name = "bot_binary") {
        TradeSignal signal;
        signal.symbol = command.symbol;
        signal.signal_name = std::move(signal_name);
        signal.order_type = command.order_type;
        signal.amount = std::stod(command.amount_value);

        if (command.expiry_kind == BotBinaryExpiryKind::DURATION) {
            signal.option_type = OptionType::SPRINT;
            const auto seconds = bot_binary_duration_seconds(command);
            if (seconds > (std::numeric_limits<std::uint32_t>::max)()) {
                throw std::invalid_argument("BotBinary duration is too large for TradeSignal.");
            }
            signal.duration = static_cast<std::uint32_t>(seconds);
        } else {
            signal.option_type = OptionType::CLASSIC;
            if (command.expiry_value > static_cast<std::uint64_t>(
                    (std::numeric_limits<std::int64_t>::max)())) {
                throw std::invalid_argument("BotBinary endtime is too large for TradeSignal.");
            }
            signal.expiry_time = static_cast<std::int64_t>(command.expiry_value);
        }

        return signal;
    }

    /// \brief Converts a parsed BotBinary command to an executable trade request snapshot.
    inline TradeRequest bot_binary_to_trade_request(
            const BotBinaryParsedCommand& command,
            std::string signal_name = "bot_binary") {
        return bot_binary_to_trade_signal(command, std::move(signal_name)).to_trade_request();
    }

    /// \brief Prepares stable BotBinary HTTP and file-signal command strings.
    /// \details The returned `file_name` and raw `request_query_value` are the
    /// exact transport values that a durable runtime bridge should persist for
    /// retries of the same logical operation. The convenience `http_url`
    /// percent-encodes `request_query_value` for HTTP delivery.
    inline BotBinaryPreparedCommand prepare_bot_binary_command(
            BotBinaryTradeCommand command,
            BotBinaryAdapterConfig config = {}) {
        detail::validate_bot_binary_token(command.symbol, "symbol");
        detail::validate_positive_decimal_amount(command.amount_value);
        detail::validate_optionx_idempotency_key(command.idempotency_key);
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
