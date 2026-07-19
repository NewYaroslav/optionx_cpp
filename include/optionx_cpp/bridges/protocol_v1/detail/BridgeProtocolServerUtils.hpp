#pragma once
#ifndef OPTIONX_HEADER_BRIDGES_PROTOCOL_V1_DETAIL_BRIDGE_PROTOCOL_SERVER_UTILS_HPP_INCLUDED
#define OPTIONX_HEADER_BRIDGES_PROTOCOL_V1_DETAIL_BRIDGE_PROTOCOL_SERVER_UTILS_HPP_INCLUDED

/// \file BridgeProtocolServerUtils.hpp
/// \brief Helper functions for Bridge Protocol v1 JSON-RPC transports.

namespace optionx::bridges::protocol_v1::detail {

    inline constexpr int jsonrpc_parse_error = -32700;
    inline constexpr int jsonrpc_invalid_request = -32600;
    inline constexpr int jsonrpc_method_not_found = -32601;
    inline constexpr int jsonrpc_invalid_params = -32602;
    inline constexpr int jsonrpc_internal_error = -32603;
    inline constexpr int jsonrpc_authorization_failed = -32001;

    inline nlohmann::json jsonrpc_result(nlohmann::json id, nlohmann::json result) {
        return nlohmann::json{
            {"jsonrpc", "2.0"},
            {"id", std::move(id)},
            {"result", std::move(result)}
        };
    }

    inline nlohmann::json jsonrpc_error(
            nlohmann::json id,
            const int code,
            std::string message,
            nlohmann::json data = nlohmann::json::object()) {
        return nlohmann::json{
            {"jsonrpc", "2.0"},
            {"id", std::move(id)},
            {"error", {
                {"code", code},
                {"message", std::move(message)},
                {"data", std::move(data)}
            }}
        };
    }

    inline std::string json_id_to_key(const nlohmann::json& id) {
        if (id.is_null()) {
            return {};
        }
        if (id.is_string()) {
            return id.get<std::string>();
        }
        return id.dump(-1);
    }

    inline std::string header_value(
            const SimpleWeb::CaseInsensitiveMultimap& headers,
            const std::string& name) {
        const auto range = headers.equal_range(name);
        return range.first == range.second ? std::string() : range.first->second;
    }

    inline bool authorized(
            const BridgeProtocolServerConfig& config,
            const SimpleWeb::CaseInsensitiveMultimap& headers) {
        if (config.secret.empty()) {
            return true;
        }
        const auto secret = header_value(headers, "X-OptionX-Secret");
        if (secret == config.secret) {
            return true;
        }
        const auto authorization = header_value(headers, "Authorization");
        const std::string prefix = "Bearer ";
        return authorization.size() > prefix.size() &&
            authorization.compare(0, prefix.size(), prefix) == 0 &&
            authorization.substr(prefix.size()) == config.secret;
    }

    inline SimpleWeb::CaseInsensitiveMultimap json_headers(
            const BridgeProtocolServerConfig& config) {
        SimpleWeb::CaseInsensitiveMultimap headers;
        headers.emplace("Content-Type", "application/json");
        headers.emplace("Connection", "close");
        if (config.allow_cors) {
            headers.emplace("Access-Control-Allow-Origin", config.allowed_origin);
            headers.emplace("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
            headers.emplace(
                "Access-Control-Allow-Headers",
                "Content-Type, X-OptionX-Secret, Authorization");
        }
        return headers;
    }

    inline nlohmann::json capabilities_snapshot() {
        return nlohmann::json{
            {"protocol_versions", nlohmann::json::array({"1"})},
            {"supported_methods", nlohmann::json::array({
                "protocol.hello",
                "protocol.capabilities.get",
                "account.balance.get",
                "signal.submit",
                "trade.open"
            })},
            {"features", {
                {"http", true},
                {"websocket", true},
                {"subscriptions", false},
                {"event_replay", false},
                {"trade_open_batch", false}
            }},
            {"limits", {
                {"event_retention_ms", 0},
                {"max_replay_events", 0}
            }}
        };
    }

} // namespace optionx::bridges::protocol_v1::detail

#endif // OPTIONX_HEADER_BRIDGES_PROTOCOL_V1_DETAIL_BRIDGE_PROTOCOL_SERVER_UTILS_HPP_INCLUDED
