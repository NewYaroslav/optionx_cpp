#pragma once
#ifndef _OPTIONX_MODULES_TRADE_MANAGER_MODULE_HPP_INCLUDED
#define _OPTIONX_MODULES_TRADE_MANAGER_MODULE_HPP_INCLUDED

/// \file BaseTradeExecutionModule.hpp
/// \brief Defines the BaseTradeExecutionModule class for managing trade requests and processing transactions.

#include "BaseTradeExecutionModule/AccountInfoProvider.hpp"
#include "BaseTradeExecutionModule/TradeStateManager.hpp"
#include "BaseTradeExecutionModule/TradeQueueManager.hpp"

namespace optionx::modules {

    /// \class BaseTradeExecutionModule
    /// \brief Central module responsible for managing trade requests and processing transactions.
    ///
    /// This class acts as a high-level manager that handles trade requests, validates them, and processes pending orders.
    /// It delegates state management to `TradeStateManager` and queue management to `TradeQueueManager`.
    ///
    /// ### Subscribed Events:
    /// - `PriceUpdateEvent` – Updates trade states based on market prices.
    /// - `DisconnectRequestEvent` – Handles connection loss and forces trade finalization.
    ///
    /// ### Emitted Events:
    /// - `TradeTransactionEvent` – Notifies about trade request updates.
    /// - `TradeRequestEvent` – Sent when a trade request is accepted for processing.
    /// - `TradeStatusEvent` – Updates listeners on trade state changes.
    /// - `OpenTradesEvent` – Notifies about changes in the number of open trades.
    class BaseTradeExecutionModule : public BaseModule {
    public:
        using trade_result_callback_t = std::function<void(std::unique_ptr<TradeRequest>, std::unique_ptr<TradeResult>)>;

        /// \brief Constructs a `BaseTradeExecutionModule` instance.
        /// \param hub Reference to the `EventHub` for subscribing to and emitting events.
        /// \param account_info Shared pointer to the `BaseAccountInfoData` interface for retrieving account-related information.
        explicit BaseTradeExecutionModule(
                utils::EventHub& hub,
                std::shared_ptr<BaseAccountInfoData> account_info)
            : BaseModule(hub), m_account_info(std::move(account_info)),
              m_trade_state_manager(m_account_info),
              m_trade_queue(hub, m_account_info, m_trade_state_manager) {
        }

        /// \brief Default virtual destructor.
        virtual ~BaseTradeExecutionModule() = default;

        /// \brief Handles an event notification received as a shared pointer.
        /// \param event The received event.
        void on_event(const std::shared_ptr<utils::Event>& event) override {};

        /// \brief Handles an event notification received as a raw pointer.
        /// \param event The received event.
        void on_event(const utils::Event* const event) override {};

        /// \brief Sets a callback for trade result events (open, close, or errors).
        /// \param callback Function to handle trade result events.
        void set_trade_result_callback(trade_result_callback_t callback) {
            m_trade_queue.set_trade_result_callback(callback);
        }

        trade_result_callback_t& on_trade_result() {
            return m_trade_queue.on_trade_result();
        }

        /// \brief Validates and places a trade request into the pending queue.
        /// \param request Unique pointer to a trade request.
        /// \return True if the request passes validation and is added to the queue; false otherwise.
        bool place_trade(std::unique_ptr<TradeRequest> request) {
            return m_trade_queue.add_trade(std::move(request), platform_type());
        }

        /// \brief Initializes the module.
        void initialize() override {};

        /// \brief Processes all pending and active trades.
        void process() override {
            m_trade_queue.process();
        }

        /// \brief Finalizes all active and pending trades before shutdown.
        void shutdown() override {
            m_trade_queue.finalize_all_trades();
        }

    protected:
        AccountInfoProvider m_account_info; ///< Manages access to account-related data.
        TradeStateManager   m_trade_state_manager; ///< Handles trade state transitions and validation.
        TradeQueueManager   m_trade_queue; ///< Manages the queue of pending and active trade transactions.

        /// \brief Returns the platform type associated with this trade manager.
        /// \return The `PlatformType` of the trading module.
        virtual PlatformType platform_type() = 0;
    };

} // namespace optionx::modules

#endif // _OPTIONX_MODULES_TRADE_MANAGER_MODULE_HPP_INCLUDED
