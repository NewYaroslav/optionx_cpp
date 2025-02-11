#pragma once
#ifndef _OPTIONX_BAR_FLAGS_HPP_INCLUDED
#define _OPTIONX_BAR_FLAGS_HPP_INCLUDED

/// \file BarFlags.hpp
/// \brief Defines flags for bar data status and update events

namespace dfh {

    /// \brief Flags indicating the status of bar data (real-time, historical, etc.)
    enum class BarStatusFlags : uint64_t {
        NONE        = 0,        ///< No flags set
        REALTIME    = 1 << 0,   ///< Data received in real-time
        HISTORICAL  = 1 << 1,   ///< Data has been initialized from history
        INCOMPLETE  = 1 << 2,   ///< Bar is still forming
        FINALIZED   = 1 << 3    ///< Bar is finalized and will not change
    };

} // namespace dfh

#endif // _OPTIONX_BAR_FLAGS_HPP_INCLUDED