#pragma once
#ifndef _OPTIONX_FLAGS_HPP_INCLUDED
#define _OPTIONX_FLAGS_HPP_INCLUDED

/// \file Flags.hpp
/// \brief Contains flag constants for use in SeriesData and other structures.

namespace optionx {
    const uint16_t REALTIME_FLAG = 0x01; ///< Flag indicating real-time data.
    const uint16_t INIT_FLAG     = 0x02; ///< Flag indicating initialization status.
}; // namespace optionx

#endif // _OPTIONX_FLAGS_HPP_INCLUDED
