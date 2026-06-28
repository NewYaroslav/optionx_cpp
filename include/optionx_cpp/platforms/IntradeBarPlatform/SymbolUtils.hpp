#pragma once
#ifndef _OPTIONX_PLATFORMS_INTRADERBAR_SYMBOL_UTILS_HPP_INCLUDED
#define _OPTIONX_PLATFORMS_INTRADERBAR_SYMBOL_UTILS_HPP_INCLUDED

/// \file SymbolUtils.hpp
/// \brief Broker-specific symbol normalization helpers for Intrade Bar.

#include <string>

namespace optionx::platforms::intrade_bar {

    /// \brief Converts public symbol aliases to broker-side Intrade Bar names.
    /// \param symbol Public or broker symbol name.
    /// \return Normalized broker symbol name.
    inline std::string normalize_symbol_name(std::string symbol) {
        for (;;) {
            auto it_str = symbol.find('/');
            if (it_str != std::string::npos) symbol.erase(it_str, 1);
            else break;
        }
        if (symbol == "BTCUSD") return "BTCUSDT";
        return symbol;
    }

    /// \brief Checks whether a public or broker symbol refers to the BTCUSDT instrument.
    /// \param symbol Public or broker symbol name.
    /// \return True for BTCUSD/BTCUSDT aliases.
    inline bool is_btc_symbol(const std::string& symbol) {
        return normalize_symbol_name(symbol) == "BTCUSDT";
    }

} // namespace optionx::platforms::intrade_bar

#endif // _OPTIONX_PLATFORMS_INTRADERBAR_SYMBOL_UTILS_HPP_INCLUDED
