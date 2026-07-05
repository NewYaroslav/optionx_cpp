#pragma once
#ifndef OPTIONX_HEADER_DATA_TRADING_TYPES_HPP_INCLUDED
#define OPTIONX_HEADER_DATA_TRADING_TYPES_HPP_INCLUDED

/// \file types.hpp
/// \brief Defines shared trading data identifier types.

#include <cstdint>

namespace optionx {

    using BridgeId = std::uint32_t; ///< Runtime bridge/source identifier; 0 means "not assigned".
    using SignalId = std::uint32_t; ///< Persistent signal identifier; 0 means "not assigned".

} // namespace optionx

#endif // OPTIONX_HEADER_DATA_TRADING_TYPES_HPP_INCLUDED
