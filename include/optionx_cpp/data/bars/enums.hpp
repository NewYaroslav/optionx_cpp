#pragma once
#ifndef _OPTIONX_BAR_ENUMS_HPP_INCLUDED
#define _OPTIONX_BAR_ENUMS_HPP_INCLUDED

/// \file enums.hpp
/// \brief Defines bar data enums.

#include <cstdint>
#include <algorithm>
#include <cctype>
#include <string>

namespace optionx {

    /// \enum BarPriceSource
    /// \brief Defines which price stream was used to build a single OHLC bar.
    enum class BarPriceSource : std::uint8_t {
        UNKNOWN = 0, ///< Price source is not specified.
        BID,         ///< Bar was built from bid prices.
        ASK,         ///< Bar was built from ask prices.
        MID,         ///< Bar was built from the bid/ask midpoint.
        LAST         ///< Bar was built from last trade prices.
    };

    /// \brief Converts BarPriceSource to its string representation.
    inline const char* to_str(BarPriceSource value) noexcept {
        switch (value) {
        case BarPriceSource::BID:
            return "BID";
        case BarPriceSource::ASK:
            return "ASK";
        case BarPriceSource::MID:
            return "MID";
        case BarPriceSource::LAST:
            return "LAST";
        case BarPriceSource::UNKNOWN:
        default:
            return "UNKNOWN";
        }
    }

    /// \brief Parses a bar price source token.
    /// \param value Input token such as BID, ASK, MID, AVG, or LAST.
    /// \param fallback Value returned for empty or unknown input.
    /// \return Parsed price source, or fallback.
    inline BarPriceSource bar_price_source_from_string(
            std::string value,
            BarPriceSource fallback = BarPriceSource::UNKNOWN) {
        value.erase(std::remove_if(
            value.begin(),
            value.end(),
            [](unsigned char ch) {
                return std::isspace(ch) != 0;
            }), value.end());
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
            return static_cast<char>(std::toupper(ch));
        });

        if (value.empty()) return fallback;
        if (value == "BID") return BarPriceSource::BID;
        if (value == "ASK") return BarPriceSource::ASK;
        if (value == "MID" || value == "AVG") return BarPriceSource::MID;
        if (value == "LAST") return BarPriceSource::LAST;
        return fallback;
    }

} // namespace optionx

#endif // _OPTIONX_BAR_ENUMS_HPP_INCLUDED
