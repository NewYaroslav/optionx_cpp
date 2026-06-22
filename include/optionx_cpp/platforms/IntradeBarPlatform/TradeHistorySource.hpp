#pragma once
#ifndef _OPTIONX_PLATFORMS_INTRADEBAR_TRADE_HISTORY_SOURCE_HPP_INCLUDED
#define _OPTIONX_PLATFORMS_INTRADEBAR_TRADE_HISTORY_SOURCE_HPP_INCLUDED

/// \file TradeHistorySource.hpp
/// \brief Defines Intrade Bar closed trade history source modes.

#include <algorithm>
#include <cctype>
#include <string>

namespace optionx::platforms::intrade_bar {

    /// \brief Source used to load closed trade history.
    enum class TradeHistorySource {
        HTML,    ///< Parse the authenticated HTML page history.
        CSV,     ///< Use /stat_trade_export.php CSV export.
        HTML_CSV ///< Combine HTML broker IDs with CSV financial history.
    };

    /// \brief Converts trade history source to a stable config string.
    /// \param source History source mode.
    /// \return String representation used in JSON/env config.
    inline const char* trade_history_source_to_string(
            TradeHistorySource source) noexcept {
        switch (source) {
        case TradeHistorySource::HTML:
            return "HTML";
        case TradeHistorySource::CSV:
            return "CSV";
        case TradeHistorySource::HTML_CSV:
            return "HTML_CSV";
        }
        return "CSV";
    }

    /// \brief Parses trade history source from config text.
    /// \param value String value such as HTML, CSV, HTML_CSV, or HTML+CSV.
    /// \param fallback Value returned when the input is unknown.
    /// \return Parsed history source mode.
    inline TradeHistorySource trade_history_source_from_string(
            std::string value,
            TradeHistorySource fallback = TradeHistorySource::CSV) {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
            return static_cast<char>(std::toupper(ch));
        });
        value.erase(std::remove_if(value.begin(), value.end(), [](unsigned char ch) {
            return std::isspace(ch) != 0;
        }), value.end());
        if (value == "HTML") return TradeHistorySource::HTML;
        if (value == "CSV") return TradeHistorySource::CSV;
        if (value == "HTML_CSV" || value == "HTML+CSV" || value == "HTMLCSV") {
            return TradeHistorySource::HTML_CSV;
        }
        return fallback;
    }

} // namespace optionx::platforms::intrade_bar

#endif // _OPTIONX_PLATFORMS_INTRADEBAR_TRADE_HISTORY_SOURCE_HPP_INCLUDED
