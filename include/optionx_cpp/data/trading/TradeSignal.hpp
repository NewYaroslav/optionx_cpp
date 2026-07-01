#pragma once
#ifndef _OPTIONX_TRADE_SIGNAL_HPP_INCLUDED
#define _OPTIONX_TRADE_SIGNAL_HPP_INCLUDED

/// \file TradeSignal.hpp
/// \brief Defines the TradeSignal class with execution and money-management data.

namespace optionx {

    /// \class TradeSignal
    /// \brief Represents a trade signal with execution, decision, and money-management parameters.
    class TradeSignal {
    public:
        std::uint64_t signal_id = 0;                         ///< Persistent signal ID; 0 means "not assigned".
        TradeRequest request;                                ///< Trade request parameters.
        MmSystemType mm_type = MmSystemType::NONE;           ///< Money management system type.
        std::unique_ptr<IMoneyManagementParams> mm_params;   ///< Money management parameters.
        std::unique_ptr<ITradeDecisionParams> decision_params; ///< Decision-making parameters.

        /// \brief Returns the effective signal ID shared with generated requests.
        std::uint64_t resolved_signal_id() const noexcept {
            return signal_id != 0 ? signal_id : request.signal_id;
        }

        /// \brief Assigns the signal ID to both the signal and its root request.
        void set_signal_id(std::uint64_t value) noexcept {
            signal_id = value;
            request.signal_id = value;
        }

        /// \brief Sets the money management parameters for the trade signal.
        ///
        /// If `params` is valid, it is stored and the `mm_type` is updated accordingly.
        /// If `params` is null, the money management type is reset to NONE.
        ///
        /// \param params A unique pointer to money management parameters.
        void set_money_management(std::unique_ptr<IMoneyManagementParams> params) {
            if (params) {
                mm_type = params->get_type();
                mm_params = std::move(params);
            } else {
                mm_type = MmSystemType::NONE;
                mm_params.reset();
            }
        }

        /// \brief Creates a deep copy of the trade signal.
        ///
        /// This method clones the `TradeSignal`, including a copy of `TradeRequest`
        /// and cloned parameter objects when they exist.
        ///
        /// \return A unique pointer to the cloned `TradeSignal`.
        std::unique_ptr<TradeSignal> clone() const {
            auto cloned = std::make_unique<TradeSignal>();
            const auto effective_signal_id = resolved_signal_id();
            cloned->signal_id = effective_signal_id;
            cloned->request = request;
            if (cloned->request.signal_id == 0) {
                cloned->request.signal_id = effective_signal_id;
            }
            cloned->mm_type = mm_type;
            if (mm_params) {
                cloned->mm_params = mm_params->clone();
            }
            if (decision_params) {
                cloned->decision_params = decision_params->clone();
            }
            return cloned;
        }
    };

} // namespace optionx

namespace nlohmann {

    /// \brief Custom JSON serializer for TradeSignal.
    template <>
    struct adl_serializer<optionx::TradeSignal> {
        /// \brief Serializes a TradeSignal object to JSON.
        /// \param j The JSON object to populate.
        /// \param signal The TradeSignal object to convert.
        static void to_json(json& j, const optionx::TradeSignal& signal) {
            j = json{
                {"signal_id", signal.resolved_signal_id()},
                {"request", signal.request},
                {"mm_type", signal.mm_type},
                {"mm_params", signal.mm_params ? signal.mm_params->to_json() : nullptr},
                {"decision_params", signal.decision_params ? signal.decision_params->to_json() : nullptr}
            };
        }

        /// \brief Deserializes a TradeSignal object from JSON.
        /// \param j The JSON object to read.
        /// \param signal The TradeSignal object to populate.
        static void from_json(const json& j, optionx::TradeSignal& signal) {
            signal.signal_id = j.value("signal_id", std::uint64_t{0});
            signal.request = j.at("request").get<optionx::TradeRequest>();
            if (signal.signal_id == 0) {
                signal.signal_id = signal.request.signal_id;
            } else if (signal.request.signal_id == 0) {
                signal.request.signal_id = signal.signal_id;
            }
            signal.mm_type = j.at("mm_type").get<optionx::MmSystemType>();

            if (!j["mm_params"].is_null()) {
                // Typed money-management params are restored by higher-level strategy code.
            }
            if (j.contains("decision_params") && !j["decision_params"].is_null()) {
                // Typed decision params are restored by higher-level strategy code.
            }
        }
    };

} // namespace nlohmann

#endif // _OPTIONX_TRADE_SIGNAL_HPP_INCLUDED
