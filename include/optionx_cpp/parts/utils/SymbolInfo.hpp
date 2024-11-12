#pragma once
#ifndef _OPTIONX_SYMBOL_INFO_HPP_INCLUDED
#define _OPTIONX_SYMBOL_INFO_HPP_INCLUDED

/// \file SymbolInfo.hpp
/// \brief Contains the SymbolInfo struct for storing basic information about a trading symbol.

#include <string>

namespace optionx {

    /// \struct SymbolInfo
    /// \brief Represents basic information for a trading symbol.
    struct SymbolInfo {
        std::string symbol; ///< The trading symbol name.
        int64_t digits = 0; ///< Number of decimal places for price values of the symbol.

        /// \brief Default constructor.
        SymbolInfo() = default;

        /// \brief Constructs a SymbolInfo with specified parameters.
        /// \param symbol The name of the trading symbol.
        /// \param digits Number of decimal places for the symbol's price values.
        SymbolInfo(const std::string& symbol, int64_t digits)
            : symbol(symbol), digits(digits) {}
    }; // SymbolInfo

}; // namespace optionx

#endif // _OPTIONX_SYMBOL_INFO_HPP_INCLUDED
