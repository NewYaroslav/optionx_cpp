#pragma once
#ifndef OPTIONX_HEADER_BRIDGES_METATRADER_FILE_META_TRADER_FILE_BRIDGE_CONFIG_HPP_INCLUDED
#define OPTIONX_HEADER_BRIDGES_METATRADER_FILE_META_TRADER_FILE_BRIDGE_CONFIG_HPP_INCLUDED

/// \file MetaTraderFileBridgeConfig.hpp
/// \brief Defines configuration for the MetaTrader common-files bridge transport.

namespace optionx::bridges::metatrader_file {

    /// \class MetaTraderFileBridgeConfig
    /// \brief Configuration for the OptionX MetaTrader NDJSON file transport.
    class MetaTraderFileBridgeConfig final : public IBridgeConfig {
    public:
        /// \brief Serializes the bridge configuration.
        /// \param j Output JSON object.
        void to_json(nlohmann::json& j) const override {
            j = nlohmann::json{
                {"common_files_root", common_files_root},
                {"namespace_subdir", namespace_subdir},
                {"bridge_id", bridge_id},
                {"client_id", client_id},
                {"client_secret", client_secret},
                {"poll_interval_ms", poll_interval_ms},
                {"max_line_bytes", max_line_bytes},
                {"enable_events", enable_events},
                {"enable_state_snapshot", enable_state_snapshot}
            };
        }

        /// \brief Deserializes the bridge configuration.
        /// \param j Input JSON object.
        void from_json(const nlohmann::json& j) override {
            if (j.contains("common_files_root")) {
                common_files_root = j.at("common_files_root").get<std::string>();
            }
            if (j.contains("namespace_subdir")) {
                namespace_subdir = j.at("namespace_subdir").get<std::string>();
            }
            if (j.contains("bridge_id")) {
                bridge_id = j.at("bridge_id").get<BridgeId>();
            }
            if (j.contains("client_id")) {
                client_id = j.at("client_id").get<std::string>();
            }
            if (j.contains("client_secret")) {
                client_secret = j.at("client_secret").get<std::string>();
            }
            if (j.contains("poll_interval_ms")) {
                poll_interval_ms = j.at("poll_interval_ms").get<std::int64_t>();
            }
            if (j.contains("max_line_bytes")) {
                max_line_bytes = j.at("max_line_bytes").get<std::size_t>();
            }
            if (j.contains("enable_events")) {
                enable_events = j.at("enable_events").get<bool>();
            }
            if (j.contains("enable_state_snapshot")) {
                enable_state_snapshot = j.at("enable_state_snapshot").get<bool>();
            }
        }

        /// \brief Validates the bridge configuration.
        /// \return Pair with success flag and validation message.
        std::pair<bool, std::string> validate() const override {
            if (common_files_root.empty()) {
                return {false, "MetaTrader file bridge common_files_root is empty."};
            }
            if (namespace_subdir.empty()) {
                return {false, "MetaTrader file bridge namespace_subdir is empty."};
            }
            if (!is_safe_namespace_subdir(namespace_subdir)) {
                return {
                    false,
                    "MetaTrader file bridge namespace_subdir must be a safe relative path."
                };
            }
            if (bridge_id == 0) {
                return {false, "MetaTrader file bridge bridge_id is required."};
            }
            if (!is_safe_file_transport_id(client_id)) {
                return {
                    false,
                    "MetaTrader file bridge client_id must be a safe [A-Za-z0-9.-]+ identifier."
                };
            }
            if (poll_interval_ms <= 0) {
                return {false, "MetaTrader file bridge poll_interval_ms must be positive."};
            }
            if (max_line_bytes == 0) {
                return {false, "MetaTrader file bridge max_line_bytes must be positive."};
            }
            const auto root = std::filesystem::u8path(common_files_root);
            if (!path_is_within_or_equal(root, client_root())) {
                return {
                    false,
                    "MetaTrader file bridge client_root must stay inside common_files_root."
                };
            }
            return {true, std::string()};
        }

        /// \brief Creates a unique pointer clone of this configuration.
        /// \return A unique pointer to a copied configuration.
        std::unique_ptr<IBridgeConfig> clone_unique() const override {
            return std::make_unique<MetaTraderFileBridgeConfig>(*this);
        }

        /// \brief Creates a shared pointer clone of this configuration.
        /// \return A shared pointer to a copied configuration.
        std::shared_ptr<IBridgeConfig> clone_shared() const override {
            return std::make_shared<MetaTraderFileBridgeConfig>(*this);
        }

        /// \brief Returns the bridge type.
        /// \return `BridgeType::METATRADER_FILE_TRANSPORT`.
        BridgeType bridge_type() const override {
            return BridgeType::METATRADER_FILE_TRANSPORT;
        }

        /// \brief Returns the configured client root directory.
        /// \return Common-files root plus OptionX namespace, bridge ID and client ID.
        std::filesystem::path client_root() const {
            return std::filesystem::u8path(common_files_root) /
                   std::filesystem::u8path(namespace_subdir) /
                   std::to_string(bridge_id) /
                   client_id;
        }

        std::string common_files_root = default_common_files_root(); ///< MetaQuotes Common\Files root.
        std::string namespace_subdir = "OptionX/Bridge/v1"; ///< Relative OptionX file namespace.
        BridgeId bridge_id = 0; ///< Source bridge ID; must be non-zero.
        std::string client_id = "default"; ///< Path-safe client directory ID.
        std::string client_secret; ///< Optional directory-level shared secret metadata.
        std::int64_t poll_interval_ms = 250; ///< Recommended bridge polling interval.
        std::size_t max_line_bytes = 64 * 1024; ///< Maximum complete NDJSON line size.
        bool enable_events = true; ///< Append bridge events to events.ndjson.
        bool enable_state_snapshot = true; ///< Atomically publish state.json snapshots.
    };

} // namespace optionx::bridges::metatrader_file

#endif // OPTIONX_HEADER_BRIDGES_METATRADER_FILE_META_TRADER_FILE_BRIDGE_CONFIG_HPP_INCLUDED
