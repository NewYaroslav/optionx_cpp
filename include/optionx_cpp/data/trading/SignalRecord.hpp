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
    };

} // namespace optionx

#endif // _OPTIONX_SIGNAL_RECORD_HPP_INCLUDED
