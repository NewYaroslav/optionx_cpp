#pragma once
#ifndef OPTIONX_HEADER_BRIDGES_PROTOCOL_V1_BRIDGE_PROTOCOL_NAMED_PIPE_CONFIG_HPP_INCLUDED
#define OPTIONX_HEADER_BRIDGES_PROTOCOL_V1_BRIDGE_PROTOCOL_NAMED_PIPE_CONFIG_HPP_INCLUDED

/// \file BridgeProtocolNamedPipeConfig.hpp
/// \brief Defines configuration for Bridge Protocol v1 over a named pipe.

namespace optionx::bridges::protocol_v1 {

    /// \class BridgeProtocolNamedPipeConfig
    /// \brief Configuration for serving Bridge Protocol v1 over local named pipes.
    class BridgeProtocolNamedPipeConfig final : public IBridgeConfig {
    public:
        std::string named_pipe = "optionx_bridge_protocol_v1"; ///< Named pipe endpoint.
        BridgeId bridge_id = 0; ///< Source bridge ID assigned to emitted signals.
        std::string installation_id = "optionx-local"; ///< Stable installation identifier.
        std::string server_instance_id = "optionx-bridge-named-pipe"; ///< Runtime server identifier.
        std::size_t buffer_size = 64 * 1024; ///< Named-pipe read/write buffer size.
        std::size_t pipe_timeout_ms = 50; ///< Named-pipe internal wait timeout.
        std::size_t request_body_limit = 1024 * 1024; ///< Maximum JSON-RPC message size.
        std::size_t dedupe_cache_size = 4096; ///< In-memory idempotency/result cache size.
        std::size_t max_jsonrpc_id_bytes = 256; ///< Maximum serialized JSON-RPC `id` size.
        std::size_t max_idempotency_key_bytes = 512; ///< Maximum logical idempotency key size.
        std::size_t max_operation_fingerprint_bytes = 64 * 1024; ///< Maximum canonical payload bytes.
        std::size_t max_operation_cache_bytes = 4 * 1024 * 1024; ///< Maximum result cache bytes.
        std::int64_t operation_cache_retention_ms = 15 * 60 * 1000; ///< Completed result TTL.

        /// \brief Serializes configuration to JSON.
        void to_json(nlohmann::json& j) const override {
            j = nlohmann::json{
                {"named_pipe", named_pipe},
                {"bridge_id", bridge_id},
                {"installation_id", installation_id},
                {"server_instance_id", server_instance_id},
                {"buffer_size", buffer_size},
                {"pipe_timeout_ms", pipe_timeout_ms},
                {"request_body_limit", request_body_limit},
                {"dedupe_cache_size", dedupe_cache_size},
                {"max_jsonrpc_id_bytes", max_jsonrpc_id_bytes},
                {"max_idempotency_key_bytes", max_idempotency_key_bytes},
                {"max_operation_fingerprint_bytes", max_operation_fingerprint_bytes},
                {"max_operation_cache_bytes", max_operation_cache_bytes},
                {"operation_cache_retention_ms", operation_cache_retention_ms}
            };
        }

        /// \brief Loads configuration from JSON.
        void from_json(const nlohmann::json& j) override {
            if (j.contains("named_pipe")) named_pipe = j.at("named_pipe").get<std::string>();
            if (j.contains("bridge_id")) bridge_id = j.at("bridge_id").get<BridgeId>();
            if (j.contains("installation_id")) {
                installation_id = j.at("installation_id").get<std::string>();
            }
            if (j.contains("server_instance_id")) {
                server_instance_id = j.at("server_instance_id").get<std::string>();
            }
            if (j.contains("buffer_size")) buffer_size = j.at("buffer_size").get<std::size_t>();
            if (j.contains("pipe_timeout_ms")) {
                pipe_timeout_ms = j.at("pipe_timeout_ms").get<std::size_t>();
            }
            if (j.contains("request_body_limit")) {
                request_body_limit = j.at("request_body_limit").get<std::size_t>();
            }
            if (j.contains("dedupe_cache_size")) {
                dedupe_cache_size = j.at("dedupe_cache_size").get<std::size_t>();
            }
            if (j.contains("max_jsonrpc_id_bytes")) {
                max_jsonrpc_id_bytes = j.at("max_jsonrpc_id_bytes").get<std::size_t>();
            }
            if (j.contains("max_idempotency_key_bytes")) {
                max_idempotency_key_bytes =
                    j.at("max_idempotency_key_bytes").get<std::size_t>();
            }
            if (j.contains("max_operation_fingerprint_bytes")) {
                max_operation_fingerprint_bytes =
                    j.at("max_operation_fingerprint_bytes").get<std::size_t>();
            }
            if (j.contains("max_operation_cache_bytes")) {
                max_operation_cache_bytes =
                    j.at("max_operation_cache_bytes").get<std::size_t>();
            }
            if (j.contains("operation_cache_retention_ms")) {
                operation_cache_retention_ms =
                    j.at("operation_cache_retention_ms").get<std::int64_t>();
            }
        }

        /// \brief Creates a unique copy.
        std::unique_ptr<IBridgeConfig> clone_unique() const override {
            return std::make_unique<BridgeProtocolNamedPipeConfig>(*this);
        }

        /// \brief Creates a shared copy.
        std::shared_ptr<IBridgeConfig> clone_shared() const override {
            return std::make_shared<BridgeProtocolNamedPipeConfig>(*this);
        }

        /// \return `BridgeType::BRIDGE_PROTOCOL_V1_NAMED_PIPE`.
        BridgeType bridge_type() const override {
            return BridgeType::BRIDGE_PROTOCOL_V1_NAMED_PIPE;
        }

        /// \brief Validates configuration.
        std::pair<bool, std::string> validate() const override {
            if (named_pipe.empty()) {
                return {false, "Bridge Protocol v1 named_pipe must not be empty."};
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
            if (buffer_size == 0) {
                return {false, "Bridge Protocol v1 buffer_size must be positive."};
            }
            if (pipe_timeout_ms == 0) {
                return {false, "Bridge Protocol v1 pipe_timeout_ms must be positive."};
            }
            if (request_body_limit == 0) {
                return {false, "Bridge Protocol v1 request_body_limit must be positive."};
            }
            if (dedupe_cache_size == 0) {
                return {false, "Bridge Protocol v1 dedupe_cache_size must be positive."};
            }
            if (max_jsonrpc_id_bytes == 0) {
                return {false, "Bridge Protocol v1 max_jsonrpc_id_bytes must be positive."};
            }
            if (max_idempotency_key_bytes == 0) {
                return {false, "Bridge Protocol v1 max_idempotency_key_bytes must be positive."};
            }
            if (max_operation_fingerprint_bytes == 0) {
                return {
                    false,
                    "Bridge Protocol v1 max_operation_fingerprint_bytes must be positive."
                };
            }
            if (max_operation_cache_bytes == 0) {
                return {false, "Bridge Protocol v1 max_operation_cache_bytes must be positive."};
            }
            if (operation_cache_retention_ms <= 0) {
                return {false, "Bridge Protocol v1 operation_cache_retention_ms must be positive."};
            }
            return {true, {}};
        }
    };

} // namespace optionx::bridges::protocol_v1

#endif // OPTIONX_HEADER_BRIDGES_PROTOCOL_V1_BRIDGE_PROTOCOL_NAMED_PIPE_CONFIG_HPP_INCLUDED
