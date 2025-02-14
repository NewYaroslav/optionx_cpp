#pragma once
#ifndef _OPTIONX_BRIDGES_BASE_BRIDGE_HPP_INCLUDED
#define _OPTIONX_BRIDGES_BASE_BRIDGE_HPP_INCLUDED

/// \file BaseBridge.hpp
/// \brief Declares the BaseBridge abstract class, serving as a foundation for bridge implementations.

namespace optionx::bridges {

    /// \class BaseBridge
    /// \brief Abstract base class for bridges facilitating communication between the application and external platforms.
    class BaseBridge {
    public:
        using place_trade_callback_t = std::function<void(std::unique_ptr<TradeRequest>)>;

        /// \brief Virtual destructor for safe polymorphic destruction.
        virtual ~BaseBridge() = default;

        /// \brief Configures the bridge with the provided configuration data.
        /// \param config Unique pointer to the bridge configuration data.
        /// \return True if the configuration was set successfully; false otherwise.
        virtual bool configure(std::unique_ptr<IBridgeConfig> config) {
            if (!config) return false;
            // Process the configuration data as needed.
            return true;
        }

        /// \brief Retrieves a reference to the bridge status update callback function.
        /// \return Reference to the status update callback function.
        virtual bridge_status_callback_t& on_status_update() {
            static bridge_status_callback_t null_callback;
            return null_callback;
        }

        /// \brief Retrieves a reference to the place trade callback function.
        /// \return Reference to the place trade callback function.
        virtual place_trade_callback_t& on_place_trade() {
            static place_trade_callback_t null_callback;
            return null_callback;
        }

        /// \brief Retrieves a reference to the trade result callback function.
        /// \return Reference to the trade result callback function.
        virtual trade_result_callback_t& on_trade_result() {
            static trade_result_callback_t null_callback;
            return null_callback;
        }

        /// \brief Updates the account information with the provided data.
        /// \param info Structure containing account information updates.
        virtual void update_account_info(const AccountInfoUpdate& info) = 0;

        /// \brief Initiates the bridge's main operations.
        virtual void run() = 0;

        /// \brief Shuts down the bridge, terminating its operations.
        virtual void shutdown() = 0;
    };

} // namespace optionx::bridges

#endif // _OPTIONX_BRIDGES_BASE_BRIDGE_HPP_INCLUDED
