#pragma once
#ifndef _OPTIONX_PLATFORMS_INTRADERBAR_FX_PRICE_WEBSOCKET_MANAGER_HPP_INCLUDED
#define _OPTIONX_PLATFORMS_INTRADERBAR_FX_PRICE_WEBSOCKET_MANAGER_HPP_INCLUDED

/// \file FxPriceWebSocketManager.hpp
/// \brief Defines the Intrade Bar FX websocket tick source manager.

namespace optionx::platforms::intrade_bar {

    /// \class FxPriceWebSocketManager
    /// \brief Manages `/fxconnect` websocket tick streams for subscribed FX symbols.
    /// \details Intrade Bar accepts one FX symbol per websocket connection. This
    /// manager keeps a refcount per normalized symbol, opens the required
    /// connections, sends the slash-separated stream symbol after `WS_OPEN`, and
    /// publishes parsed ticks as `PriceUpdateEvent`.
    class FxPriceWebSocketManager final : public components::BaseComponent {
    public:
        /// \brief Constructs the FX websocket source manager.
        /// \param platform Owning platform facade.
        explicit FxPriceWebSocketManager(BaseTradingPlatform& platform)
                : BaseComponent(platform.event_bus()) {
            subscribe<events::AuthDataEvent>();
            subscribe<events::DisconnectRequestEvent>();
            subscribe<events::AccountInfoUpdateEvent>();
            subscribe<events::AutoDomainSelectedEvent>();
            platform.register_component(this);
        }

        /// \brief Destructor that closes all active websocket connections.
        ~FxPriceWebSocketManager() override {
            shutdown();
        }

        /// \brief Adds one consumer reference for an FX symbol websocket stream.
        /// \param symbol Public or broker symbol name.
        /// \return True if the symbol maps to an FX websocket stream.
        bool add_symbol_subscription(const std::string& symbol);

        /// \brief Removes one consumer reference for an FX symbol websocket stream.
        /// \param symbol Public or broker symbol name.
        void remove_symbol_subscription(const std::string& symbol);

        /// \brief Handles platform lifecycle/configuration events.
        void on_event(const utils::Event* const event) override;

        /// \brief Disconnects and removes all managed FX websocket streams.
        void shutdown() override;

    private:
        /// \brief Runtime state for one `/fxconnect` websocket.
        struct FxStreamState {
            std::string symbol;        ///< Normalized internal symbol, such as `EURUSD`.
            std::string stream_symbol; ///< Broker websocket symbol, such as `EUR/USD`.
            std::shared_ptr<kurlyk::WebSocketClient> client; ///< Websocket client.
            std::size_t ref_count = 0; ///< Number of active subscriptions using this stream.
            bool is_error = false;     ///< Suppresses repeated error logging.
        };

        std::string m_ws_host = "wss://intrade.bar"; ///< Websocket host.
        std::string m_user_agent =
            "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
            "(KHTML, like Gecko) Chrome/133.0.0.0 Safari/537.36"; ///< User-Agent header.
        std::string m_accept_language = "ru,ru-RU;q=0.9,en;q=0.8,en-US;q=0.7"; ///< Accept-Language header.
        std::string m_proxy_server; ///< Proxy address in <ip:port> format.
        std::string m_proxy_auth;   ///< Proxy authentication in <username:password> format.
        kurlyk::ProxyType m_proxy_type = kurlyk::ProxyType::PROXY_HTTP; ///< Proxy type.
        std::unordered_map<std::string, std::shared_ptr<FxStreamState>> m_streams; ///< Active FX streams.
        std::mutex m_mutex; ///< Protects stream state and websocket settings.

        /// \brief Configures a websocket client from current manager settings.
        void configure_client_no_lock(kurlyk::WebSocketClient& client) const;

        /// \brief Converts an HTTP(S) or WS(S) host setting to a websocket host.
        static std::string make_websocket_host(const std::string& host);

        /// \brief Creates a configured websocket stream state for a normalized FX symbol.
        std::shared_ptr<FxStreamState> create_stream_no_lock(
                const std::string& symbol,
                const std::string& stream_symbol);

        /// \brief Connects a stream if it has a websocket client.
        static void connect_stream(const std::shared_ptr<FxStreamState>& stream);

        /// \brief Disconnects a stream if it has a websocket client.
        static void disconnect_stream(const std::shared_ptr<FxStreamState>& stream);

        /// \brief Reconnects all active streams after a host/proxy/header change.
        void reconnect_all_streams();

        /// \brief Disconnects all active streams without changing subscription refcounts.
        void disconnect_all_streams();

        /// \brief Handles a websocket event for one FX stream.
        void handle_websocket_event(
                const std::shared_ptr<FxStreamState>& stream,
                const kurlyk::WebSocketEventData& event);

        /// \brief Parses and publishes a websocket message for one FX stream.
        void handle_message(
                const std::shared_ptr<FxStreamState>& stream,
                const std::string& message);

        /// \brief Applies Intrade auth/config data to websocket settings.
        void handle_event(const events::AuthDataEvent& event);

        /// \brief Handles a platform disconnect request.
        void handle_event(const events::DisconnectRequestEvent& event);

        /// \brief Handles account connection-state updates.
        void handle_event(const events::AccountInfoUpdateEvent& event);

        /// \brief Applies the selected auto-domain host to websocket settings.
        void handle_event(const events::AutoDomainSelectedEvent& event);
    };

    inline bool FxPriceWebSocketManager::add_symbol_subscription(
            const std::string& symbol) {
        const auto normalized = normalize_symbol_name(symbol);
        const auto stream_symbol = make_fxconnect_symbol(normalized);
        if (stream_symbol.empty()) return false;

        std::shared_ptr<FxStreamState> stream;
        bool should_connect = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_streams.find(normalized);
            if (it == m_streams.end()) {
                stream = create_stream_no_lock(normalized, stream_symbol);
                stream->ref_count = 1;
                m_streams.emplace(normalized, stream);
                should_connect = true;
            } else {
                stream = it->second;
                ++stream->ref_count;
            }
        }

        if (should_connect) {
            connect_stream(stream);
        }
        return true;
    }

    inline void FxPriceWebSocketManager::remove_symbol_subscription(
            const std::string& symbol) {
        const auto normalized = normalize_symbol_name(symbol);
        std::shared_ptr<FxStreamState> stream_to_disconnect;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_streams.find(normalized);
            if (it == m_streams.end()) return;

            if (it->second->ref_count > 1) {
                --it->second->ref_count;
                return;
            }

            stream_to_disconnect = it->second;
            m_streams.erase(it);
        }

        disconnect_stream(stream_to_disconnect);
    }

    inline void FxPriceWebSocketManager::on_event(const utils::Event* const event) {
        if (const auto* msg = dynamic_cast<const events::AuthDataEvent*>(event)) {
            handle_event(*msg);
        } else
        if (const auto* msg = dynamic_cast<const events::DisconnectRequestEvent*>(event)) {
            handle_event(*msg);
        } else
        if (const auto* msg = dynamic_cast<const events::AccountInfoUpdateEvent*>(event)) {
            handle_event(*msg);
        } else
        if (const auto* msg = dynamic_cast<const events::AutoDomainSelectedEvent*>(event)) {
            handle_event(*msg);
        }
    }

    inline void FxPriceWebSocketManager::shutdown() {
        std::vector<std::shared_ptr<FxStreamState>> streams;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            streams.reserve(m_streams.size());
            for (auto& [symbol, stream] : m_streams) {
                (void)symbol;
                streams.push_back(std::move(stream));
            }
            m_streams.clear();
        }

        for (auto& stream : streams) {
            disconnect_stream(stream);
        }
    }

    inline void FxPriceWebSocketManager::configure_client_no_lock(
            kurlyk::WebSocketClient& client) const {
        client.set_url(m_ws_host, "/fxconnect");
        client.set_user_agent(m_user_agent);
        client.set_accept_language(m_accept_language);
        client.set_accept_encoding(true, true, true, true);
        client.set_reconnect(true);
        client.set_request_timeout(20);
        client.set_proxy_server(m_proxy_server);
        client.set_proxy_auth(m_proxy_auth);
        client.set_proxy_type(m_proxy_type);
    }

    inline std::string FxPriceWebSocketManager::make_websocket_host(
            const std::string& host) {
        const std::string wss_prefix = "wss://";
        const std::string ws_prefix = "ws://";
        const std::string https_prefix = "https://";
        const std::string http_prefix = "http://";

        if (host.rfind(wss_prefix, 0) == 0 || host.rfind(ws_prefix, 0) == 0) {
            return host;
        }
        if (host.rfind(https_prefix, 0) == 0) {
            return wss_prefix + host.substr(https_prefix.size());
        }
        if (host.rfind(http_prefix, 0) == 0) {
            return ws_prefix + host.substr(http_prefix.size());
        }
        return wss_prefix + host;
    }

    inline std::shared_ptr<FxPriceWebSocketManager::FxStreamState>
    FxPriceWebSocketManager::create_stream_no_lock(
            const std::string& symbol,
            const std::string& stream_symbol) {
        auto stream = std::make_shared<FxStreamState>();
        stream->symbol = symbol;
        stream->stream_symbol = stream_symbol;
        stream->client = std::make_shared<kurlyk::WebSocketClient>();
        configure_client_no_lock(*stream->client);

        std::weak_ptr<FxStreamState> weak_stream = stream;
        stream->client->on_event(
            [this, weak_stream](std::unique_ptr<kurlyk::WebSocketEventData> event) {
                auto locked = weak_stream.lock();
                if (!locked || !event) return;
                handle_websocket_event(locked, *event);
            });
        return stream;
    }

    inline void FxPriceWebSocketManager::connect_stream(
            const std::shared_ptr<FxStreamState>& stream) {
        if (stream && stream->client && !stream->client->is_connected()) {
            stream->client->connect();
        }
    }

    inline void FxPriceWebSocketManager::disconnect_stream(
            const std::shared_ptr<FxStreamState>& stream) {
        if (stream && stream->client) {
            stream->client->disconnect_and_wait();
        }
    }

    inline void FxPriceWebSocketManager::reconnect_all_streams() {
        std::vector<std::shared_ptr<FxStreamState>> streams;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            streams.reserve(m_streams.size());
            for (auto& [symbol, stream] : m_streams) {
                (void)symbol;
                configure_client_no_lock(*stream->client);
                streams.push_back(stream);
            }
        }

        for (auto& stream : streams) {
            disconnect_stream(stream);
            connect_stream(stream);
        }
    }

    inline void FxPriceWebSocketManager::handle_websocket_event(
            const std::shared_ptr<FxStreamState>& stream,
            const kurlyk::WebSocketEventData& event) {
        switch (event.event_type) {
        case kurlyk::WebSocketEventType::WS_OPEN:
            stream->is_error = false;
            if (event.sender) {
                const auto result = event.sender->submit_message(stream->stream_symbol);
                if (!result.accepted) {
                    LOGIT_WARN("Intrade Bar FX websocket: failed to submit stream symbol ",
                               stream->stream_symbol,
                               ": ",
                               result.error_code.message());
                }
            }
            break;
        case kurlyk::WebSocketEventType::WS_MESSAGE:
            handle_message(stream, event.message);
            break;
        case kurlyk::WebSocketEventType::WS_CLOSE:
            stream->is_error = false;
            break;
        case kurlyk::WebSocketEventType::WS_ERROR:
            if (!stream->is_error) {
                LOGIT_ERROR("Intrade Bar FX websocket error for ",
                            stream->stream_symbol,
                            ": ",
                            event.error_code.message());
                stream->is_error = true;
            }
            break;
        default:
            break;
        }
    }

    inline void FxPriceWebSocketManager::handle_message(
            const std::shared_ptr<FxStreamState>& stream,
            const std::string& message) {
        try {
            SingleTick tick;
            if (!parse_fxconnect_tick(message, tick)) return;
            if (tick.symbol != stream->symbol) return;

            std::vector<SingleTick> ticks;
            ticks.push_back(std::move(tick));
            notify_async(std::make_unique<events::PriceUpdateEvent>(std::move(ticks)));
        } catch (const std::exception& ex) {
            LOGIT_WARN("Intrade Bar FX websocket: failed to parse tick for ",
                       stream ? stream->stream_symbol : std::string(),
                       ": ",
                       ex.what());
        }
    }

    inline void FxPriceWebSocketManager::handle_event(
            const events::AuthDataEvent& event) {
        auto auth_data = std::dynamic_pointer_cast<AuthData>(event.auth_data);
        if (!auth_data) return;

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!auth_data->auto_find_domain && !auth_data->host.empty()) {
                m_ws_host = make_websocket_host(auth_data->host);
            }
            m_user_agent = auth_data->user_agent;
            m_accept_language = auth_data->accept_language;
            m_proxy_server = auth_data->proxy_server;
            m_proxy_auth = auth_data->proxy_auth;
            m_proxy_type = auth_data->proxy_type;
        }

        reconnect_all_streams();
    }

    inline void FxPriceWebSocketManager::handle_event(
            const events::DisconnectRequestEvent& event) {
        (void)event;
        disconnect_all_streams();
    }

    inline void FxPriceWebSocketManager::handle_event(
            const events::AccountInfoUpdateEvent& event) {
        using Status = events::AccountInfoUpdateEvent::Status;
        if (event.status == Status::CONNECTED) {
            reconnect_all_streams();
        } else if (event.status == Status::DISCONNECTED) {
            disconnect_all_streams();
        }
    }

    inline void FxPriceWebSocketManager::handle_event(
            const events::AutoDomainSelectedEvent& event) {
        if (!event.success || event.selected_host.empty()) return;

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_ws_host = make_websocket_host(event.selected_host);
        }

        reconnect_all_streams();
    }

    inline void FxPriceWebSocketManager::disconnect_all_streams() {
        std::vector<std::shared_ptr<FxStreamState>> streams;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            streams.reserve(m_streams.size());
            for (auto& [symbol, stream] : m_streams) {
                (void)symbol;
                streams.push_back(stream);
            }
        }

        for (auto& stream : streams) {
            disconnect_stream(stream);
        }
    }

} // namespace optionx::platforms::intrade_bar

#endif // _OPTIONX_PLATFORMS_INTRADERBAR_FX_PRICE_WEBSOCKET_MANAGER_HPP_INCLUDED
