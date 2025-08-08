#pragma once
#ifndef _OPTIONX_EVENTS_HPP_INCLUDED
#define _OPTIONX_EVENTS_HPP_INCLUDED

/// \file events.hpp
/// \brief Aggregated include for all core event types used across the OptionX system.
///
/// This header provides a single entry point to include all event definitions.
/// Include this file instead of individual event headers when working with multiple event types.


#include "events/AuthDataEvent.hpp"
#include "events/RestartAuthEvent.hpp"
#include "events/AutoDomainSelectedEvent.hpp"
#include "events/AccountInfoUpdateEvent.hpp"
#include "events/BalanceRequestEvent.hpp"
#include "events/PriceUpdateEvent.hpp"
#include "events/ConnectRequestEvent.hpp"
#include "events/DisconnectRequestEvent.hpp"
#include "events/TradeRequestEvent.hpp"
#include "events/TradeTransactionEvent.hpp"
#include "events/TradeStatusEvent.hpp"
#include "events/OpenTradesEvent.hpp"

#endif // _OPTIONX_EVENTS_HPP_INCLUDED
