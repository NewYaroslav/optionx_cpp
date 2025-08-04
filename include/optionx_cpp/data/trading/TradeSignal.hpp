#pragma once
#ifndef _OPTIONX_TRADE_SIGNAL_HPP_INCLUDED
#define _OPTIONX_TRADE_SIGNAL_HPP_INCLUDED

/// \file TradeSignal.hpp
/// \brief Defines the TradeSignal class, which represents a trade signal with execution and money management parameters.

namespace optionx {

    /// \class TradeSignal
    /// \brief Represents a trade signal with execution and money management parameters.
	/// \brief Represents a trade signal containing trade execution parameters and decision-making settings.
    class TradeSignal {
    public:
        TradeRequest request;  								///< Trade request parameters.
        MmSystemType mm_type = MmSystemType::NONE;  		///< Money management system type.
        std::unique_ptr<IMoneyManagementParams> mm_params;  ///< Money management parameters.
		std::unique_ptr<ITradeDecisionParams> decision_params;  ///< Decision-making parameters.

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
        /// and a new instance of `mm_params` if it exists.
        /// 
        /// \return A unique pointer to the cloned `TradeSignal`.
        std::unique_ptr<TradeSignal> clone() const {
            auto cloned = std::make_unique<TradeSignal>();
            cloned->request = request;
            cloned->mm_type = mm_type;
            if (mm_params) {
                cloned->mm_params = mm_params->clone();
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
                {"request", signal.request},
                {"mm_type", signal.mm_type},  // Теперь сериализуется через adl_serializer
                {"mm_params", signal.mm_params ? signal.mm_params->to_json() : nullptr}
            };
        }

        /// \brief Deserializes a TradeSignal object from JSON.
        /// \param j The JSON object to read.
        /// \param signal The TradeSignal object to populate.
        static void from_json(const json& j, optionx::TradeSignal& signal) {
            signal.request = j.at("request").get<optionx::TradeRequest>();
            signal.mm_type = j.at("mm_type").get<optionx::MmSystemType>(); // Используем adl_serializer

            if (!j["mm_params"].is_null()) {
                //auto mm = std::make_unique<optionx::MoneyManagementParams>();
                //*mm = j.at("mm_params").get<optionx::MoneyManagementParams>();
                //signal.mm_params = std::move(mm);
            }
        }
    };

} // namespace nlohmann

#endif // _OPTIONX_TRADE_SIGNAL_HPP_INCLUDED