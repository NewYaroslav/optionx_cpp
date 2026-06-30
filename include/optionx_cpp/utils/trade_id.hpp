#pragma once
#ifndef _OPTIONX_UTILS_TRADE_ID_HPP_INCLUDED
#define _OPTIONX_UTILS_TRADE_ID_HPP_INCLUDED

/// \file TradeId.hpp
/// \brief Thread-safe generator for unique trade identifiers.
/// C++17+: one inline variable per binary.
/// C++11 : define OPTIONX_DEFINE_TRADE_ID_COUNTER in exactly ONE TU before including this header.

#include <atomic>
#include <cstdint>
#include <limits>

namespace optionx::utils {

    using TradeId = std::uint32_t;
    static const TradeId kInvalidTradeId = 0;

    // Storage
#   if __cplusplus >= 201703L
        inline std::atomic<TradeId> g_trade_id_counter{1}; // 0 = invalid
#   else
#       ifdef OPTIONX_DEFINE_TRADE_ID_COUNTER
            std::atomic<TradeId> g_trade_id_counter(1);
#       else
            extern std::atomic<TradeId> g_trade_id_counter;
#       endif
#   endif

    inline TradeId make_trade_id() noexcept {
        auto current = g_trade_id_counter.load(std::memory_order_relaxed);
        for (;;) {
            const TradeId result =
                current == kInvalidTradeId ? TradeId{1} : current;
            const auto next =
                result == (std::numeric_limits<TradeId>::max)()
                    ? TradeId{1}
                    : static_cast<TradeId>(result + 1);
            if (g_trade_id_counter.compare_exchange_weak(
                    current,
                    next,
                    std::memory_order_relaxed,
                    std::memory_order_relaxed)) {
                return result;
            }
        }
    }

} // namespace optionx::utils

#endif // _OPTIONX_UTILS_TRADE_ID_HPP_INCLUDED
