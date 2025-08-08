#pragma once
#ifndef _OPTIONX_PLATFORMS_BASE_TRADING_PLATFORM_HPP_INCLUDED
#define _OPTIONX_PLATFORMS_BASE_TRADING_PLATFORM_HPP_INCLUDED

/// \file BaseTradingPlatform.hpp
/// \brief Base class for interacting with trading platforms.

namespace optionx::platforms {

    /// \class BaseTradingPlatform
    /// \brief Base class for interacting with trading platforms, handling trades, market data, and account management.
    class BaseTradingPlatform {
    public:
        using trade_result_callback_t = std::function<void(std::unique_ptr<TradeRequest>, std::unique_ptr<TradeResult>)>;
        using bars_callback_t  = std::function<void(const std::vector<BarData>&)>;
        using ticks_callback_t = std::function<void(const std::vector<TickData>&)>;

        BaseTradingPlatform(std::shared_ptr<BaseAccountInfoData> account_info)
            : m_account_info(std::move(account_info)),
              m_account_provider(m_account_info),
              m_event_bus(), m_task_manager(),
              m_account_info_handler(m_event_bus) {
        }

        virtual ~BaseTradingPlatform() {
            shutdown();
        }

        /// \brief Returns a reference to the trade result callback.
        /// \return Reference to the stored trade result callback, or a null function if not set.
        virtual trade_result_callback_t& on_trade_result() {
            static trade_result_callback_t null_callback;
            return null_callback;
        }

        /// \brief Returns a reference to the account info callback.
        virtual account_info_callback_t& on_account_info() {
            return m_account_info_handler.on_account_info();
        }

        /// \brief Returns a reference to the candle data callback.
        virtual bars_callback_t& on_candle_data() {
            static bars_callback_t null_callback;
            return null_callback;
        }

        /// \brief Returns a reference to the tick data callback.
        virtual ticks_callback_t& on_tick_data() {
            static ticks_callback_t null_callback;
            return null_callback;
        }

        /// \brief Sets the authorization data for the platform.
        /// \param auth_data Authorization data.
        /// \return True if the authorization data was set successfully; false otherwise.
        virtual bool configure_auth(std::unique_ptr<IAuthData> auth_data) {
            if (!auth_data) return false;
            m_event_bus.notify_async(std::make_unique<events::AuthDataEvent>(std::move(auth_data)));
            return true;
        }

        /// \brief Places a trade based on the specified trade request.
        /// \param trade_request Trade request details.
        /// \return True if the trade was placed successfully; false otherwise.
        virtual bool place_trade(std::unique_ptr<TradeRequest> trade_request) { return false; };

        /// \brief Requests historical candle data for a specified time range.
        /// \param request Historical candle data request parameters.
        /// \param callback Callback function to receive the candle data.
        virtual bool fetch_candle_data(
            const BarHistoryRequest& request,
            std::function<void(const BarSequence&)> callback) { return false; };

        /// \brief Requests the list of available trading symbols.
        /// \param callback Callback function to receive the symbol list.
        virtual bool fetch_symbol_list(std::function<void(const std::vector<SymbolInfo>&)> callback) { return false; };

        /// \brief Initiates a connection to the trading platform.
        /// \param callback Callback to handle connection result with error code.
        virtual void connect(connection_callback_t callback) {
            m_event_bus.notify_async(std::make_unique<events::ConnectRequestEvent>(std::move(callback)));
        }

        /// \brief Disconnects from the trading platform.
        /// \param callback Callback to handle completion of the disconnection process.
        virtual void disconnect(connection_callback_t callback) {
            m_event_bus.notify_async(std::make_unique<events::DisconnectRequestEvent>(std::move(callback)));
        }

        /// \brief Checks if the platform is connected.
        /// \return True if connected, otherwise false.
        virtual bool is_connected() const {
            return m_account_provider.get_info<bool>(AccountInfoType::CONNECTION_STATUS);
        }

        /// \brief Retrieves account information based on a detailed request.
        /// \tparam T The expected type of the returned value.
        /// \param request The request containing specific parameters for data retrieval.
        /// \return The requested account information of type `T`.
        template<class T>
        T get_info(const AccountInfoRequest& request) const {
            return m_account_provider.get_info<T>(request);
        }

        /// \brief Retrieves account information by `AccountInfoType`.
        /// \tparam T The expected type of the returned value.
        /// \param type The specific type of account information.
        /// \param timestamp Optional timestamp for retrieving historical values (default: 0).
        /// \return The requested account information of type `T`.
        template<class T>
        T get_info(AccountInfoType type, int64_t timestamp = 0) const {
            return m_account_provider.get_info<T>(type, timestamp);
        }

        /// \brief Retrieves account information by account type.
        /// \tparam T The expected type of the returned value.
        /// \param account The `AccountType` (e.g., Demo, Real).
        /// \param timestamp Optional timestamp for retrieving historical values (default: 0).
        /// \return The requested account information of type `T`
        template<class T>
        T get_info(AccountType account, int64_t timestamp = 0) const {
            return m_account_provider.get_info<T>(account, timestamp);
        }

        /// \brief Retrieves account information by currency type.
        /// \tparam T The expected type of the returned value.
        /// \param currency The `CurrencyType` (e.g., USD, EUR).
        /// \param timestamp Optional timestamp for retrieving historical values (default: 0).
        /// \return The requested account information of type `T`.
        template<class T>
        T get_info(CurrencyType currency, int64_t timestamp = 0) const {
            return m_account_provider.get_info<T>(currency, timestamp);
        }

        /// \brief Starts the platform's event loop and module lifecycle.
        /// \details Adds initialization and periodic update tasks.
        ///          If use_internal_thread is true (default), TaskManager launches its own worker thread.
        ///          Otherwise, the caller must periodically call process() manually.
        /// \param use_internal_thread Whether to use an internal background thread for updates.
        void run(bool use_internal_thread = true) {
            m_task_manager.add_single_task("initialize", [this](
                    std::shared_ptr<utils::Task> task){
                if (task->is_shutdown()) return;
                LOGIT_TRACE0();
                for (auto* module : m_modules) {
                    module->initialize();
                }
                on_once();
            });
            
            m_task_manager.add_periodic_task("loop", 1, [this](
                    std::shared_ptr<utils::Task> task){
                m_event_bus.process();
                if (task->is_shutdown()) {
                    LOGIT_TRACE0();
                    for (auto* module : m_modules) {
                        module->shutdown();
                    }
                    on_shutdown();
                } else {
                    for (auto* module : m_modules) {
                        module->process();
                    }
                    on_loop();
                }
            });
            
            if (!use_internal_thread) return;
            m_task_manager.run();
        };
        
        /// \brief Manually processes pending tasks and events.
        /// \details Should be called periodically if internal threading is disabled (run(false)).
        void process() {
            m_task_manager.process();
        }

        /// \brief Shuts down the platform, stopping the event loop and tasks.
        /// \details Always calls shutdown() on TaskManager, regardless of internal thread usage.
        void shutdown() {
            m_task_manager.shutdown();
        };

        /// \brief Returns a reference to the event bus.
        utils::EventBus& event_bus() { return m_event_bus; }

        /// \brief Registers a module for processing.
        /// \param module The module to be registered.
        void register_module(modules::BaseModule* module) {
            m_modules.push_back(module);
        }

        /// \brief Retrieves the API type associated with this authorization data.
        /// \return The `PlatformType` associated with this authentication data.
        virtual PlatformType platform_type() const = 0;

    protected:
        std::shared_ptr<BaseAccountInfoData> m_account_info;
        modules::AccountInfoProvider         m_account_provider;
        utils::EventBus                      m_event_bus;
        utils::TaskManager                   m_task_manager;
        modules::BaseAccountInfoHandler      m_account_info_handler;
        std::vector<modules::BaseModule*>    m_modules;

        virtual void on_once() {};

        virtual void on_loop() {};

        virtual void on_shutdown() {};
    };

}; // namespace optionx

#endif // _OPTIONX_PLATFORMS_BASE_TRADING_PLATFORM_HPP_INCLUDED
