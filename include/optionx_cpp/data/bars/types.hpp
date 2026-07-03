#pragma once
#ifndef _OPTIONX_BAR_TYPES_HPP_INCLUDED
#define _OPTIONX_BAR_TYPES_HPP_INCLUDED

/// \file types.hpp
/// \brief Defines shared bar data types.

#include <cstdint>

namespace optionx {

    /// \brief Bar timeframe in seconds.
    /// \details Request DTOs treat values less than or equal to zero as invalid.
    using BarTimeframe = std::int32_t;

} // namespace optionx

#endif // _OPTIONX_BAR_TYPES_HPP_INCLUDED
