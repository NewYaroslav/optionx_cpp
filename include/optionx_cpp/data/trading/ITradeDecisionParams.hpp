#pragma once
#ifndef _OPTIONX_TRADING_I_TRADE_DECISION_PARAMS_HPP_INCLUDED
#define _OPTIONX_TRADING_I_TRADE_DECISION_PARAMS_HPP_INCLUDED

/// \file ITradeDecisionParams.hpp
/// \brief Defines the ITradeDecisionParams interface for trade decision strategies.

namespace optionx {

    /// \class ITradeDecisionParams
    /// \brief Interface for trade decision parameters used in TradeDecisionEngine.
    class ITradeDecisionParams {
    public:
        virtual ~ITradeDecisionParams() = default;

        /// \brief Gets the decision strategy type.
        /// \return The corresponding `MmSystemType` value.
        virtual MmSystemType get_type() const = 0;

        /// \brief Clones the decision parameters.
        /// \return A unique pointer to a cloned instance.
        virtual std::unique_ptr<ITradeDecisionParams> clone() const = 0;
		
		/// \brief Serializes the object to JSON.
        /// \return A JSON object representing the decision parameters.
        virtual nlohmann::json to_json() const = 0;

        /// \brief Creates a trade decision parameter instance from a JSON object.
        /// \param j The JSON object containing the parameters.
        /// \return A unique pointer to a new `ITradeDecisionParams` instance.
        static std::unique_ptr<ITradeDecisionParams> from_json(const nlohmann::json& j);
    };

} // namespace optionx

#endif // _OPTIONX_TRADING_I_TRADE_DECISION_PARAMS_HPP_INCLUDED