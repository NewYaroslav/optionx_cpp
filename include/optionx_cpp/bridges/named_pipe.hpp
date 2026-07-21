#pragma once
#ifndef OPTIONX_HEADER_BRIDGES_NAMED_PIPE_HPP_INCLUDED
#define OPTIONX_HEADER_BRIDGES_NAMED_PIPE_HPP_INCLUDED

/// \file named_pipe.hpp
/// \brief Compatibility include for the legacy named-pipe trading bridge.

#include "legacy_trading.hpp"

namespace optionx::bridges::named_pipe {
    using LegacyTradingBridge = legacy_trading::LegacyTradingBridge;
    using LegacyTradingBridgeConfig = legacy_trading::LegacyTradingBridgeConfig;
    namespace detail = legacy_trading::detail;
}

#endif // OPTIONX_HEADER_BRIDGES_NAMED_PIPE_HPP_INCLUDED
