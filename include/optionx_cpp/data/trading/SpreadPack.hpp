#pragma once
#ifndef OPTIONX_HEADER_DATA_TRADING_SPREAD_PACK_HPP_INCLUDED
#define OPTIONX_HEADER_DATA_TRADING_SPREAD_PACK_HPP_INCLUDED

/// \file SpreadPack.hpp
/// \brief Packed bid/ask spread representation for trade records.

#include <cstdint>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "enums.hpp"

namespace optionx {

    /// \struct SpreadPack
    /// \brief Packs open and close spread values into a single 64-bit word with shared decimal precision.
    ///
    /// Both packed values use the same `digits` precision. Prefer set_spreads()
    /// when open and close spread are known together; individual setters are
    /// intended for updates that keep the same precision.
    struct SpreadPack {
        std::uint64_t raw = 0;     ///< Lower 32 bits = open spread fixed-point, upper 32 bits = close spread fixed-point.
        std::uint8_t  digits = 0;  ///< Number of decimal places for both spreads.

        static constexpr std::uint8_t max_digits = 18;

        /// \brief Sets both spreads using one shared decimal precision.
        /// \param open_value Open spread.
        /// \param close_value Close spread.
        /// \param d Number of decimal places for both values.
        void set_spreads(double open_value, double close_value, std::uint8_t d) {
            const auto open_fixed = pack_fixed(open_value, d);
            const auto close_fixed = pack_fixed(close_value, d);
            digits = d;
            raw = (static_cast<std::uint64_t>(close_fixed) << 32) |
                  (static_cast<std::uint64_t>(open_fixed) & kLowerMask);
        }

        /// \brief Sets open spread and updates shared decimal precision.
        void set_open_spread(double value, std::uint8_t d) {
            const auto fixed = pack_fixed(value, d);
            digits = d;
            raw = (raw & kUpperMask) | (static_cast<std::uint64_t>(fixed) & kLowerMask);
        }

        /// \brief Sets close spread and updates shared decimal precision.
        void set_close_spread(double value, std::uint8_t d) {
            const auto fixed = pack_fixed(value, d);
            digits = d;
            raw = (raw & kLowerMask) | (static_cast<std::uint64_t>(fixed) << 32);
        }

        double open_spread() const {
            const double scale = scale_for_digits(digits);
            const auto fixed = static_cast<std::int32_t>(static_cast<std::uint32_t>(raw));
            return static_cast<double>(fixed) / scale;
        }

        double close_spread() const {
            const double scale = scale_for_digits(digits);
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

    private:
        static constexpr std::uint64_t kLowerMask = 0x00000000FFFFFFFFull;
        static constexpr std::uint64_t kUpperMask = 0xFFFFFFFF00000000ull;

        static double scale_for_digits(std::uint8_t d) {
            return d == 0 ? 1.0 : std::pow(10.0, d);
        }

        static std::uint32_t pack_fixed(double value, std::uint8_t d) {
            if (d > max_digits) {
                throw std::invalid_argument("SpreadPack digits exceeds 18");
            }
            if (!std::isfinite(value)) {
                throw std::invalid_argument("SpreadPack spread value must be finite");
            }

            const double scaled = value * scale_for_digits(d);
            if (!std::isfinite(scaled)) {
                throw std::out_of_range("SpreadPack fixed-point value is out of range");
            }

            const double rounded = std::round(scaled);
            if (rounded < static_cast<double>(std::numeric_limits<std::int32_t>::min()) ||
                rounded > static_cast<double>(std::numeric_limits<std::int32_t>::max())) {
                throw std::out_of_range("SpreadPack fixed-point value is out of range");
            }

            const auto fixed = static_cast<std::int32_t>(rounded);
            return static_cast<std::uint32_t>(fixed);
        }
    };

} // namespace optionx

#endif // OPTIONX_HEADER_DATA_TRADING_SPREAD_PACK_HPP_INCLUDED
