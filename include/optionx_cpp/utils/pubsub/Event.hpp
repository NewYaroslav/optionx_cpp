#pragma once
#ifndef _OPTIONX_UTILS_PUBSUB_EVENT_HPP_INCLUDED
#define _OPTIONX_UTILS_PUBSUB_EVENT_HPP_INCLUDED

/// \file Event.hpp
/// \brief Defines the base Event class used in the publish-subscribe pattern.

#include <typeindex>

namespace optionx::utils {

    /// \class Event
    /// \brief Base class for events in the publish-subscribe system.
    ///
    /// Derived classes represent concrete events that can be published and dispatched
    /// using the EventHub. This abstract class provides a common base for all event types.
    class Event {
    public:
        /// \brief Default virtual destructor.
        virtual ~Event() = default;

        /// \brief Returns the runtime type information of the event.
        /// \return The type index representing the concrete event type.
        /// \note This method allows identifying the exact type of the event at runtime.
        ///       Useful for dispatching or logging without relying on dynamic_cast.
        virtual std::type_index type() const = 0;
        
        /// \brief Returns the name of the event type.
        /// \return A string literal representing the name of the event.
        /// \note This method is intended for debugging, logging, or serialization purposes.
        virtual const char* name() const = 0;
        
        /// \brief Checks whether the event is of the specified type.
        /// \tparam T The type to check against.
        /// \return true if the event is of type T; otherwise false.
        template <typename T>
        bool is() const {
            return type() == typeid(T);
        }

        /// \brief Attempts to cast the event to the specified type.
        /// \tparam T The target type to cast to.
        /// \return Pointer to the event cast to T, or nullptr if the type does not match.
        template <typename T>
        const T* as() const {
            return is<T>() ? static_cast<const T*>(this) : nullptr;
        }
        
        /// \brief Casts the event to the specified type by reference.
        /// \tparam T The target type to cast to.
        /// \return Reference to the event cast to T.
        /// \throws std::bad_cast if the event is not of type T.
        template <typename T>
        const T& asRef() const {
            if (!is<T>()) {
                throw std::bad_cast{};
            }
            return static_cast<const T&>(*this);
        }
    };

} // namespace optionx::utils

#endif // _OPTIONX_UTILS_PUBSUB_EVENT_HPP_INCLUDED
