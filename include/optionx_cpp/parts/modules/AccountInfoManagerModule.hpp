#pragma once
#ifndef _OPTIONX_MODULES_ACCOUNT_INFO_MANAGER_MODULE_HPP_INCLUDED
#define _OPTIONX_MODULES_ACCOUNT_INFO_MANAGER_MODULE_HPP_INCLUDED

/// \file AccountInfoManagerModule.hpp
/// \brief Defines the AccountInfoManagerModule class responsible for managing and updating account information.

#include "../pubsub/EventMediator.hpp"
#include "../interfaces/IAccountInfoData.hpp"
#include "events/OpenTradesEvent.hpp"
#include "events/TradeRequestEvent.hpp"
#include "events/TradeStatusEvent.hpp"
#include "events/TradeTransactionEvent.hpp"
#include "events/AccountInfoRequestEvent.hpp"
#include "events/AccountInfoUpdateEvent.hpp"
#include "events/AccountInfoEvent.hpp"
#include "../utils/Enums.hpp"
#include <mutex>

namespace optionx {
namespace modules {

    /// \class AccountInfoManagerModule
    /// \brief Manages and updates account information data.
    class AccountInfoManagerModule : public EventMediator {
    public:
        /// \typedef account_info_callback_t
        /// \brief Type alias for the callback function used to handle account information updates.
        using account_info_callback_t = std::function<void(std::unique_ptr<IAccountInfoData>)>;

        /// \brief Constructor initializing the account information manager.
        /// \param hub Reference to the event hub for event handling.
        /// \param account_info Shared pointer to the initial account information data.
        explicit AccountInfoManagerModule(EventHub& hub, std::shared_ptr<IAccountInfoData> account_info)
            : EventMediator(hub), m_account_info(std::move(account_info)) {
            subscribe<OpenTradesEvent>(this);
            subscribe<BalanceResponseEvent>(this);
            subscribe<TradeRequestEvent>(this);
            subscribe<TradeStatusEvent>(this);
            subscribe<TradeTransactionEvent>(this);
            subscribe<AccountInfoUpdateEvent>(this);
        }

        /// \brief Default virtual destructor.
        virtual ~AccountInfoManagerModule() = default;

        /// \brief Handles an event notification received as a shared pointer.
        /// \param event The event received, passed as a shared pointer.
        void on_event(const std::shared_ptr<Event>& event) override {
            if (auto msg = std::dynamic_pointer_cast<OpenTradesEvent>(event)) {
                handle_event(msg.get());
            } else if (auto msg = std::dynamic_pointer_cast<BalanceResponseEvent>(event)) {
                handle_event(msg.get());
            } else if (auto msg = std::dynamic_pointer_cast<TradeRequestEvent>(event)) {
                handle_event(msg.get());
            } else if (auto msg = std::dynamic_pointer_cast<TradeStatusEvent>(event)) {
                handle_event(msg.get());
            } else if (auto msg = std::dynamic_pointer_cast<TradeTransactionEvent>(event)) {
                handle_event(msg.get());
            } else if (auto msg = std::dynamic_pointer_cast<AccountInfoUpdateEvent>(event)) {
                handle_event(msg.get());
            }
        }

        /// \brief Handles an event notification received as a raw pointer.
        /// \param event The event received, passed as a raw pointer.
        void on_event(const Event* const event) override {
            if (const auto* msg = dynamic_cast<const OpenTradesEvent*>(event)) {
                handle_event(msg);
            } else if (const auto* msg = dynamic_cast<const BalanceResponseEvent*>(event)) {
                handle_event(msg);
            } else if (const auto* msg = dynamic_cast<const TradeRequestEvent*>(event)) {
                handle_event(msg);
            } else if (const auto* msg = dynamic_cast<const TradeStatusEvent*>(event)) {
                handle_event(msg);
            } else if (const auto* msg = dynamic_cast<const TradeTransactionEvent*>(event)) {
                handle_event(msg);
            } else if (const auto* msg = dynamic_cast<const AccountInfoUpdateEvent*>(event)) {
                handle_event(msg);
            }
        }

        /// \brief Sets a callback to receive updates on account information (e.g., balance, connection status).
        /// \param callback Function to handle account information updates.
        void set_account_info_callback(account_info_callback_t callback) {
            std::lock_guard<std::mutex> lock(m_account_info_mutex);
            m_account_info_callback = std::move(callback);
        }

        /// \brief Processes all pending requests or events.
        virtual void process() = 0;

    protected:

        /// \brief Handles an OpenTradesEvent to update account information.
        /// \param event Pointer to the OpenTradesEvent containing information about open trades.
        virtual void handle_event(const OpenTradesEvent* event) = 0;

        /// \brief Handles a BalanceResponseEvent to update account information.
        /// \param event Pointer to the BalanceResponseEvent containing updated balance information.
        virtual void handle_event(const BalanceResponseEvent* event) = 0;

        /// \brief Handles a TradeRequestEvent to update account information.
        /// \param event Pointer to the TradeRequestEvent containing details of a trade request.
        virtual void handle_event(const TradeRequestEvent* event) = 0;

        /// \brief Handles a TradeStatusEvent to update account information.
        /// \param event Pointer to the TradeStatusEvent containing the status of a trade.
        virtual void handle_event(const TradeStatusEvent* event) = 0;

        /// \brief Handles a TradeTransactionEvent to update account information.
        /// \param event Pointer to the TradeTransactionEvent containing details of a trade transaction.
        virtual void handle_event(const TradeTransactionEvent* event) = 0;

        /// \brief Handles an AccountInfoUpdateEvent to update account information.
        /// \param event Pointer to the AccountInfoUpdateEvent containing updated account data.
        virtual void handle_event(const AccountInfoUpdateEvent* event) = 0;

        /// \brief Invokes the registered callback with the updated account information.
        void invoke_callback() {
            std::lock_guard<std::mutex> lock(m_account_info_mutex);
            if (m_account_info_callback) {
                auto cloned_account_info = m_account_info->clone_unique();
                m_account_info_callback(std::move(cloned_account_info));
            }
        }

    private:
        mutable std::mutex                  m_account_info_mutex;       ///< Mutex for thread-safe access to account information.
        std::shared_ptr<IAccountInfoData>   m_account_info;             ///< Current account information data.
        account_info_callback_t             m_account_info_callback;    ///< Callback function for account information updates.
    }; // AccountInfoManagerModule

} // namespace modules
} // namespace optionx

#endif // _OPTIONX_MODULES_ACCOUNT_INFO_MANAGER_MODULE_HPP_INCLUDED
