#pragma once
#ifndef _OPTIONX_TIME_UTILS_HPP_INCLUDED
#define _OPTIONX_TIME_UTILS_HPP_INCLUDED

/// \file time_utils.hpp
/// \brief Utility definitions for working with time and timestamps

#include <time_shield_cpp/time_shield.hpp>
#ifndef OPTIONX_TIMESTAMP_MS
#define OPTIONX_TIMESTAMP_MS time_shield::timestamp_ms()
#endif


#endif // _OPTIONX_TIME_UTILS_HPP_INCLUDED
