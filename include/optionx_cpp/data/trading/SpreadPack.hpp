#pragma once
#ifndef _OPTIONX_SPREAD_PACK_HPP_INCLUDED
#define _OPTIONX_SPREAD_PACK_HPP_INCLUDED

/// \file SpreadPack.hpp
/// \brief Packed bid/ask spread representation for trade records.

#include <cstdint>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "optionx_cpp/data/trading/enums.hpp"

namespace optionx {

    /// \struct SpreadPack
    /// \brief Packs open and close spread values into a single 64-bit word with shared decimal precision.
    struct SpreadPack {
        std::uint64_t raw = 0;     ///< Lower 32 bits = open spread fixed-point, upper 32 bits = close spread fixed-point.
        std::uint8_t  digits = 0;  ///< Number of decimal places for both spreads.

        static constexpr std::uint8_t max_digits = 18;

        void set_open_spread(double value, std::uint8_t d) {
            if (d > max_digits) throw std::invalid_argument("SpreadPack digits exceeds 18");
            digits = d;
            const double scale = std::pow(10.0, digits);
            const auto fixed = static_cast<std::int32_t>(std::round(value * scale));
            raw = (raw & ~0xFFFFFFFFu) | (static_cast<std::uint32_t>(fixed) & 0xFFFFFFFFu);
        }

        void set_close_spread(double value, std::uint8_t d) {
            if (d > max_digits) throw std::invalid_argument("SpreadPack digits exceeds 18");
            digits = d;
            const double scale = std::pow(10.0, digits);
            const auto fixed = static_cast<std::int32_t>(std::round(value * scale));
            raw = (raw & 0xFFFFFFFFu) | (static_cast<std::uint64_t>(static_cast<std::uint32_t>(fixed)) << 32);
        }

        double open_spread() const {
            if (digits == 0) return 0.0;
            const double scale = std::pow(10.0, digits);
            const auto fixed = static_cast<std::int32_t>(static_cast<std::uint32_t>(raw));
            return static_cast<double>(fixed) / scale;
        }

        double close_spread() const {
            if (digits == 0) return 0.0;
            const double scale = std::pow(10.0, digits);
            const auto fixed = static_cast<std::int32_t>(static_cast<std::uint32_t>(raw >> 32));
            return static_cast<double>(fixed) / scale;
        }

        bool is_open_spread_positive() const noexcept {
            return static_cast<std::int32_t>(static_cast<std::uint32_t>(raw)) >= 0;
        }

        bool is_close_spread_positive() const noexcept {
            return static_cast<std::int32_t>(static_cast<std::uint32_t>(raw >> 32)) >= 0;
        }

        double spread_difference() const {
            return close_spread() - open_spread();
        }

        double open_mid_price(double price, OrderType order_type) const {
            const double spread = open_spread();
            const double half = spread * 0.5;
            if (order_type == OrderType::BUY) {
                return price + half;
            }
            return price - half;
        }

        double close_mid_price(double price, OrderType order_type) const {
            const double spread = close_spread();
            const double half = spread * 0.5;
            if (order_type == OrderType::BUY) {
                return price + half;
            }
            return price - half;
        }
    };

} // namespace optionx

#endif // _OPTIONX_SPREAD_PACK_HPP_INCLUDED
