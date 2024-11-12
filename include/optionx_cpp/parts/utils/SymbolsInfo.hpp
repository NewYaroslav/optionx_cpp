#pragma once
#ifndef _OPTIONX_SYMBOLS_INFO_HPP_INCLUDED
#define _OPTIONX_SYMBOLS_INFO_HPP_INCLUDED

/// \file SymbolsInfo.hpp
/// \brief Contains the SymbolsInfo struct for managing a collection of trading symbols information.

#include "SymbolInfo.hpp"
#include <vector>
#include <string>
#include <optional>

namespace optionx {

    /// \struct SymbolsInfo
    /// \brief Manages a collection of SymbolInfo objects, representing information for multiple trading symbols.
    struct SymbolsInfo {
        std::vector<SymbolInfo> symbols; ///< Collection of SymbolInfo objects.

        /// \brief Adds a new symbol to the collection.
        /// \param symbol The name of the trading symbol.
        /// \param digits Number of decimal places for the symbol's price values.
        void add_symbol(const std::string& symbol, int64_t digits) {
            symbols.emplace_back(symbol, digits);
        }

        /// \brief Finds information about a symbol by name.
        /// \param symbol The name of the symbol to find.
        /// \return Optional SymbolInfo if the symbol is found; std::nullopt otherwise.
        std::optional<SymbolInfo> find_symbol(const std::string& symbol) const {
            for (const auto& sym_info : symbols) {
                if (sym_info.symbol == symbol) {
                    return sym_info;
                }
            }
            return std::nullopt;
        }

        /// \brief Clears all symbols from the collection.
        void clear() {
            symbols.clear();
        }
    }; // SymbolsInfo

}; // namespace optionx

#endif // _OPTIONX_SYMBOLS_INFO_HPP_INCLUDED
