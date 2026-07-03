#pragma once
#ifndef _OPTIONX_COMPONENTS_HPP_INCLUDED
#define _OPTIONX_COMPONENTS_HPP_INCLUDED

/// \file components.hpp
/// \brief Public aggregate header for broker platform building blocks.
///
/// These components are reusable building blocks used by broker API implementations:
/// lifecycle integration, HTTP client plumbing, account-info handlers, and
/// trade execution queues. Application-level code normally uses
/// `platforms.hpp` or a concrete platform facade instead.

#include "utils.hpp"
#include "data.hpp"
#include "components/BaseEndpoint.hpp"
#include "components/BaseComponent.hpp"
#include "components/BaseTradeExecutionComponent.hpp"
#include "components/BaseHttpClientComponent.hpp"
#include "components/BaseAccountInfoHandler.hpp"

#endif // _OPTIONX_COMPONENTS_HPP_INCLUDED
