#pragma once
#ifndef _OPTIONX_PUBSUB_EVENT_HPP_INCLUDED
#define _OPTIONX_PUBSUB_EVENT_HPP_INCLUDED

/// \file Event.hpp
/// \brief Defines the base Event class used in the publish-subscribe pattern.

namespace optionx {

    /// \class Event
    /// \brief Base class for events in the publish-subscribe pattern.
    /// This abstract class serves as the base for all specific event types
    /// that can be used within the publish-subscribe system. Derived classes
    /// represent concrete events, allowing the `EventHub` to handle and dispatch them.
    class Event {
    public:
        /// \brief Default virtual destructor.
        virtual ~Event() = default;
    };

} // namespace optionx

#endif // _OPTIONX_PUBSUB_EVENT_HPP_INCLUDED
