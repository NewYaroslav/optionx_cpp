#pragma once
#ifndef OPTIONX_HEADER_BRIDGES_METATRADER_FILE_META_TRADER_FILE_BRIDGE_CONFIG_HPP_INCLUDED
#define OPTIONX_HEADER_BRIDGES_METATRADER_FILE_META_TRADER_FILE_BRIDGE_CONFIG_HPP_INCLUDED

/// \file MetaTraderFileBridgeConfig.hpp
/// \brief Defines configuration for the MetaTrader common-files bridge transport.

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>

namespace optionx::bridges::metatrader_file {

    /// \brief Returns the default MetaQuotes common files directory on Windows.
    /// \return `%APPDATA%\MetaQuotes\Terminal\Common\Files` when APPDATA is set; otherwise empty.
    inline std::string default_common_files_root() {
#if defined(_WIN32)
        const char* appdata = std::getenv("APPDATA");
        if (!appdata || !*appdata) {
            return {};
        }
        return (std::filesystem::u8path(appdata) /
                "MetaQuotes" /
                "Terminal" /
                "Common" /
                "Files").u8string();
#else
        return {};
#endif
    }

    /// \brief Returns true when a path segment is safe for protocol filenames.
    /// \details The file transport uses `_` as a separator, so stable IDs must
    /// not contain it. Conservative IDs keep parsing portable across MQL and C++.
    inline bool is_safe_file_transport_id(const std::string& value) noexcept {
        if (value.empty()) return false;
        for (const unsigned char ch : value) {
            const bool ok =
                (ch >= 'A' && ch <= 'Z') ||
                (ch >= 'a' && ch <= 'z') ||
                (ch >= '0' && ch <= '9') ||
                ch == '-' ||
                ch == '.';
            if (!ok) return false;
        }
        return true;
    }

    /// \class MetaTraderFileBridgeConfig
    /// \brief Configuration for the OptionX JSON-RPC file-drop transport.
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
                {"processing_lease_ms", processing_lease_ms},
                {"retention_ms", retention_ms},
                {"request_body_limit", request_body_limit},
                {"max_ready_files", max_ready_files},
                {"archive_processed_requests", archive_processed_requests},
                {"enable_responses", enable_responses},
                {"enable_events", enable_events}
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
            if (j.contains("processing_lease_ms")) {
                processing_lease_ms = j.at("processing_lease_ms").get<std::int64_t>();
            }
            if (j.contains("retention_ms")) {
                retention_ms = j.at("retention_ms").get<std::int64_t>();
            }
            if (j.contains("request_body_limit")) {
                request_body_limit = j.at("request_body_limit").get<std::size_t>();
            }
            if (j.contains("max_ready_files")) {
                max_ready_files = j.at("max_ready_files").get<std::size_t>();
            }
            if (j.contains("archive_processed_requests")) {
                archive_processed_requests = j.at("archive_processed_requests").get<bool>();
            }
            if (j.contains("enable_responses")) {
                enable_responses = j.at("enable_responses").get<bool>();
            }
            if (j.contains("enable_events")) {
                enable_events = j.at("enable_events").get<bool>();
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
            if (std::filesystem::u8path(namespace_subdir).is_absolute()) {
                return {false, "MetaTrader file bridge namespace_subdir must be relative."};
            }
            if (bridge_id == 0) {
                return {false, "MetaTrader file bridge bridge_id is required."};
            }
            if (!is_safe_file_transport_id(client_id)) {
                return {false, "MetaTrader file bridge client_id must match [A-Za-z0-9.-]+."};
            }
            if (poll_interval_ms <= 0) {
                return {false, "MetaTrader file bridge poll_interval_ms must be positive."};
            }
            if (processing_lease_ms <= 0) {
                return {false, "MetaTrader file bridge processing_lease_ms must be positive."};
            }
            if (retention_ms < 0) {
                return {false, "MetaTrader file bridge retention_ms must not be negative."};
            }
            if (request_body_limit == 0) {
                return {false, "MetaTrader file bridge request_body_limit must be positive."};
            }
            if (max_ready_files == 0) {
                return {false, "MetaTrader file bridge max_ready_files must be positive."};
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
        std::int64_t processing_lease_ms = 30000; ///< Time after which processing files may be recovered.
        std::int64_t retention_ms = 24LL * 60LL * 60LL * 1000LL; ///< Archive/error retention window.
        std::size_t request_body_limit = 64 * 1024; ///< Maximum JSON request file size.
        std::size_t max_ready_files = 1024; ///< Backpressure limit for ready directories.
        bool archive_processed_requests = true; ///< Retain processed request files when possible.
        bool enable_responses = true; ///< Write JSON-RPC responses for request files.
        bool enable_events = true; ///< Write event notification files for subscribed clients.
    };

} // namespace optionx::bridges::metatrader_file

#endif // OPTIONX_HEADER_BRIDGES_METATRADER_FILE_META_TRADER_FILE_BRIDGE_CONFIG_HPP_INCLUDED
