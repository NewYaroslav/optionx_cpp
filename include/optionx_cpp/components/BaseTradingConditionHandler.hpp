#pragma once
#ifndef OPTIONX_HEADER_COMPONENTS_BASE_TRADING_CONDITION_HANDLER_HPP_INCLUDED
#define OPTIONX_HEADER_COMPONENTS_BASE_TRADING_CONDITION_HANDLER_HPP_INCLUDED

/// \file BaseTradingConditionHandler.hpp
/// \brief Defines an event-to-callback handler for trading-condition updates.

namespace optionx::components {

    /// \class BaseTradingConditionHandler
    /// \brief Subscribes to trading-condition events and invokes a callback.
    class BaseTradingConditionHandler : public BaseComponent {
    public:
        /// \brief Constructs the handler and subscribes to condition updates.
        /// \param bus Event bus used by the owning platform/component.
        explicit BaseTradingConditionHandler(utils::EventBus& bus)
            : BaseComponent(bus) {
            subscribe<events::TradingConditionUpdateEvent>();
        }

        /// \brief Virtual destructor.
        virtual ~BaseTradingConditionHandler() = default;

        /// \brief Handles incoming events.
        /// \param event Received event.
        void on_event(const utils::Event* const event) override {
            if (const auto* msg =
                    dynamic_cast<const events::TradingConditionUpdateEvent*>(event)) {
                handle_event(*msg);
            }
        }

        /// \brief Returns a reference to the condition callback.
        /// \return Callback invoked for trading-condition updates.
        trading_condition_callback_t& on_trading_condition() {
            return m_callback;
        }

    private:
        trading_condition_callback_t m_callback; ///< Trading-condition callback.

        /// \brief Processes an update event and invokes the callback.
        /// \param event Trading-condition update event.
        void handle_event(const events::TradingConditionUpdateEvent& event) {
            TradingConditionUpdate update = event;
            if (m_callback) m_callback(update);
        }
    };

} // namespace optionx::components

#endif // OPTIONX_HEADER_COMPONENTS_BASE_TRADING_CONDITION_HANDLER_HPP_INCLUDED
