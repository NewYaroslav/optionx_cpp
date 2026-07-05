#pragma once
#ifndef OPTIONX_HEADER_UTILS_CORRELATION_ID_HPP_INCLUDED
#define OPTIONX_HEADER_UTILS_CORRELATION_ID_HPP_INCLUDED

/// \file CorrelationId.hpp
/// \brief Thread-safe generator for correlation identifiers (strongly-typed).
/// C++17+: one inline variable per binary.
/// C++11 : define OPTIONX_DEFINE_CORRELATION_ID_COUNTER in exactly ONE TU before including this header.

#include <atomic>
#include <cstdint>

namespace optionx::utils {

    struct CorrelationId {
        std::uint64_t v{0};
        explicit operator bool() const noexcept { return v != 0; }
        friend inline bool operator==(CorrelationId a, CorrelationId b) noexcept { return a.v == b.v; }
        friend inline bool operator!=(CorrelationId a, CorrelationId b) noexcept { return a.v != b.v; }
        friend inline bool operator< (CorrelationId a, CorrelationId b) noexcept { return a.v <  b.v; }
    };

    // Storage
#   if __cplusplus >= 201703L
        inline std::atomic<std::uint64_t> g_correlation_id_counter{1}; // 0 = invalid
#   else
#       ifdef OPTIONX_DEFINE_CORRELATION_ID_COUNTER
            std::atomic<std::uint64_t> g_correlation_id_counter(1);
#       else
            extern std::atomic<std::uint64_t> g_correlation_id_counter;
#       endif
#   endif

    inline CorrelationId make_cid() noexcept {
        return CorrelationId{ g_correlation_id_counter.fetch_add(1, std::memory_order_relaxed) };
    }

    constexpr CorrelationId null_cid() noexcept { return CorrelationId{0}; }

} // namespace optionx::utils

#endif // OPTIONX_HEADER_UTILS_CORRELATION_ID_HPP_INCLUDED
