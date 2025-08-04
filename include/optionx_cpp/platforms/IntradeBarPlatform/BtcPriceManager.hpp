#pragma once
#ifndef _OPTIONX_PLATFORMS_INTRADERBAR_BTCUSDT_PRICE_MANAGER_HPP_INCLUDED
#define _OPTIONX_PLATFORMS_INTRADERBAR_BTCUSDT_PRICE_MANAGER_HPP_INCLUDED

/// \file BtcPriceManager.hpp
/// \brief Defines the BtcPriceManager class responsible for handling BTCUSDT price updates and related events.

namespace optionx::platforms::intrade_bar {

    /// \class BtcPriceManager
    /// \brief Manages BTCUSDT price updates and related events.
    ///
    /// The BtcPriceManager class is responsible for establishing a WebSocket connection
    /// to receive real-time BTCUSDT price updates. It processes incoming data and
    /// subscribes to events related to authentication, connection requests, disconnection
    /// requests, and account information updates to maintain up-to-date price information.
    class BtcPriceManager final : public modules::BaseModule {
    public:
        /// \brief Constructs the BTCUSDT price manager.
        /// \param platform Reference to the trading platform.
        explicit BtcPriceManager(
                BaseTradingPlatform& platform)
                : BaseModule(platform.event_hub()), m_websocket_client() {
            subscribe<events::AuthDataEvent>(this);
            subscribe<events::ConnectRequestEvent>(this);
            subscribe<events::DisconnectRequestEvent>(this);
            subscribe<events::AccountInfoUpdateEvent>(this);
            platform.register_module(this);

            m_tick_data.resize(1);
            m_tick_data[0].price_digits  = 2;
            m_tick_data[0].volume_digits = 5;
            m_tick_data[0].symbol = "BTCUSDT";
            m_tick_data[0].provider = to_str(PlatformType::INTRADE_BAR);

            m_websocket_client.on_event([this](std::unique_ptr<kurlyk::WebSocketEventData> event) {
                switch (event->event_type) {
                case kurlyk::WebSocketEventType::WS_OPEN:
                    LOGIT_INFO(event->status_code, event->error_code);
                    m_tick_data[0].tick.flags = 0;
                    m_tick_data[0].flags = 0;
                    m_is_error = false;
                    break;
                case kurlyk::WebSocketEventType::WS_MESSAGE:
                    handle_message(event->message);
                    break;
                case kurlyk::WebSocketEventType::WS_CLOSE:
                    LOGIT_INFO(event->status_code, event->error_code);
                    m_tick_data[0].tick.flags = 0;
                    m_tick_data[0].flags = 0;
                    m_is_error = false;
                    break;
                case kurlyk::WebSocketEventType::WS_ERROR:
                    if (m_is_error) return;
                    LOGIT_ERROR(event->status_code, event->error_code);
                    m_tick_data[0].tick.flags = 0;
                    m_tick_data[0].flags = 0;
                    m_is_error = true;
                    break;
                default:
                    break;
                };
            });
        }

        /// \brief Default destructor.
        virtual ~BtcPriceManager() {
            shutdown();
        }

        /// \brief Processes incoming events and dispatches them to the appropriate handlers.
        /// \param event The received event.
        void on_event(const utils::Event* const event) override;

        /// \brief Periodically processes tasks related to BTCUSDT price updates.
        void process() override;

        /// \brief Shuts down the WebSocket connection and related operations.
        void shutdown() override;

    private:
        kurlyk::WebSocketClient m_websocket_client; ///< WebSocket client for BTCUSDT.
        std::vector<TickData>   m_tick_data;        ///< Container for tick data.
        bool                    m_is_error = false; ///< Flag indicating if an error has occurred.

        /// \brief Handles incoming WebSocket messages.
        /// \param message The received message as a JSON string.
        void handle_message(const std::string& message);

        /// \brief Handles an authentication event.
        /// \param event The received authentication data event.
        void handle_event(const events::AuthDataEvent& event);

        /// \brief Handles an event triggered when a connection request is received.
        /// \param event The connection request event.
        void handle_event(const events::ConnectRequestEvent& event);

        /// \brief Handles an event triggered when a disconnection request is received.
        /// \param event The disconnection request event.
        void handle_event(const events::DisconnectRequestEvent& event);

        /// \brief Handles an account information update event.
        /// \param event The account info update event.
        void handle_event(const events::AccountInfoUpdateEvent& event);
    };

    void BtcPriceManager::on_event(const utils::Event* const event) {
        if (const auto* msg = dynamic_cast<const events::AuthDataEvent*>(event)) {
            handle_event(*msg);
        } else
        if (const auto* msg = dynamic_cast<const events::ConnectRequestEvent*>(event)) {
            handle_event(*msg);
        } else
        if (const auto* msg = dynamic_cast<const events::DisconnectRequestEvent*>(event)) {
            handle_event(*msg);
        } else
        if (const auto* msg = dynamic_cast<const events::AccountInfoUpdateEvent*>(event)) {
            handle_event(*msg);
        }

    }

    void BtcPriceManager::handle_event(const events::AuthDataEvent& event) {
        if (auto auth_data = std::dynamic_pointer_cast<AuthData>(event.auth_data)) {
            LOGIT_TRACE0();
            auto [success, message] = auth_data->validate();
            if (!success) return;
            std::string host = "wss://" + utils::remove_http_prefix(auth_data->host);
            m_websocket_client.set_url(host, "/bapi");
            m_websocket_client.set_user_agent(auth_data->user_agent);
            m_websocket_client.set_accept_language(auth_data->accept_language);
            m_websocket_client.set_accept_encoding(true, true, true, true);
            m_websocket_client.set_reconnect(true);
            m_websocket_client.set_request_timeout(20);
            m_websocket_client.set_proxy_server(auth_data->proxy_server);
            m_websocket_client.set_proxy_auth(auth_data->proxy_auth);
            m_websocket_client.set_proxy_type(auth_data->proxy_type);
        }
    }

    void BtcPriceManager::handle_event(const events::ConnectRequestEvent& event) {
        LOGIT_0TRACE();
        m_websocket_client.disconnect();
    }

    void BtcPriceManager::handle_event(const events::DisconnectRequestEvent& event) {
        LOGIT_0TRACE();
        m_websocket_client.disconnect();
    }

    void BtcPriceManager::handle_event(const events::AccountInfoUpdateEvent& event) {
        using Status = events::AccountInfoUpdateEvent::Status;
        if (event.status == Status::CONNECTED) {
            LOGIT_0TRACE();
            m_websocket_client.connect();
        } else
        if (event.status == Status::DISCONNECTED) {
            LOGIT_0TRACE();
            m_websocket_client.disconnect();
        }
    }

    void BtcPriceManager::handle_message(const std::string& message) {
        if (parse_btcusdt_tick(message, m_tick_data[0])) {
            notify_async(std::make_unique<events::PriceUpdateEvent>(m_tick_data));
        }
    }

    void BtcPriceManager::process() {

    }

    void BtcPriceManager::shutdown() {
        m_websocket_client.disconnect_and_wait();
        m_tick_data[0].tick.flags = 0;
        m_tick_data[0].flags = 0;
    }

} // namespace optionx::platforms::intrade_bar

#endif // _OPTIONX_PLATFORMS_INTRADERBAR_BTCUSDT_PRICE_MANAGER_HPP_INCLUDED
