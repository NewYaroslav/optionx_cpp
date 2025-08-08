#pragma once
#ifndef _OPTIONX_MODULES_BASE_ACCOUNT_INFO_HANDLER_HPP_INCLUDED
#define _OPTIONX_MODULES_BASE_ACCOUNT_INFO_HANDLER_HPP_INCLUDED

/// \file BaseAccountInfoHandler.hpp
/// \brief Base class for handling account information updates and events.

namespace optionx::modules {

    /// \class BaseAccountInfoHandler
    /// \brief Base class for handling account information updates and notifications.
    ///
    /// This class subscribes to `AccountInfoUpdateEvent` and invokes a callback
    /// whenever an account update occurs. Derived classes can use this to track
    /// changes in account balance, connection status, and other account-related details.
    class BaseAccountInfoHandler : public BaseModule {
    public:

        /// \brief Constructs the `BaseAccountInfoHandler` and subscribes to account update events.
        /// \param bus Reference to the `EventBus` used for subscribing to events.
        explicit BaseAccountInfoHandler(utils::EventBus& bus)
            : BaseModule(bus) {
           subscribe<events::AccountInfoUpdateEvent>();
        }

        /// \brief Virtual destructor.
        virtual ~BaseAccountInfoHandler() = default;

        /// \brief Handles an incoming event notification.
        /// \param event Pointer to the received event.
        void on_event(const utils::Event* const event) override {
            if (const auto* msg = dynamic_cast<const events::AccountInfoUpdateEvent*>(event)) {
                handle_event(*msg);
            }
        }

        /// \brief Returns a reference to the account info callback.
        /// \return Reference to the callback function that processes account updates.
        account_info_callback_t& on_account_info() {
            return m_callback;
        }

    private:
        account_info_callback_t m_callback; ///< Callback function for handling account updates.

        /// \brief Processes an `AccountInfoUpdateEvent` and invokes the callback.
        /// \param event The received account info update event.
        void handle_event(const events::AccountInfoUpdateEvent& event) {
            AccountInfoUpdate update{
                event.account_info->clone_shared(),
                event.status,
                event.message
            };
            if (m_callback) m_callback(update);
        }
    };

} // namespace optionx::modules

#endif // _OPTIONX_MODULES_BASE_ACCOUNT_INFO_HANDLER_HPP_INCLUDED
