#pragma once
#ifndef _OPTIONX_MODULES_HPP_INCLUDED
#define _OPTIONX_MODULES_HPP_INCLUDED

/// \file modules.hpp
/// \brief Public aggregate header for broker platform building blocks.
///
/// These modules are reusable pieces used by broker API implementations:
/// lifecycle integration, HTTP client plumbing, account-info handlers, and
/// trade execution queues. Application-level code normally uses
/// `platforms.hpp` or a concrete platform facade instead.

#include "utils.hpp"
#include "data.hpp"
#include "modules/BaseModule.hpp"
#include "modules/BaseTradeExecutionModule.hpp"
#include "modules/BaseHttpClientModule.hpp"
#include "modules/BaseAccountInfoHandler.hpp"

#endif // _OPTIONX_MODULES_HPP_INCLUDED
