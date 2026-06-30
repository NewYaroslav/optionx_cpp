#pragma once
#ifndef _OPTIONX_COMPONENTS_BASE_COMPONENT_HPP_INCLUDED
#define _OPTIONX_COMPONENTS_BASE_COMPONENT_HPP_INCLUDED

/// \file BaseComponent.hpp
/// \brief Defines the BaseComponent class for event-driven components with default lifecycle implementations.

namespace optionx::components {

    /// \class BaseComponent
    /// \brief Base class for event-driven components with default implementations for lifecycle methods.
    ///
    /// The `BaseComponent` class provides default "do nothing" implementations for the methods `initialize`, `process`,
    /// and `shutdown`. Derived classes can override only the methods they require. This class is designed to integrate
    /// with an `EventBus` through its base `EventMediator` class, enabling event-driven behavior.
    class BaseComponent : public utils::EventMediator {
    public:
        /// \brief Constructs a `BaseComponent` instance.
        /// \param bus Pointer to the `EventBus` used for handling events.
        explicit BaseComponent(utils::EventBus& bus) : EventMediator(bus) {}

        /// \brief Virtual destructor.
        /// Ensures proper cleanup of resources in derived classes.
        ~BaseComponent() noexcept override = default;

        /// \brief Handles an event notification received as a raw pointer.
        /// \param event The event received, passed as a raw pointer.
        void on_event(const utils::Event* const event) override {}

        /// \brief Initializes the component.
        ///
        /// This method is called to prepare the component before it starts processing events.
        /// The default implementation performs no operations. Derived classes can override
        /// this method to implement their specific initialization logic.
        virtual void initialize() {}

        /// \brief Processes component-specific logic.
        ///
        /// This method should be called periodically (e.g., within a main loop) to allow the
        /// component to perform its tasks, such as handling events or updating state.
        /// The default implementation performs no operations. Derived classes can override
        /// this method to implement their specific processing logic.
        virtual void process() {}

        /// \brief Shuts down the component.
        ///
        /// This method is called to clean up resources or stop ongoing operations before
        /// the component is destroyed. The default implementation performs no operations.
        /// Derived classes can override this method to implement their specific shutdown logic.
        virtual void shutdown() {}
    };

} // namespace optionx::components

#endif // _OPTIONX_COMPONENTS_BASE_COMPONENT_HPP_INCLUDED
