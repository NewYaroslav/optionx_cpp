#pragma once
#ifndef OPTIONX_HEADER_BRIDGES_PROTOCOL_V1_BRIDGE_PROTOCOL_SERVER_CONFIG_HPP_INCLUDED
#define OPTIONX_HEADER_BRIDGES_PROTOCOL_V1_BRIDGE_PROTOCOL_SERVER_CONFIG_HPP_INCLUDED

/// \file BridgeProtocolServerConfig.hpp
/// \brief Defines configuration for Bridge Protocol v1 HTTP/WebSocket server.

namespace optionx::bridges::protocol_v1 {

    /// \class BridgeProtocolServerConfig
    /// \brief Configuration for serving Bridge Protocol v1 over HTTP and WebSocket.
    class BridgeProtocolServerConfig final : public IBridgeConfig {
    public:
        std::string address = "127.0.0.1"; ///< Interface address for both servers.
        std::uint16_t http_port = 0; ///< HTTP port; zero requests an ephemeral port.
        std::uint16_t websocket_port = 0; ///< WebSocket port; zero requests an ephemeral port.
        std::string command_path = "/api/v1/bridge/command"; ///< HTTP JSON-RPC command endpoint.
        std::string health_path = "/api/v1/bridge/health"; ///< HTTP health endpoint.
        std::string websocket_path = "/api/v1/bridge/ws"; ///< WebSocket JSON-RPC endpoint.
        std::string secret; ///< Shared secret accepted in `X-OptionX-Secret`.
        BridgeId bridge_id = 0; ///< Source bridge ID assigned to emitted signals.
        std::string installation_id = "optionx-local"; ///< Stable installation identifier.
        std::string server_instance_id = "optionx-bridge-server"; ///< Runtime server identifier.
        std::size_t request_body_limit = 1024 * 1024; ///< Maximum JSON-RPC body/message size.
        std::size_t dedupe_cache_size = 4096; ///< In-memory idempotency/result cache size.
        std::size_t max_request_id_aliases_per_operation = 16; ///< Retry request IDs stored per operation.
        std::size_t max_ws_pending_messages = 64; ///< Pending outbound WebSocket messages per client.
        std::size_t max_ws_pending_bytes = 1024 * 1024; ///< Pending outbound WebSocket bytes per client.
        long content_timeout_seconds = 30; ///< HTTP content read timeout.
        bool enable_http = true; ///< Start the HTTP command server.
        bool enable_websocket = true; ///< Start the WebSocket command server.
        bool allow_unauthenticated_local = false; ///< Allow no-secret loopback dev mode.
        bool allow_insecure_remote = false; ///< Allow plaintext remote bind with a shared secret.
        bool allow_cors = false; ///< Emit CORS headers for browser clients.
        std::string allowed_origin = "*"; ///< CORS `Access-Control-Allow-Origin` value.

        /// \brief Serializes configuration to JSON.
        void to_json(nlohmann::json& j) const override {
            j = nlohmann::json{
                {"address", address},
                {"http_port", http_port},
                {"websocket_port", websocket_port},
                {"command_path", command_path},
                {"health_path", health_path},
                {"websocket_path", websocket_path},
                {"secret", secret},
                {"bridge_id", bridge_id},
                {"installation_id", installation_id},
                {"server_instance_id", server_instance_id},
                {"request_body_limit", request_body_limit},
                {"dedupe_cache_size", dedupe_cache_size},
                {"max_request_id_aliases_per_operation", max_request_id_aliases_per_operation},
                {"max_ws_pending_messages", max_ws_pending_messages},
                {"max_ws_pending_bytes", max_ws_pending_bytes},
                {"content_timeout_seconds", content_timeout_seconds},
                {"enable_http", enable_http},
                {"enable_websocket", enable_websocket},
                {"allow_unauthenticated_local", allow_unauthenticated_local},
                {"allow_insecure_remote", allow_insecure_remote},
                {"allow_cors", allow_cors},
                {"allowed_origin", allowed_origin}
            };
        }

        /// \brief Loads configuration from JSON.
        void from_json(const nlohmann::json& j) override {
            if (j.contains("address")) address = j.at("address").get<std::string>();
            if (j.contains("http_port")) http_port = j.at("http_port").get<std::uint16_t>();
            if (j.contains("websocket_port")) {
                websocket_port = j.at("websocket_port").get<std::uint16_t>();
            }
            if (j.contains("command_path")) command_path = j.at("command_path").get<std::string>();
            if (j.contains("health_path")) health_path = j.at("health_path").get<std::string>();
            if (j.contains("websocket_path")) websocket_path = j.at("websocket_path").get<std::string>();
            if (j.contains("secret")) secret = j.at("secret").get<std::string>();
            if (j.contains("bridge_id")) bridge_id = j.at("bridge_id").get<BridgeId>();
            if (j.contains("installation_id")) {
                installation_id = j.at("installation_id").get<std::string>();
            }
            if (j.contains("server_instance_id")) {
                server_instance_id = j.at("server_instance_id").get<std::string>();
            }
            if (j.contains("request_body_limit")) {
                request_body_limit = j.at("request_body_limit").get<std::size_t>();
            }
            if (j.contains("dedupe_cache_size")) {
                dedupe_cache_size = j.at("dedupe_cache_size").get<std::size_t>();
            }
            if (j.contains("max_request_id_aliases_per_operation")) {
                max_request_id_aliases_per_operation =
                    j.at("max_request_id_aliases_per_operation").get<std::size_t>();
            }
            if (j.contains("max_ws_pending_messages")) {
                max_ws_pending_messages = j.at("max_ws_pending_messages").get<std::size_t>();
            }
            if (j.contains("max_ws_pending_bytes")) {
                max_ws_pending_bytes = j.at("max_ws_pending_bytes").get<std::size_t>();
            }
            if (j.contains("content_timeout_seconds")) {
                content_timeout_seconds = j.at("content_timeout_seconds").get<long>();
            }
            if (j.contains("enable_http")) enable_http = j.at("enable_http").get<bool>();
            if (j.contains("enable_websocket")) {
                enable_websocket = j.at("enable_websocket").get<bool>();
            }
            if (j.contains("allow_unauthenticated_local")) {
                allow_unauthenticated_local = j.at("allow_unauthenticated_local").get<bool>();
            }
            if (j.contains("allow_insecure_remote")) {
                allow_insecure_remote = j.at("allow_insecure_remote").get<bool>();
            }
            if (j.contains("allow_cors")) allow_cors = j.at("allow_cors").get<bool>();
            if (j.contains("allowed_origin")) {
                allowed_origin = j.at("allowed_origin").get<std::string>();
            }
        }

        /// \brief Creates a unique copy.
        std::unique_ptr<IBridgeConfig> clone_unique() const override {
            return std::make_unique<BridgeProtocolServerConfig>(*this);
        }

        /// \brief Creates a shared copy.
        std::shared_ptr<IBridgeConfig> clone_shared() const override {
            return std::make_shared<BridgeProtocolServerConfig>(*this);
        }

        /// \return `BridgeType::BRIDGE_PROTOCOL_V1_HTTP_WEBSOCKET`.
        BridgeType bridge_type() const override {
            return BridgeType::BRIDGE_PROTOCOL_V1_HTTP_WEBSOCKET;
        }

        /// \brief Validates configuration.
        std::pair<bool, std::string> validate() const override {
            if (address.empty()) {
                return {false, "Bridge Protocol v1 server address is empty."};
            }
            if (!enable_http && !enable_websocket) {
                return {false, "Bridge Protocol v1 server must enable HTTP, WebSocket, or both."};
            }
            if (enable_http && (command_path.empty() || command_path.front() != '/')) {
                return {false, "Bridge Protocol v1 HTTP command_path must start with '/'."};
            }
            if (enable_http && (health_path.empty() || health_path.front() != '/')) {
                return {false, "Bridge Protocol v1 HTTP health_path must start with '/'."};
            }
            if (enable_websocket && (websocket_path.empty() || websocket_path.front() != '/')) {
                return {false, "Bridge Protocol v1 websocket_path must start with '/'."};
            }
            if (request_body_limit == 0) {
                return {false, "Bridge Protocol v1 request_body_limit must be positive."};
            }
            if (dedupe_cache_size == 0) {
                return {false, "Bridge Protocol v1 dedupe_cache_size must be positive."};
            }
            if (max_request_id_aliases_per_operation == 0) {
                return {
                    false,
                    "Bridge Protocol v1 max_request_id_aliases_per_operation must be positive."
                };
            }
            if (max_ws_pending_messages == 0) {
                return {false, "Bridge Protocol v1 max_ws_pending_messages must be positive."};
            }
            if (max_ws_pending_bytes == 0) {
                return {false, "Bridge Protocol v1 max_ws_pending_bytes must be positive."};
            }
            if (content_timeout_seconds <= 0) {
                return {false, "Bridge Protocol v1 content_timeout_seconds must be positive."};
            }
            if (bridge_id == 0) {
                return {false, "Bridge Protocol v1 bridge_id is required."};
            }
            if (installation_id.empty()) {
                return {false, "Bridge Protocol v1 installation_id must not be empty."};
            }
            if (server_instance_id.empty()) {
                return {false, "Bridge Protocol v1 server_instance_id must not be empty."};
            }
            const auto is_loopback =
                address == "127.0.0.1" ||
                address == "localhost" ||
                address == "::1" ||
                address.rfind("127.", 0) == 0;
            if (secret.empty() && !allow_unauthenticated_local) {
                return {
                    false,
                    "Bridge Protocol v1 secret is required unless allow_unauthenticated_local is enabled."
                };
            }
            if (secret.empty() && allow_unauthenticated_local && !is_loopback) {
                return {
                    false,
                    "Bridge Protocol v1 unauthenticated mode is only allowed on loopback addresses."
                };
            }
            if (!secret.empty() && !allow_insecure_remote && !is_loopback) {
                return {
                    false,
                    "Bridge Protocol v1 remote plaintext bind requires allow_insecure_remote=true."
                };
            }
            if (allow_cors && allowed_origin.empty()) {
                return {false, "Bridge Protocol v1 allowed_origin must not be empty when CORS is enabled."};
            }
            if (secret.empty() && allow_cors && allowed_origin == "*") {
                return {
                    false,
                    "Bridge Protocol v1 wildcard CORS is not allowed without authentication."
                };
            }
            return {true, {}};
        }
    };

} // namespace optionx::bridges::protocol_v1

#endif // OPTIONX_HEADER_BRIDGES_PROTOCOL_V1_BRIDGE_PROTOCOL_SERVER_CONFIG_HPP_INCLUDED
