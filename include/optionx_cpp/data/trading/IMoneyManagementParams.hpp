#pragma once
#ifndef _OPTIONX_TRADING_I_MONEY_MANAGEMENT_PARAMS_HPP_INCLUDED
#define _OPTIONX_TRADING_I_MONEY_MANAGEMENT_PARAMS_HPP_INCLUDED

/// \file IMoneyManagementParams.hpp
/// \brief Defines the IMoneyManagementParams interface for money management strategies.

namespace optionx {

    /// \class IMoneyManagementParams
    /// \brief Interface for money management parameters.
    class IMoneyManagementParams {
    public:
        virtual ~IMoneyManagementParams() = default;

        /// \brief Gets the money management type.
        /// \return The corresponding `MmSystemType` value.
        virtual MmSystemType get_type() const = 0;

        /// \brief Clones the money management parameters.
        /// \return A unique pointer to a cloned instance.
        virtual std::unique_ptr<IMoneyManagementParams> clone() const = 0;

        /// \brief Creates a money management instance from a JSON object.
        /// \param j The JSON object containing the parameters.
        /// \return A unique pointer to a new `IMoneyManagementParams` instance.
        static std::unique_ptr<IMoneyManagementParams> from_json(const nlohmann::json& j);
		
		/// \brief Serializes the object to JSON.
        /// \return A JSON object representing the money management parameters.
        virtual nlohmann::json to_json() const = 0;
    };

} // namespace optionx

#endif // _OPTIONX_TRADING_I_MONEY_MANAGEMENT_PARAMS_HPP_INCLUDED