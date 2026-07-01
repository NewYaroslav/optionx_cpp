#pragma once
#ifndef _OPTIONX_SIGNAL_RECORD_HPP_INCLUDED
#define _OPTIONX_SIGNAL_RECORD_HPP_INCLUDED

/// \file SignalRecord.hpp
/// \brief Defines the persistent DTO used to store trading signals.

namespace optionx {

    /// \class SignalRecord
    /// \brief Flat persistent representation of a signal and the trades it produced.
    class SignalRecord {
    public:
        // Storage identity
        std::uint32_t signal_id = 0;          ///< Persistent signal ID; 0 means "not assigned".
        std::int64_t unique_id = 0;           ///< External/runtime signal identifier.
        std::string unique_hash;              ///< External/runtime signal hash.
        std::int64_t account_id = 0;          ///< Target account ID, if known.

        // Signal context
        PlatformType platform_type = PlatformType::UNKNOWN; ///< Trading platform.
        AccountType account_type = AccountType::UNKNOWN;    ///< Account type.
        CurrencyType currency = CurrencyType::UNKNOWN;      ///< Account currency.
        std::string symbol;                                 ///< Trading symbol.
        std::string signal_name;                            ///< Signal or strategy name.
        std::string user_data;                              ///< User-defined request data.
        std::string comment;                                ///< Signal comment.

        // Requested trade parameters
        OptionType option_type = OptionType::UNKNOWN;       ///< Requested option type.
        OrderType order_type = OrderType::UNKNOWN;          ///< Requested direction.
        double amount = 0.0;                                ///< Initial requested amount.
        double refund = 0.0;                                ///< Requested refund ratio.
        double min_payout = 0.0;                            ///< Minimum acceptable payout.
        std::uint32_t duration = 0;                         ///< Requested duration in seconds; 0 means not specified.
        std::int64_t expiry_time = 0;                       ///< Requested classic expiry time in seconds.

        // Signal lifecycle and outcome
        SignalStatus status = SignalStatus::UNKNOWN;        ///< Signal processing status.
        SignalRejectCode reject_code = SignalRejectCode::NONE; ///< Rejection reason, if rejected.
        std::string reject_desc;                            ///< Human-readable rejection details.
        SignalOutcome outcome = SignalOutcome::UNKNOWN;     ///< Aggregated signal outcome.
        TradeState trade_state = TradeState::UNKNOWN;       ///< Trade-like terminal state for summary use.
        double total_amount = 0.0;                          ///< Sum of generated trade amounts.
        double total_profit = 0.0;                          ///< Aggregated realized/current profit.

        // Timestamps are milliseconds.
        std::int64_t create_date = 0;                       ///< Signal creation timestamp.
        std::int64_t accept_date = 0;                       ///< Accepted timestamp.
        std::int64_t reject_date = 0;                       ///< Rejected timestamp.
        std::int64_t complete_date = 0;                     ///< Completed timestamp.

        // Money management and extensibility
        MmSystemType mm_type = MmSystemType::NONE;          ///< Money management strategy.
        std::int32_t mm_step = 0;                           ///< Generic money management step.
        std::int64_t mm_group_id = 0;                       ///< Numeric money management group.
        std::string mm_group_hash;                          ///< Hash of the money management group.
        std::string mm_group_name;                          ///< Human-readable group name.
        std::string mm_params_json;                         ///< Serialized money management params.
        std::string decision_params_json;                   ///< Serialized decision params.
        std::string metadata_json;                          ///< Future extension data.

        // Produced trades
        std::vector<std::uint32_t> trade_ids;               ///< Persistent TradeRecord IDs produced by this signal.

        /// \brief Returns true when signal_id contains a persistent identity.
        bool has_signal_id() const noexcept {
            return signal_id != 0;
        }

        /// \brief Returns true when this signal already references the trade ID.
        bool has_trade_id(std::uint32_t trade_id) const {
            return std::find(trade_ids.begin(), trade_ids.end(), trade_id) != trade_ids.end();
        }

        /// \brief Adds a produced trade ID, ignoring zero and duplicates.
        void add_trade_id(std::uint32_t trade_id) {
            if (trade_id == 0 || has_trade_id(trade_id)) return;
            trade_ids.push_back(trade_id);
        }

        /// \brief Copies request-side fields into this signal record.
        void assign_request(const TradeRequest& request) {
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
            expiry_time = request.expiry_time;
        }

        /// \brief Copies request and money-management fields from a signal.
        void assign_signal(const TradeSignal& signal) {
            signal_id = signal.signal_id;
            unique_id = signal.unique_id;
            unique_hash = signal.unique_hash;
            account_id = signal.account_id;
            platform_type = signal.platform_type;
            account_type = signal.account_type;
            currency = signal.currency;
            symbol = signal.symbol;
            signal_name = signal.signal_name;
            user_data = signal.user_data;
            comment = signal.comment;
            option_type = signal.option_type;
            order_type = signal.order_type;
            amount = signal.amount;
            refund = signal.refund;
            min_payout = signal.min_payout;
            duration = signal.duration;
            expiry_time = signal.expiry_time;
            mm_type = signal.mm_type;
            mm_step = signal.mm_step;
            mm_group_id = signal.mm_group_id;
            mm_group_hash = signal.mm_group_hash;
            mm_group_name = signal.mm_group_name;
            mm_params_json = signal.mm_params ? signal.mm_params->to_json().dump() : std::string();
            decision_params_json = signal.decision_params ? signal.decision_params->to_json().dump() : std::string();
        }

        /// \brief Builds a record from a trade request.
        static SignalRecord from_signal(const TradeRequest& request) {
            SignalRecord record;
            record.assign_request(request);
            return record;
        }

        /// \brief Builds a record from a trade signal.
        static SignalRecord from_signal(const TradeSignal& signal) {
            SignalRecord record;
            record.assign_signal(signal);
            return record;
        }

        /// \brief Serializes the record using the current binary storage format.
        std::vector<std::uint8_t> to_bytes() const {
            std::vector<std::uint8_t> bytes;
            bytes.reserve(384 + unique_hash.size() + symbol.size() +
                          signal_name.size() + user_data.size() +
                          comment.size() + reject_desc.size() +
                          mm_group_hash.size() + mm_group_name.size() +
                          mm_params_json.size() + decision_params_json.size() +
                          metadata_json.size() +
                          trade_ids.size() * sizeof(std::uint32_t));

            append_value(bytes, kBinaryMagic);
            append_value(bytes, kBinaryVersion);

            append_value(bytes, signal_id);
            append_value(bytes, unique_id);
            append_string(bytes, unique_hash);
            append_value(bytes, account_id);

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
            append_value(bytes, duration);
            append_value(bytes, expiry_time);

            append_enum8(bytes, status);
            append_enum8(bytes, reject_code);
            append_string(bytes, reject_desc);
            append_enum8(bytes, outcome);
            append_enum8(bytes, trade_state);
            append_value(bytes, total_amount);
            append_value(bytes, total_profit);

            append_value(bytes, create_date);
            append_value(bytes, accept_date);
            append_value(bytes, reject_date);
            append_value(bytes, complete_date);

            append_enum8(bytes, mm_type);
            append_value(bytes, mm_step);
            append_value(bytes, mm_group_id);
            append_string(bytes, mm_group_hash);
            append_string(bytes, mm_group_name);
            append_string(bytes, mm_params_json);
            append_string(bytes, decision_params_json);
            append_string(bytes, metadata_json);

            append_vector_u32(bytes, trade_ids);
            return bytes;
        }

        /// \brief Deserializes a signal record serialized by the current binary storage format.
        static SignalRecord from_bytes(const void* data, std::size_t size) {
            BinaryReader reader(data, size);

            const auto magic = reader.read<std::uint32_t>();
            if (magic != kBinaryMagic) {
                throw std::runtime_error("SignalRecord::from_bytes: invalid magic");
            }

            const auto version = reader.read<std::uint16_t>();
            if (version != kBinaryVersion) {
                throw std::runtime_error("SignalRecord::from_bytes: unsupported version");
            }

            SignalRecord record;
            record.signal_id = reader.read<std::uint32_t>();
            record.unique_id = reader.read<std::int64_t>();
            record.unique_hash = reader.read_string();
            record.account_id = reader.read<std::int64_t>();

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
            record.duration = reader.read<std::uint32_t>();
            record.expiry_time = reader.read<std::int64_t>();

            record.status = reader.read_enum8<SignalStatus>();
            record.reject_code = reader.read_enum8<SignalRejectCode>();
            record.reject_desc = reader.read_string();
            record.outcome = reader.read_enum8<SignalOutcome>();
            record.trade_state = reader.read_enum8<TradeState>();
            record.total_amount = reader.read<double>();
            record.total_profit = reader.read<double>();

            record.create_date = reader.read<std::int64_t>();
            record.accept_date = reader.read<std::int64_t>();
            record.reject_date = reader.read<std::int64_t>();
            record.complete_date = reader.read<std::int64_t>();

            record.mm_type = reader.read_enum8<MmSystemType>();
            record.mm_step = reader.read<std::int32_t>();
            record.mm_group_id = reader.read<std::int64_t>();
            record.mm_group_hash = reader.read_string();
            record.mm_group_name = reader.read_string();
            record.mm_params_json = reader.read_string();
            record.decision_params_json = reader.read_string();
            record.metadata_json = reader.read_string();

            record.trade_ids = reader.read_vector_u32();
            reader.ensure_finished();
            return record;
        }

        bool operator==(const SignalRecord& other) const {
            return signal_id == other.signal_id &&
                   unique_id == other.unique_id &&
                   unique_hash == other.unique_hash &&
                   account_id == other.account_id &&
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
                   duration == other.duration &&
                   expiry_time == other.expiry_time &&
                   status == other.status &&
                   reject_code == other.reject_code &&
                   reject_desc == other.reject_desc &&
                   outcome == other.outcome &&
                   trade_state == other.trade_state &&
                   total_amount == other.total_amount &&
                   total_profit == other.total_profit &&
                   create_date == other.create_date &&
                   accept_date == other.accept_date &&
                   reject_date == other.reject_date &&
                   complete_date == other.complete_date &&
                   mm_type == other.mm_type &&
                   mm_step == other.mm_step &&
                   mm_group_id == other.mm_group_id &&
                   mm_group_hash == other.mm_group_hash &&
                   mm_group_name == other.mm_group_name &&
                   mm_params_json == other.mm_params_json &&
                   decision_params_json == other.decision_params_json &&
                   metadata_json == other.metadata_json &&
                   trade_ids == other.trade_ids;
        }

        bool operator!=(const SignalRecord& other) const {
            return !(*this == other);
        }

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(
            SignalRecord,
            signal_id,
            unique_id,
            unique_hash,
            account_id,
            platform_type,
            account_type,
            currency,
            symbol,
            signal_name,
            user_data,
            comment,
            option_type,
            order_type,
            amount,
            refund,
            min_payout,
            duration,
            expiry_time,
            status,
            reject_code,
            reject_desc,
            outcome,
            trade_state,
            total_amount,
            total_profit,
            create_date,
            accept_date,
            reject_date,
            complete_date,
            mm_type,
            mm_step,
            mm_group_id,
            mm_group_hash,
            mm_group_name,
            mm_params_json,
            decision_params_json,
            metadata_json,
            trade_ids
        )

    private:
        static constexpr std::uint32_t kBinaryMagic = 0x5253584fU; // "OXSR" on little-endian hosts.
        static constexpr std::uint16_t kBinaryVersion = 1;
        static_assert(sizeof(double) == 8, "SignalRecord binary format requires 64-bit double");
        static_assert(std::numeric_limits<double>::is_iec559,
                      "SignalRecord binary format requires IEEE-754 double");

        // This is a compact local storage format, not a cross-architecture wire format.
        // It is intended for little-endian Windows/Linux targets with IEEE-754 doubles.

        template<class T>
        static void append_value(std::vector<std::uint8_t>& bytes, const T& value) {
            static_assert(std::is_trivially_copyable_v<T>, "SignalRecord binary value must be trivially copyable");
            const auto* ptr = reinterpret_cast<const std::uint8_t*>(&value);
            bytes.insert(bytes.end(), ptr, ptr + sizeof(T));
        }

        template<class Enum>
        static void append_enum8(std::vector<std::uint8_t>& bytes, Enum value) {
            append_value(bytes, static_cast<std::uint8_t>(value));
        }

        static void append_string(std::vector<std::uint8_t>& bytes, const std::string& value) {
            if (value.size() > (std::numeric_limits<std::uint32_t>::max)()) {
                throw std::length_error("SignalRecord string field is too large to serialize");
            }
            append_value(bytes, static_cast<std::uint32_t>(value.size()));
            bytes.insert(bytes.end(), value.begin(), value.end());
        }

        static void append_vector_u32(
                std::vector<std::uint8_t>& bytes,
                const std::vector<std::uint32_t>& values) {
            if (values.size() > (std::numeric_limits<std::uint32_t>::max)()) {
                throw std::length_error("SignalRecord vector field is too large to serialize");
            }
            append_value(bytes, static_cast<std::uint32_t>(values.size()));
            for (const auto value : values) {
                append_value(bytes, value);
            }
        }

        class BinaryReader {
        public:
            BinaryReader(const void* data, std::size_t size)
                : m_data(static_cast<const std::uint8_t*>(data)), m_size(size) {
                if (!m_data && size > 0) {
                    throw std::runtime_error("SignalRecord::from_bytes: null data");
                }
            }

            template<class T>
            T read() {
                static_assert(std::is_trivially_copyable_v<T>, "SignalRecord binary value must be trivially copyable");
                ensure(sizeof(T));
                T value{};
                std::memcpy(&value, m_data + m_offset, sizeof(T));
                m_offset += sizeof(T);
                return value;
            }

            template<class Enum>
            Enum read_enum8() {
                return static_cast<Enum>(read<std::uint8_t>());
            }

            std::string read_string() {
                const auto length = read<std::uint32_t>();
                ensure(length);
                std::string value(
                    reinterpret_cast<const char*>(m_data + m_offset),
                    reinterpret_cast<const char*>(m_data + m_offset + length));
                m_offset += length;
                return value;
            }

            std::vector<std::uint32_t> read_vector_u32() {
                const auto length = read<std::uint32_t>();
                if (length > remaining() / sizeof(std::uint32_t)) {
                    throw std::runtime_error("SignalRecord::from_bytes: corrupted vector length");
                }

                std::vector<std::uint32_t> values;
                values.reserve(length);
                for (std::uint32_t i = 0; i < length; ++i) {
                    values.push_back(read<std::uint32_t>());
                }
                return values;
            }

            void ensure_finished() const {
                if (m_offset != m_size) {
                    throw std::runtime_error("SignalRecord::from_bytes: trailing data");
                }
            }

            std::size_t remaining() const noexcept {
                return m_size - m_offset;
            }

        private:
            const std::uint8_t* m_data = nullptr;
            std::size_t m_size = 0;
            std::size_t m_offset = 0;

            void ensure(std::size_t bytes) const {
                if (bytes > m_size - m_offset) {
                    throw std::runtime_error("SignalRecord::from_bytes: corrupted or truncated data");
                }
            }
        };
    };

} // namespace optionx

#endif // _OPTIONX_SIGNAL_RECORD_HPP_INCLUDED
