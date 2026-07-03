#pragma once
#ifndef _OPTIONX_PLATFORMS_HPP_INCLUDED
#define _OPTIONX_PLATFORMS_HPP_INCLUDED

/// \file platforms.hpp
/// \brief Includes platform-related headers.
/// \note Headers under platforms subdirectories are internal components and are
/// intended to be included through this umbrella header.

#include "utils.hpp"
#include "data.hpp"
#include "market_data.hpp"
#include "storages.hpp"
#include "components.hpp"
#include "platforms/common/ApiResult.hpp"
#include "platforms/common/BaseTradingApi.hpp"
#include "platforms/common/BaseTradingPlatform.hpp"
#include "platforms/IntradeBarPlatform.hpp"
//#include "platforms/TradeUpPlatform.hpp"

#endif // _OPTIONX_PLATFORMS_HPP_INCLUDED
