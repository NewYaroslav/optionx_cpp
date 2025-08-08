#pragma once
#ifndef _OPTIONX_MODULES_BASE_MODULE_HPP_INCLUDED
#define _OPTIONX_MODULES_BASE_MODULE_HPP_INCLUDED

/// \file BaseModule.hpp
/// \brief Defines the BaseModule class for modular components with default lifecycle implementations.

namespace optionx::modules {

    /// \class BaseModule
    /// \brief Base class for modular components with default implementations for lifecycle methods.
    ///
    /// The `BaseModule` class provides default "do nothing" implementations for the methods `initialize`, `process`,
    /// and `shutdown`. Derived classes can override only the methods they require. This class is designed to integrate
    /// with an `EventBus` through its base `EventMediator` class, enabling event-driven behavior.
    class BaseModule : public utils::EventMediator {
    public:
        /// \brief Constructs a `BaseModule` instance.
        /// \param bus Pointer to the `EventBus` used for handling events.
        explicit BaseModule(utils::EventBus& bus) : EventMediator(bus) {}

        /// \brief Virtual destructor.
        ///
        /// Ensures proper cleanup of resources in derived classes.
        virtual ~BaseModule() = default;

        /// \brief Handles an event notification received as a raw pointer.
        /// \param event The event received, passed as a raw pointer.
        void on_event(const utils::Event* const event) override {}

        /// \brief Initializes the module.
        ///
        /// This method is called to prepare the module before it starts processing events.
        /// The default implementation performs no operations. Derived classes can override
        /// this method to implement their specific initialization logic.
        virtual void initialize() {}

        /// \brief Processes module-specific logic.
        ///
        /// This method should be called periodically (e.g., within a main loop) to allow the
        /// module to perform its tasks, such as handling events or updating state.
        /// The default implementation performs no operations. Derived classes can override
        /// this method to implement their specific processing logic.
        virtual void process() {}

        /// \brief Shuts down the module.
        ///
        /// This method is called to clean up resources or stop ongoing operations before
        /// the module is destroyed. The default implementation performs no operations.
        /// Derived classes can override this method to implement their specific shutdown logic.
        virtual void shutdown() {}
    };

} // namespace optionx::modules

#endif // _OPTIONX_MODULES_BASE_MODULE_HPP_INCLUDED
