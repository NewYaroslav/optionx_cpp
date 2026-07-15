#pragma once
#ifndef OPTIONX_HEADER_BRIDGES_METATRADER_FILE_HPP_INCLUDED
#define OPTIONX_HEADER_BRIDGES_METATRADER_FILE_HPP_INCLUDED

/// \file metatrader_file.hpp
/// \brief Includes MetaTrader file-transport bridge headers.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "data/bridge.hpp"
#include "data/trading.hpp"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include "BaseBridge.hpp"
#include "metatrader_file/MetaTraderFileBridgeConfig.hpp"
#include "metatrader_file/detail/MetaTraderFileProtocol.hpp"

#endif // OPTIONX_HEADER_BRIDGES_METATRADER_FILE_HPP_INCLUDED
