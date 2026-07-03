#pragma once
#ifndef _OPTIONX_PLATFORMS_INTRADEBAR_SYMBOL_UTILS_HPP_INCLUDED
#define _OPTIONX_PLATFORMS_INTRADEBAR_SYMBOL_UTILS_HPP_INCLUDED

/// \file SymbolUtils.hpp
/// \brief Broker-specific symbol normalization helpers for Intrade Bar.

#include <cctype>
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
        for (auto& ch : symbol) {
            ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
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

    /// \brief Converts a normalized FX symbol to the `/fxconnect` stream format.
    /// \param symbol Public or broker symbol name.
    /// \return Slash-separated stream symbol, such as `EUR/USD`; empty for non-FX symbols.
    inline std::string make_fxconnect_symbol(const std::string& symbol) {
        const auto normalized = normalize_symbol_name(symbol);
        if (normalized.empty() || is_btc_symbol(normalized) || normalized.size() != 6) {
            return {};
        }
        return normalized.substr(0, 3) + "/" + normalized.substr(3, 3);
    }

} // namespace optionx::platforms::intrade_bar

#endif // _OPTIONX_PLATFORMS_INTRADEBAR_SYMBOL_UTILS_HPP_INCLUDED
