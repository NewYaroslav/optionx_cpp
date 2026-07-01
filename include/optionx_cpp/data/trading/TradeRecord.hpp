#pragma once
#ifndef _OPTIONX_TRADE_RECORD_HPP_INCLUDED
#define _OPTIONX_TRADE_RECORD_HPP_INCLUDED

/// \file TradeRecord.hpp
/// \brief Defines the TradeRecord DTO used for persistent trade storage.

#include "SpreadPack.hpp"

namespace optionx {

    /// \class TradeRecord
    /// \brief Flat persistent representation of a trade for MDBX key-value storage.
    class TradeRecord {
    public:
        // Storage identity
        std::uint32_t trade_id = 0;           ///< Linear persistent trade ID; 0 means "not assigned".
        std::uint32_t signal_id = 0;          ///< Persistent signal ID; 0 means "not attached to a signal".
        std::int64_t unique_id = 0;          ///< User-defined request/signal correlation ID; 0 means "not assigned".
        std::string unique_hash;             ///< User-defined request/signal correlation hash.
        std::int64_t account_id = 0;          ///< Trading account ID.
        std::int64_t option_id = 0;           ///< Numeric broker-side order ID.
        std::string option_hash;              ///< String broker-side order ID.

        // Context
        PlatformType platform_type = PlatformType::UNKNOWN; ///< Trading platform.
        AccountType account_type = AccountType::UNKNOWN;    ///< Account type.
        CurrencyType currency = CurrencyType::UNKNOWN;      ///< Trade currency.
        std::string symbol;                                 ///< Trading symbol.
        std::string signal_name;                            ///< Signal or strategy name.
        std::string user_data;                              ///< User-defined request data.
        std::string comment;                                ///< Trade comment.

        // Trade parameters and result
        OptionType option_type = OptionType::UNKNOWN;       ///< Binary option type.
        OrderType order_type = OrderType::UNKNOWN;          ///< Direction.
        double amount = 0.0;                                ///< Trade amount.
        double refund = 0.0;                                ///< Refund ratio.
        double min_payout = 0.0;                            ///< Minimum acceptable payout.
        double payout = 0.0;                                ///< Actual payout ratio.
        double profit = 0.0;                                ///< Final or current profit/loss.
        double open_balance = 0.0;                          ///< Account balance before opening the trade.
        double close_balance = 0.0;                         ///< Known or estimated close-equivalent balance.

        // State and error details
        TradeState trade_state = TradeState::UNKNOWN;       ///< Current trade lifecycle state.
        TradeState live_state = TradeState::UNKNOWN;        ///< Live calculated state.
        TradeErrorCode error_code = TradeErrorCode::SUCCESS;///< Error code.
        std::string error_desc;                             ///< Error description.

        // Prices and timing, all dates are milliseconds.
        double open_price = 0.0;                            ///< Opening price.
        double close_price = 0.0;                           ///< Closing/latest price.
        std::int64_t delay = 0;                             ///< Entry delay in milliseconds.
        std::int64_t ping = 0;                              ///< Ping estimate in milliseconds.
        std::int64_t place_date = 0;                        ///< Request creation timestamp.
        std::int64_t send_date = 0;                         ///< Broker send timestamp.
        std::int64_t open_date = 0;                         ///< Trade open timestamp.
        std::int64_t close_date = 0;                        ///< Planned or known close timestamp (classic expiry, sprint open + duration).
        std::uint32_t duration = 0;                         ///< Requested duration in seconds; 0 means not specified.

        // Money management and extensibility
        MmSystemType mm_type = MmSystemType::NONE;          ///< Money management strategy.
        std::int32_t mm_step = 0;                           ///< Generic money management step.
        std::int64_t mm_group_id = 0;                       ///< Numeric money management group.
        std::string mm_group_hash;                          ///< Hash of the money management group.
        std::string mm_group_name;                          ///< Human-readable group name.
        std::string mm_params_json;                         ///< Serialized money management params.
        std::string decision_params_json;                   ///< Serialized decision params.
        std::string metadata_json;                          ///< Future extension data.

        // Flags
        std::uint8_t flags = 0;                             ///< Bit flags for grouping and optional snapshots.
        static constexpr std::uint8_t FLAG_LAST_IN_GROUP = 0x01;
        static constexpr std::uint8_t FLAG_HAS_OPEN_BALANCE = 0x02;
        static constexpr std::uint8_t FLAG_HAS_CLOSE_BALANCE = 0x04;

        /// \brief Returns true if this record is the last in its money-management group.
        bool last_in_group() const noexcept { return (flags & FLAG_LAST_IN_GROUP) != 0; }

        /// \brief Returns true when open_balance contains an explicit snapshot.
        bool has_open_balance() const noexcept { return (flags & FLAG_HAS_OPEN_BALANCE) != 0; }

        /// \brief Returns true when close_balance contains a close-equivalent value.
        bool has_close_balance() const noexcept { return (flags & FLAG_HAS_CLOSE_BALANCE) != 0; }

        /// \brief Sets or clears the last-in-group flag.
        void set_last_in_group(bool v) noexcept {
            flags = v ? (flags | FLAG_LAST_IN_GROUP) : (flags & ~FLAG_LAST_IN_GROUP);
        }

        /// \brief Stores the account balance before opening the trade.
        void set_open_balance(double value) noexcept {
            open_balance = value;
            flags |= FLAG_HAS_OPEN_BALANCE;
        }

        /// \brief Stores the known or estimated close-equivalent balance.
        void set_close_balance(double value) noexcept {
            close_balance = value;
            flags |= FLAG_HAS_CLOSE_BALANCE;
        }

        // Spread
        SpreadPack spread;                                   ///< Packed open/close spread data.

        /// \brief Copies request-side fields into this record.
        void assign_request(const TradeRequest& request) {
            trade_id = request.trade_id;
            signal_id = request.signal_id;
            unique_id = request.unique_id;
            unique_hash = request.unique_hash;
            account_id = request.account_id;
            account_type = request.account_type;
            currency = request.currency;
            symbol = request.symbol;
            signal_name = request.signal_name;
            user_data = request.user_data;
            comment = request.comment;
            option_type = request.option_type;
            order_type = request.order_type;
            amount = request.amount;
            refund = request.refund;
            min_payout = request.min_payout;
            duration = request.duration;
            if (request.expiry_time > 0) {
                close_date = request.expiry_time * 1000;
            }
        }

        /// \brief Applies a result snapshot to this record.
        /// \details Use this for a normal TradeResult that represents the
        /// current complete result view. Request-derived identity and account
        /// context are preserved when corresponding result fields are
        /// unspecified. Result-state fields are otherwise copied as-is,
        /// including default values.
        void apply_result_snapshot(const TradeResult& result) {
            if (result.trade_id > 0) trade_id = result.trade_id;
            if (result.option_id != 0) option_id = result.option_id;
            if (!result.option_hash.empty()) option_hash = result.option_hash;
            if (result.amount > 0.0) amount = result.amount;
            payout = result.payout;
            profit = result.profit;
            if (result.has_open_balance()) {
                set_open_balance(result.open_balance);
            }
            if (result.has_close_balance()) {
                set_close_balance(result.close_balance);
            } else if (has_open_balance() && result_has_balance_projection(result)) {
                set_close_balance(open_balance + result.profit);
            }
            open_price = result.open_price;
            close_price = result.close_price;
            delay = result.delay;
            ping = result.ping;
            place_date = result.place_date;
            send_date = result.send_date;
            open_date = result.open_date;
            if (result.close_date > 0 || close_date == 0) {
                close_date = result.close_date;
            }
            trade_state = result.trade_state;
            live_state = result.live_state;
            error_code = result.error_code;
            error_desc = result.error_desc;
            if (result.account_type != AccountType::UNKNOWN) account_type = result.account_type;
            if (result.currency != CurrencyType::UNKNOWN) currency = result.currency;
            if (result.platform_type != PlatformType::UNKNOWN) platform_type = result.platform_type;
            if (result.spread.raw != 0 || result.spread.digits != 0) spread = result.spread;
        }

        /// \brief Backward-compatible alias for apply_result_snapshot().
        void assign_result(const TradeResult& result) {
            apply_result_snapshot(result);
        }

        /// \brief Selectively merges known fields from a result patch.
        /// \details TradeResult does not provide presence flags for every
        /// scalar field. This method therefore updates only fields that can be
        /// distinguished from their default sentinel values. It is intended for
        /// recovery/status-fixer flows, not for applying a complete broker
        /// result snapshot.
        /// \return True if any field was updated.
        bool merge_result_patch(const TradeResult& result) {
            bool updated = false;

            if (result.trade_id > 0 && trade_id != result.trade_id) {
                trade_id = result.trade_id;
                updated = true;
            }
            if (result.option_id != 0 && option_id != result.option_id) {
                option_id = result.option_id;
                updated = true;
            }
            if (!result.option_hash.empty() && option_hash != result.option_hash) {
                option_hash = result.option_hash;
                updated = true;
            }
            if (result.amount > 0.0 && amount != result.amount) {
                amount = result.amount;
                updated = true;
            }
            if (result.payout != 0.0 && payout != result.payout) {
                payout = result.payout;
                updated = true;
            }
            if (result.profit != 0.0 ||
                    result.trade_state == TradeState::STANDOFF ||
                    result.trade_state == TradeState::REFUND) {
                if (profit != result.profit) {
                    profit = result.profit;
                    updated = true;
                }
            }
            if (result.has_open_balance()) {
                const bool changed = !has_open_balance() || open_balance != result.open_balance;
                set_open_balance(result.open_balance);
                updated = updated || changed;
            }
            if (result.has_close_balance()) {
                const bool changed = !has_close_balance() || close_balance != result.close_balance;
                set_close_balance(result.close_balance);
                updated = updated || changed;
            } else if (!has_close_balance() && has_open_balance() && result_has_balance_projection(result)) {
                set_close_balance(open_balance + result.profit);
                updated = true;
            }
            if (result.open_price != 0.0 && open_price != result.open_price) {
                open_price = result.open_price;
                updated = true;
            }
            if (result.close_price != 0.0 && close_price != result.close_price) {
                close_price = result.close_price;
                updated = true;
            }
            if (result.delay != 0 && delay != result.delay) {
                delay = result.delay;
                updated = true;
            }
            if (result.ping != 0 && ping != result.ping) {
                ping = result.ping;
                updated = true;
            }
            if (result.place_date > 0 && place_date != result.place_date) {
                place_date = result.place_date;
                updated = true;
            }
            if (result.send_date > 0 && send_date != result.send_date) {
                send_date = result.send_date;
                updated = true;
            }
            if (result.open_date > 0 && open_date != result.open_date) {
                open_date = result.open_date;
                updated = true;
            }
            if (result.close_date > 0 && close_date != result.close_date) {
                close_date = result.close_date;
                updated = true;
            }
            if (result.trade_state != TradeState::UNKNOWN && trade_state != result.trade_state) {
                trade_state = result.trade_state;
                updated = true;
            }
            if (result.live_state != TradeState::UNKNOWN && live_state != result.live_state) {
                live_state = result.live_state;
                updated = true;
            }
            if (result.error_code != TradeErrorCode::SUCCESS && error_code != result.error_code) {
                error_code = result.error_code;
                updated = true;
            }
            if (!result.error_desc.empty() && error_desc != result.error_desc) {
                error_desc = result.error_desc;
                updated = true;
            }
            if (result.account_type != AccountType::UNKNOWN && account_type != result.account_type) {
                account_type = result.account_type;
                updated = true;
            }
            if (result.currency != CurrencyType::UNKNOWN && currency != result.currency) {
                currency = result.currency;
                updated = true;
            }
            if (result.platform_type != PlatformType::UNKNOWN && platform_type != result.platform_type) {
                platform_type = result.platform_type;
                updated = true;
            }
            if (result.spread.raw != 0 || result.spread.digits != 0) {
                if (spread.raw != result.spread.raw || spread.digits != result.spread.digits) {
                    spread = result.spread;
                    updated = true;
                }
            }

            return updated;
        }

        /// \brief Copies request and money-management fields from a signal.
        void assign_signal(const TradeSignal& signal) {
            assign_request(signal.to_trade_request());
            mm_type = signal.mm_type;
            mm_step = signal.mm_step;
            mm_group_id = signal.mm_group_id;
            mm_group_hash = signal.mm_group_hash;
            mm_group_name = signal.mm_group_name;
            mm_params_json = signal.mm_params ? signal.mm_params->to_json().dump() : std::string();
            decision_params_json = signal.decision_params ? signal.decision_params->to_json().dump() : std::string();
        }

        /// \brief Builds a record from a trade request.
        static TradeRecord from_trade(const TradeRequest& request) {
            TradeRecord record;
            record.assign_request(request);
            return record;
        }

        /// \brief Builds a record from a request and result.
        static TradeRecord from_trade(const TradeRequest& request, const TradeResult& result) {
            TradeRecord record;
            record.assign_request(request);
            record.apply_result_snapshot(result);
            return record;
        }

        /// \brief Builds a record from a signal.
        static TradeRecord from_trade(const TradeSignal& signal) {
            TradeRecord record;
            record.assign_signal(signal);
            return record;
        }

        /// \brief Builds a record from a signal and result.
        static TradeRecord from_trade(const TradeSignal& signal, const TradeResult& result) {
            TradeRecord record;
            record.assign_signal(signal);
            record.apply_result_snapshot(result);
            return record;
        }

        /// \brief Returns true when the record has a broker-side order identity.
        bool has_broker_identity() const noexcept {
            return option_id != 0 || !option_hash.empty();
        }

        /// \brief Checks whether two records appear to refer to the same broker-side trade.
        bool same_broker_identity(const TradeRecord& other) const noexcept {
            if (!has_broker_identity() || !other.has_broker_identity()) return false;
            if (!same_known(platform_type, other.platform_type, PlatformType::UNKNOWN)) return false;
            if (!same_known(account_type, other.account_type, AccountType::UNKNOWN)) return false;
            if (!same_known(account_id, other.account_id, std::int64_t{0})) return false;
            if (!same_known(symbol, other.symbol)) return false;
            if (option_id != 0 && other.option_id != 0 && option_id != other.option_id) return false;
            if (!option_hash.empty() && !other.option_hash.empty() && option_hash != other.option_hash) return false;

            const bool same_numeric_id = option_id != 0 &&
                                         other.option_id != 0 &&
                                         option_id == other.option_id;
            const bool same_string_id = !option_hash.empty() &&
                                        !other.option_hash.empty() &&
                                        option_hash == other.option_hash;
            return same_numeric_id || same_string_id;
        }

        /// \brief Serializes the record using the current binary storage format.
        std::vector<std::uint8_t> to_bytes() const {
            std::vector<std::uint8_t> bytes;
            bytes.reserve(512 + unique_hash.size() + option_hash.size() +
                          symbol.size() + signal_name.size() + user_data.size() +
                          comment.size() + error_desc.size() + mm_group_hash.size() +
                          mm_group_name.size() + mm_params_json.size() +
                          decision_params_json.size() + metadata_json.size());

            append_value(bytes, kBinaryMagic);
            append_value(bytes, kBinaryVersion);

            append_value(bytes, trade_id);
            append_value(bytes, signal_id);
            append_value(bytes, unique_id);
            append_string(bytes, unique_hash);
            append_value(bytes, account_id);
            append_value(bytes, option_id);
            append_string(bytes, option_hash);

            append_enum8(bytes, platform_type);
            append_enum8(bytes, account_type);
            append_enum8(bytes, currency);
            append_string(bytes, symbol);
            append_string(bytes, signal_name);
            append_string(bytes, user_data);
            append_string(bytes, comment);

            append_enum8(bytes, option_type);
            append_enum8(bytes, order_type);
            append_value(bytes, amount);
            append_value(bytes, refund);
            append_value(bytes, min_payout);
            append_value(bytes, payout);
            append_value(bytes, profit);
            append_value(bytes, open_balance);
            append_value(bytes, close_balance);

            append_enum8(bytes, trade_state);
            append_enum8(bytes, live_state);
            append_enum8(bytes, error_code);
            append_string(bytes, error_desc);

            append_value(bytes, open_price);
            append_value(bytes, close_price);
            append_value(bytes, delay);
            append_value(bytes, ping);
            append_value(bytes, place_date);
            append_value(bytes, send_date);
            append_value(bytes, open_date);
            append_value(bytes, close_date);
            append_value(bytes, duration);

            append_enum8(bytes, mm_type);
            append_value(bytes, mm_step);
            append_value(bytes, mm_group_id);
            append_string(bytes, mm_group_hash);
            append_string(bytes, mm_group_name);
            append_string(bytes, mm_params_json);
            append_string(bytes, decision_params_json);
            append_string(bytes, metadata_json);

            append_value(bytes, flags);
            append_value(bytes, spread.raw);
            append_value(bytes, spread.digits);

            return bytes;
        }

        /// \brief Deserializes a trade record serialized by the current binary storage format.
        static TradeRecord from_bytes(const void* data, std::size_t size) {
            BinaryReader reader(data, size);

            const auto magic = reader.read<std::uint32_t>();
            if (magic != kBinaryMagic) {
                throw std::runtime_error("TradeRecord::from_bytes: invalid magic");
            }

            const auto version = reader.read<std::uint16_t>();
            if (version != kBinaryVersion) {
                throw std::runtime_error("TradeRecord::from_bytes: unsupported version");
            }

            TradeRecord record;
            record.trade_id = reader.read<std::uint32_t>();
            record.signal_id = reader.read<std::uint32_t>();
            record.unique_id = reader.read<std::int64_t>();
            record.unique_hash = reader.read_string();
            record.account_id = reader.read<std::int64_t>();
            record.option_id = reader.read<std::int64_t>();
            record.option_hash = reader.read_string();

            record.platform_type = reader.read_enum8<PlatformType>();
            record.account_type = reader.read_enum8<AccountType>();
            record.currency = reader.read_enum8<CurrencyType>();
            record.symbol = reader.read_string();
            record.signal_name = reader.read_string();
            record.user_data = reader.read_string();
            record.comment = reader.read_string();

            record.option_type = reader.read_enum8<OptionType>();
            record.order_type = reader.read_enum8<OrderType>();
            record.amount = reader.read<double>();
            record.refund = reader.read<double>();
            record.min_payout = reader.read<double>();
            record.payout = reader.read<double>();
            record.profit = reader.read<double>();
            record.open_balance = reader.read<double>();
            record.close_balance = reader.read<double>();

            record.trade_state = reader.read_enum8<TradeState>();
            record.live_state = reader.read_enum8<TradeState>();
            record.error_code = reader.read_enum8<TradeErrorCode>();
            record.error_desc = reader.read_string();

            record.open_price = reader.read<double>();
            record.close_price = reader.read<double>();
            record.delay = reader.read<std::int64_t>();
            record.ping = reader.read<std::int64_t>();
            record.place_date = reader.read<std::int64_t>();
            record.send_date = reader.read<std::int64_t>();
            record.open_date = reader.read<std::int64_t>();
            record.close_date = reader.read<std::int64_t>();
            record.duration = reader.read<std::uint32_t>();

            record.mm_type = reader.read_enum8<MmSystemType>();
            record.mm_step = reader.read<std::int32_t>();
            record.mm_group_id = reader.read<std::int64_t>();
            record.mm_group_hash = reader.read_string();
            record.mm_group_name = reader.read_string();
            record.mm_params_json = reader.read_string();
            record.decision_params_json = reader.read_string();
            record.metadata_json = reader.read_string();

            record.flags = reader.read<std::uint8_t>();

            record.spread.raw = reader.read<std::uint64_t>();
            record.spread.digits = reader.read<std::uint8_t>();

            reader.ensure_finished();
            return record;
        }

        bool operator==(const TradeRecord& other) const {
            return trade_id == other.trade_id &&
                   signal_id == other.signal_id &&
                   unique_id == other.unique_id &&
                   unique_hash == other.unique_hash &&
                   account_id == other.account_id &&
                   option_id == other.option_id &&
                   option_hash == other.option_hash &&
                   platform_type == other.platform_type &&
                   account_type == other.account_type &&
                   currency == other.currency &&
                   symbol == other.symbol &&
                   signal_name == other.signal_name &&
                   user_data == other.user_data &&
                   comment == other.comment &&
                   option_type == other.option_type &&
                   order_type == other.order_type &&
                   amount == other.amount &&
                   refund == other.refund &&
                   min_payout == other.min_payout &&
                   payout == other.payout &&
                   profit == other.profit &&
                   open_balance == other.open_balance &&
                   close_balance == other.close_balance &&
                   trade_state == other.trade_state &&
                   live_state == other.live_state &&
                   error_code == other.error_code &&
                   error_desc == other.error_desc &&
                   open_price == other.open_price &&
                   close_price == other.close_price &&
                   delay == other.delay &&
                   ping == other.ping &&
                   place_date == other.place_date &&
                   send_date == other.send_date &&
                   open_date == other.open_date &&
                   close_date == other.close_date &&
                   duration == other.duration &&
                   mm_type == other.mm_type &&
                   mm_step == other.mm_step &&
                   mm_group_id == other.mm_group_id &&
                   mm_group_hash == other.mm_group_hash &&
                   mm_group_name == other.mm_group_name &&
                   mm_params_json == other.mm_params_json &&
                   decision_params_json == other.decision_params_json &&
                   metadata_json == other.metadata_json &&
                   flags == other.flags &&
                   spread.raw == other.spread.raw &&
                   spread.digits == other.spread.digits;
        }

        bool operator!=(const TradeRecord& other) const {
            return !(*this == other);
        }

    private:
        static constexpr std::uint32_t kBinaryMagic = 0x5254584fU; // "OXTR" on little-endian hosts.
        static constexpr std::uint16_t kBinaryVersion = 1;

        template<typename T>
        static bool same_known(T lhs, T rhs, T unknown) noexcept {
            return lhs == unknown || rhs == unknown || lhs == rhs;
        }

        static bool same_known(const std::string& lhs, const std::string& rhs) noexcept {
            return lhs.empty() || rhs.empty() || lhs == rhs;
        }

        static bool result_has_balance_projection(const TradeResult& result) noexcept {
            return result.trade_state == TradeState::WIN ||
                   result.trade_state == TradeState::LOSS ||
                   result.trade_state == TradeState::STANDOFF ||
                   result.trade_state == TradeState::REFUND;
        }

        template<typename T>
        static void append_value(std::vector<std::uint8_t>& bytes, const T& value) {
            static_assert(std::is_trivially_copyable<T>::value, "TradeRecord supports only trivially copyable scalar values");
            const auto* ptr = reinterpret_cast<const std::uint8_t*>(&value);
            bytes.insert(bytes.end(), ptr, ptr + sizeof(T));
        }

        template<typename EnumType>
        static void append_enum(std::vector<std::uint8_t>& bytes, EnumType value) {
            append_value(bytes, static_cast<std::int32_t>(value));
        }

        template<typename EnumType>
        static void append_enum8(std::vector<std::uint8_t>& bytes, EnumType value) {
            using underlying = typename std::underlying_type<EnumType>::type;
            auto v = static_cast<underlying>(value);
            if (v > std::numeric_limits<std::uint8_t>::max()) {
                throw std::runtime_error("TradeRecord: enum value exceeds uint8_t range");
            }
            append_value(bytes, static_cast<std::uint8_t>(v));
        }

        static void append_string(std::vector<std::uint8_t>& bytes, const std::string& value) {
            if (value.size() > std::numeric_limits<std::uint32_t>::max()) {
                throw std::length_error("TradeRecord string field is too large to serialize");
            }
            const auto size = static_cast<std::uint32_t>(value.size());
            append_value(bytes, size);
            const auto* ptr = reinterpret_cast<const std::uint8_t*>(value.data());
            bytes.insert(bytes.end(), ptr, ptr + value.size());
        }

        class BinaryReader {
        public:
            BinaryReader(const void* data, std::size_t size)
                : m_data(static_cast<const std::uint8_t*>(data)), m_size(size) {
                if (!m_data && m_size != 0) {
                    throw std::runtime_error("TradeRecord::from_bytes: null data");
                }
            }

            template<typename T>
            T read() {
                static_assert(std::is_trivially_copyable<T>::value, "TradeRecord supports only trivially copyable scalar values");
                require(sizeof(T));
                T value;
                std::memcpy(&value, m_data + m_offset, sizeof(T));
                m_offset += sizeof(T);
                return value;
            }

            template<typename EnumType>
            EnumType read_enum() {
                return static_cast<EnumType>(read<std::int32_t>());
            }

            template<typename EnumType>
            EnumType read_enum8() {
                return static_cast<EnumType>(read<std::uint8_t>());
            }

            std::string read_string() {
                const auto len = read<std::uint32_t>();
                require(len);
                std::string value(reinterpret_cast<const char*>(m_data + m_offset), len);
                m_offset += len;
                return value;
            }

            void ensure_finished() const {
                if (m_offset != m_size) {
                    throw std::runtime_error("TradeRecord::from_bytes: trailing data");
                }
            }

        private:
            const std::uint8_t* m_data = nullptr;
            std::size_t m_size = 0;
            std::size_t m_offset = 0;

            void require(std::size_t bytes) const {
                if (bytes > m_size - m_offset) {
                    throw std::runtime_error("TradeRecord::from_bytes: corrupted or truncated data");
                }
            }
        };
    };

} // namespace optionx

#endif // _OPTIONX_TRADE_RECORD_HPP_INCLUDED
