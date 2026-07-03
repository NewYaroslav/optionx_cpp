#pragma once
#ifndef _OPTIONX_MARKET_DATA_HPP_INCLUDED
#define _OPTIONX_MARKET_DATA_HPP_INCLUDED

/// \file market_data.hpp
/// \brief Includes public market-data provider contracts.
/// \note Headers under market_data subdirectories are internal components and
/// intended to be included through this umbrella header.

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "data.hpp"
#include "market_data/enums.hpp"
#include "market_data/MarketDataSubscription.hpp"
#include "market_data/BaseMarketDataProvider.hpp"

#endif // _OPTIONX_MARKET_DATA_HPP_INCLUDED
