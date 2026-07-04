#pragma once
#ifndef _OPTIONX_TICK_FLAGS_HPP_INCLUDED
#define _OPTIONX_TICK_FLAGS_HPP_INCLUDED

/// \file flags.hpp
/// \brief Defines flags for tick data status and update events

#include <cstdint>

namespace optionx {

    /// \brief Flags describing updates in tick data
    enum class TickUpdateFlags : std::uint32_t {
        NONE = 0,                   ///< No updates
        BID_UPDATED    = 1 << 0,    ///< Bid price updated
        ASK_UPDATED    = 1 << 1,    ///< Ask price updated
		LAST_UPDATED   = 1 << 2,    ///< Last price updated
        VOLUME_UPDATED = 1 << 3,    ///< Volume updated
        TICK_FROM_BUY  = 1 << 4,    ///< Tick resulted from a buy trade
        TICK_FROM_SELL = 1 << 5,    ///< Tick resulted from a sell trade
        BEST_MATH      = 1 << 6     ///< Tick matched the best price in the order book at the time of execution
    };

    /// \brief Combines two UpdateFlags using bitwise OR
    /// \param a First flag
    /// \param b Second flag
    /// \return Combined flags
    constexpr std::uint32_t operator|(TickUpdateFlags lhs, TickUpdateFlags rhs) {
        return static_cast<std::uint32_t>(lhs) | static_cast<std::uint32_t>(rhs);
    }

    /// \brief Checks if specific flags are set in a bitmask
    /// \param flags The bitmask of flags
    /// \param flag The flag to check
    /// \return True if the flag is set, false otherwise
    inline bool has_flag(std::uint32_t flags, TickUpdateFlags flag) {
        return (flags & static_cast<std::uint32_t>(flag)) != 0;
    }

	/// \brief Sets a specific flag in a bitmask.
	/// \param flags The bitmask of flags.
	/// \param flag The flag to set.
	inline void set_flag_in_place(std::uint32_t& flags, TickUpdateFlags flag) {
		flags = flags | static_cast<std::uint32_t>(flag);
	}

	/// \brief Sets a specific flag in a bitmask.
	/// \param flags The bitmask of flags.
	/// \param flag The flag to set.
	/// \return The updated bitmask with the specified flag set.
	inline std::uint32_t set_flag(std::uint32_t flags, TickUpdateFlags flag) {
		return flags | static_cast<std::uint32_t>(flag);
	}

	/// \brief Clears a specific flag in a bitmask.
	/// \param flags The bitmask of flags.
	/// \param flag The flag to clear.
	/// \return The updated bitmask with the specified flag cleared.
	inline std::uint32_t clear_flag(std::uint32_t flags, TickUpdateFlags flag) {
		return flags & ~static_cast<std::uint32_t>(flag);
	}

} // namespace optionx

#endif // _OPTIONX_TICK_FLAGS_HPP_INCLUDED
