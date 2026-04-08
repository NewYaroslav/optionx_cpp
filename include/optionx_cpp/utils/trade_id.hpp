#pragma once
#ifndef _OPTIONX_UTILS_TRADE_ID_HPP_INCLUDED
#define _OPTIONX_UTILS_TRADE_ID_HPP_INCLUDED

/// \file TradeId.hpp
/// \brief Thread-safe generator for unique trade identifiers.
/// C++17+: one inline variable per binary.
/// C++11 : define OPTIONX_DEFINE_TRADE_ID_COUNTER in exactly ONE TU before including this header.

#include <atomic>
#include <cstdint>

namespace optionx::utils {

    using TradeId = std::uint64_t;
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
        return g_trade_id_counter.fetch_add(1, std::memory_order_relaxed);
    }

} // namespace optionx::utils

#endif // _OPTIONX_UTILS_TRADE_ID_HPP_INCLUDED
