#pragma once
#ifndef _OPTIONX_UTILS_TRADE_ID_GENERATOR_HPP_INCLUDED
#define _OPTIONX_UTILS_TRADE_ID_GENERATOR_HPP_INCLUDED

/// \file TradeIdGenerator.hpp
/// \brief Provides a thread-safe singleton class for generating unique trade identifiers.

#include <atomic>
#include <cstdint>

namespace optionx::utils {

    /// \class TradeIdGenerator
    /// \brief Generates unique trade identifiers within the application's runtime.
    class TradeIdGenerator {
    public:
        /// \brief Retrieves the singleton instance of TradeIdGenerator.
        /// \return Reference to the singleton instance.
        static TradeIdGenerator& instance() {
            static TradeIdGenerator* s_instance = new TradeIdGenerator();
            return *s_instance;
        }

        /// \brief Generates a new unique trade identifier.
        /// \return A unique 64-bit integer identifier.
        int64_t generate_id() {
            return m_current_id.fetch_add(1, std::memory_order_relaxed);
        }

    private:
        std::atomic<int64_t> m_current_id{1}; ///< Atomic counter for generating unique IDs.

        /// \brief Private constructor to enforce singleton pattern.
        TradeIdGenerator() = default;

        /// \brief Deleted copy constructor to prevent copying.
        TradeIdGenerator(const TradeIdGenerator&) = delete;

        /// \brief Deleted assignment operator to prevent assignment.
        TradeIdGenerator& operator=(const TradeIdGenerator&) = delete;
    };

} // namespace optionx::utils

#endif // _OPTIONX_UTILS_TRADE_ID_GENERATOR_HPP_INCLUDED
