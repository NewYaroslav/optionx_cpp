#pragma once
#ifndef _OPTIONX_BAR_TYPES_HPP_INCLUDED
#define _OPTIONX_BAR_TYPES_HPP_INCLUDED

/// \file types.hpp
/// \brief Defines shared bar data types.

#include <cstdint>

namespace optionx {

    using BarTimeframe = std::int32_t; ///< Bar timeframe in seconds; values <= 0 are invalid.

} // namespace optionx

#endif // _OPTIONX_BAR_TYPES_HPP_INCLUDED
