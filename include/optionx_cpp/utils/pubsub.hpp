#pragma once
#ifndef OPTIONX_HEADER_UTILS_PUBSUB_HPP_INCLUDED
#define OPTIONX_HEADER_UTILS_PUBSUB_HPP_INCLUDED

/// \file pubsub.hpp
/// \brief Main header file for the publish-subscribe system.
///
/// This file serves as an entry point for the publish-subscribe component,
/// including all necessary components for event-driven communication.
/// It provides a convenient way to include the entire system with a single import.


#include "pubsub/Event.hpp"
#include "pubsub/EventListener.hpp"
#include "pubsub/EventBus.hpp"
#include "pubsub/EventAwaiter.hpp"
#include "pubsub/EventMediator.hpp"

#endif // OPTIONX_HEADER_UTILS_PUBSUB_HPP_INCLUDED
