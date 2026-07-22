#pragma once
#ifndef OPTIONX_HEADER_BRIDGES_BOT_BINARY_BOT_BINARY_BRIDGE_CONFIG_HPP_INCLUDED
#define OPTIONX_HEADER_BRIDGES_BOT_BINARY_BOT_BINARY_BRIDGE_CONFIG_HPP_INCLUDED

/// \file BotBinaryBridgeConfig.hpp
/// \brief Defines configuration for the BotBinary/BinaryBot compatibility bridge.

namespace optionx::bridges::bot_binary {

    /// \class BotBinaryBridgeConfig
    /// \brief Configuration for receiving BotBinary/BinaryBot-compatible signals.
    class BotBinaryBridgeConfig final : public IBridgeConfig {
    public:
        /// \brief Serializes the bridge configuration.
        /// \param j Output JSON object.
        void to_json(nlohmann::json& j) const override {
            j = nlohmann::json{
                {"address", address},
                {"port", port},
                {"http_path", http_path},
                {"bridge_id", bridge_id},
                {"signal_name", signal_name},
                {"enable_http", enable_http},
                {"allow_insecure_remote", allow_insecure_remote},
                {"enable_file_signal", enable_file_signal},
                {"file_signal_dir", file_signal_dir},
                {"poll_interval_ms", poll_interval_ms},
                {"dedupe_cache_size", dedupe_cache_size},
                {"request_query_limit", request_query_limit},
                {"delete_processed_files", delete_processed_files},
                {"delete_invalid_files", delete_invalid_files},
                {"symbol_map", symbol_map}
            };
        }

        /// \brief Deserializes the bridge configuration.
        /// \param j Input JSON object.
        void from_json(const nlohmann::json& j) override {
            if (j.contains("address")) {
                address = j.at("address").get<std::string>();
            }
            if (j.contains("port")) {
                port = j.at("port").get<std::uint16_t>();
            }
            if (j.contains("http_path")) {
                http_path = j.at("http_path").get<std::string>();
            }
            if (j.contains("bridge_id")) {
                bridge_id = j.at("bridge_id").get<BridgeId>();
            }
            if (j.contains("signal_name")) {
                signal_name = j.at("signal_name").get<std::string>();
            }
            if (j.contains("enable_http")) {
                enable_http = j.at("enable_http").get<bool>();
            }
            if (j.contains("allow_insecure_remote")) {
                allow_insecure_remote = j.at("allow_insecure_remote").get<bool>();
            }
            if (j.contains("enable_file_signal")) {
                enable_file_signal = j.at("enable_file_signal").get<bool>();
            }
            if (j.contains("file_signal_dir")) {
                file_signal_dir = j.at("file_signal_dir").get<std::string>();
            } else if (j.contains("signal_path")) {
                file_signal_dir = j.at("signal_path").get<std::string>();
            }
            if (j.contains("poll_interval_ms")) {
                poll_interval_ms = j.at("poll_interval_ms").get<std::int64_t>();
            }
            if (j.contains("dedupe_cache_size")) {
                dedupe_cache_size = j.at("dedupe_cache_size").get<std::size_t>();
            }
            if (j.contains("request_query_limit")) {
                request_query_limit = j.at("request_query_limit").get<std::size_t>();
            }
            if (j.contains("delete_processed_files")) {
                delete_processed_files = j.at("delete_processed_files").get<bool>();
            }
            if (j.contains("delete_invalid_files")) {
                delete_invalid_files = j.at("delete_invalid_files").get<bool>();
            }
            if (j.contains("symbol_map")) {
                symbol_map = j.at("symbol_map").get<std::unordered_map<std::string, std::string>>();
            }
        }

        /// \brief Validates the bridge configuration.
        /// \return Pair with success flag and validation message.
        std::pair<bool, std::string> validate() const override {
            if (!enable_http && !enable_file_signal) {
                return {false, "BotBinary bridge must enable HTTP or file signal intake."};
            }
            if (enable_http && address.empty()) {
                return {false, "BotBinary bridge address is empty."};
            }
            if (enable_http &&
                !allow_insecure_remote &&
                !is_loopback_address(address)) {
                return {
                    false,
                    "BotBinary HTTP bridge refuses non-loopback address without allow_insecure_remote."
                };
            }
            if (enable_http && (http_path.empty() || http_path.front() != '/')) {
                return {false, "BotBinary bridge http_path must start with '/'."};
            }
            if (enable_file_signal && file_signal_dir.empty()) {
                return {false, "BotBinary bridge file_signal_dir is empty."};
            }
            if (bridge_id == 0) {
                return {false, "Bridge ID is required."};
            }
            if (signal_name.empty()) {
                return {false, "BotBinary bridge signal_name is empty."};
            }
            if (poll_interval_ms <= 0) {
                return {false, "BotBinary bridge poll_interval_ms must be positive."};
            }
            if (dedupe_cache_size == 0) {
                return {false, "BotBinary bridge dedupe_cache_size must be positive."};
            }
            if (request_query_limit == 0) {
                return {false, "BotBinary bridge request_query_limit must be positive."};
            }
            return {true, std::string()};
        }

        /// \brief Creates a unique pointer clone of this configuration.
        /// \return A unique pointer to a copied configuration.
        std::unique_ptr<IBridgeConfig> clone_unique() const override {
            return std::make_unique<BotBinaryBridgeConfig>(*this);
        }

        /// \brief Creates a shared pointer clone of this configuration.
        /// \return A shared pointer to a copied configuration.
        std::shared_ptr<IBridgeConfig> clone_shared() const override {
            return std::make_shared<BotBinaryBridgeConfig>(*this);
        }

        /// \brief Returns the bridge type.
        /// \return `BridgeType::BOT_BINARY`.
        BridgeType bridge_type() const override {
            return BridgeType::BOT_BINARY;
        }

        /// \brief Returns true for bind addresses scoped to the local machine.
        static bool is_loopback_address(std::string address_value) {
            std::transform(
                address_value.begin(),
                address_value.end(),
                address_value.begin(),
                [](const unsigned char ch) {
                    return static_cast<char>(std::tolower(ch));
                });
            if (address_value == "localhost" ||
                address_value == "::1" ||
                address_value == "[::1]") {
                return true;
            }
            return address_value.rfind("127.", 0) == 0;
        }

        /// \brief Returns BotBinary's documented default file-signal directory.
        static std::string default_file_signal_dir() {
            const auto common_root = optionx::utils::metatrader::default_common_files_root();
            if (common_root.empty()) {
                return {};
            }
            return (common_root / "Signal").u8string();
        }

        std::string address = "127.0.0.1"; ///< HTTP bind address.
        std::uint16_t port = 0;            ///< HTTP bind port; 0 allows an OS-assigned port.
        std::string http_path = "/";       ///< HTTP path that accepts BotBinary `request=` queries.
        BridgeId bridge_id = 0;            ///< Source bridge ID; must be non-zero.
        std::string signal_name = "bot_binary"; ///< Signal name assigned to parsed commands.

        bool enable_http = true;        ///< Accept BotBinary HTTP `request=` commands.
        bool allow_insecure_remote = false; ///< Permit unauthenticated non-loopback HTTP bind.
        bool enable_file_signal = true; ///< Poll BotBinary file-signal filenames.
        std::string file_signal_dir = default_file_signal_dir(); ///< Directory with BotBinary `.txt` signals.
        std::int64_t poll_interval_ms = 250; ///< File-signal polling interval.

        std::size_t dedupe_cache_size = 1024; ///< Recent transport keys retained to reject duplicates.
        std::size_t request_query_limit = 64 * 1024; ///< Maximum accepted HTTP query size.
        bool delete_processed_files = true; ///< Remove accepted file-signal files after dispatch.
        bool delete_invalid_files = false;  ///< Remove invalid file-signal files after reporting.
        std::unordered_map<std::string, std::string> symbol_map; ///< External-to-platform symbol map.
    };

} // namespace optionx::bridges::bot_binary

#endif // OPTIONX_HEADER_BRIDGES_BOT_BINARY_BOT_BINARY_BRIDGE_CONFIG_HPP_INCLUDED
