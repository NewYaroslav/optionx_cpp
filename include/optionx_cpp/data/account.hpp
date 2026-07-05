#pragma once
#ifndef OPTIONX_HEADER_DATA_ACCOUNT_HPP_INCLUDED
#define OPTIONX_HEADER_DATA_ACCOUNT_HPP_INCLUDED

/// \file account.hpp
/// \brief Core components for account management and data querying.

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <kurlyk/types.hpp>

#include "IEndpointConfig.hpp"
#include "trading.hpp"
#include "account/enums.hpp"
#include "account/AccountInfoRequest.hpp"
#include "account/IAuthData.hpp"
#include "account/BaseAccountInfoData.hpp"
#include "account/AccountInfoUpdate.hpp"
#include "account/TradingConditionUpdate.hpp"
#include "account/ConnectionResult.hpp"

#endif // OPTIONX_HEADER_DATA_ACCOUNT_HPP_INCLUDED
