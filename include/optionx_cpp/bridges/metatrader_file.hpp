#pragma once
#ifndef OPTIONX_HEADER_BRIDGES_METATRADER_FILE_HPP_INCLUDED
#define OPTIONX_HEADER_BRIDGES_METATRADER_FILE_HPP_INCLUDED

/// \file metatrader_file.hpp
/// \brief Includes MetaTrader file-transport bridge headers.

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <optionx_cpp/data/bridge.hpp>
#include <optionx_cpp/data/trading.hpp>
#include <optionx_cpp/utils/metatrader_paths.hpp>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include "BaseBridge.hpp"
#include "metatrader_file/MetaTraderFilePathUtils.hpp"
#include "metatrader_file/MetaTraderFileBridgeConfig.hpp"
#include "metatrader_file/detail/MetaTraderFileProtocol.hpp"

#endif // OPTIONX_HEADER_BRIDGES_METATRADER_FILE_HPP_INCLUDED
