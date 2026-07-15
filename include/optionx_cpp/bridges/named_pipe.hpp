#pragma once
#ifndef OPTIONX_HEADER_BRIDGES_NAMED_PIPE_HPP_INCLUDED
#define OPTIONX_HEADER_BRIDGES_NAMED_PIPE_HPP_INCLUDED

/// \file named_pipe.hpp
/// \brief Includes named-pipe bridge headers.


#include <algorithm>
#include <cctype>
#include <cstdint>
#include <limits>
#include <cstddef>
#include <utility>
#include <chrono>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include <logit_cpp/logit.hpp>
#include <nlohmann/json.hpp>
#include <time_shield.hpp>

#include "BaseBridge.hpp"
#include <optionx_cpp/utils/tasks.hpp>

#if defined(_WIN32)
#include <SimpleNamedPipe/NamedPipeServer.hpp>
#endif

#include "named_pipe/LegacyTradingBridgeConfig.hpp"
#include "named_pipe/detail/LegacyTradingProtocol.hpp"
#include "named_pipe/LegacyTradingBridge.hpp"

#endif // OPTIONX_HEADER_BRIDGES_NAMED_PIPE_HPP_INCLUDED
