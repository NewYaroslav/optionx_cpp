#pragma once
#ifndef OPTIONX_HEADER_BRIDGES_LEGACY_TRADING_LEGACY_TRADING_BRIDGE_CONFIG_HPP_INCLUDED
#define OPTIONX_HEADER_BRIDGES_LEGACY_TRADING_LEGACY_TRADING_BRIDGE_CONFIG_HPP_INCLUDED

/// \file LegacyTradingBridgeConfig.hpp
/// \brief Defines configuration for the legacy named-pipe trading bridge.

namespace optionx::bridges::legacy_trading {

    /// \class LegacyTradingBridgeConfig
    /// \brief Configuration for the legacy named-pipe trading bridge protocol.
    class LegacyTradingBridgeConfig final : public IBridgeConfig {
    public:
        /// \brief Serializes the bridge configuration.
        /// \param j Output JSON object.
        void to_json(nlohmann::json& j) const override {
            j = nlohmann::json{
                {"named_pipe", named_pipe},
                {"bridge_id", bridge_id},
                {"min_payout", min_payout},
                {"buffer_size", buffer_size},
                {"pipe_timeout_ms", pipe_timeout_ms},
                {"ping_period_ms", ping_period_ms}
            };
        }

        /// \brief Deserializes the bridge configuration.
        /// \param j Input JSON object.
        void from_json(const nlohmann::json& j) override {
            if (j.contains("named_pipe")) {
                named_pipe = j.at("named_pipe").get<std::string>();
            }
            if (j.contains("bridge_id")) {
                bridge_id = j.at("bridge_id").get<BridgeId>();
            }
            if (j.contains("min_payout")) {
                min_payout = j.at("min_payout").get<double>();
            }
            if (j.contains("buffer_size")) {
                buffer_size = j.at("buffer_size").get<std::size_t>();
            }
            if (j.contains("pipe_timeout_ms")) {
                pipe_timeout_ms = j.at("pipe_timeout_ms").get<std::size_t>();
            }
            if (j.contains("ping_period_ms")) {
                ping_period_ms = j.at("ping_period_ms").get<std::int64_t>();
            }
        }

        /// \brief Validates the bridge configuration.
        /// \return Pair with success flag and validation message.
        std::pair<bool, std::string> validate() const override {
            if (named_pipe.empty()) {
                return {false, "Named pipe is empty."};
            }
            if (bridge_id == 0) {
                return {false, "Bridge ID is required."};
            }
            if (min_payout < 0.0) {
                return {false, "Minimum payout must not be negative."};
            }
            if (buffer_size == 0) {
                return {false, "Named pipe buffer size must be positive."};
            }
            if (pipe_timeout_ms == 0) {
                return {false, "Named pipe timeout must be positive."};
            }
            if (ping_period_ms <= 0) {
                return {false, "Ping period must be positive."};
            }
            return {true, std::string()};
        }

        /// \brief Creates a unique pointer clone of this configuration.
        /// \return A unique pointer to a copied configuration.
        std::unique_ptr<IBridgeConfig> clone_unique() const override {
            return std::make_unique<LegacyTradingBridgeConfig>(*this);
        }

        /// \brief Creates a shared pointer clone of this configuration.
        /// \return A shared pointer to a copied configuration.
        std::shared_ptr<IBridgeConfig> clone_shared() const override {
            return std::make_shared<LegacyTradingBridgeConfig>(*this);
        }

        /// \brief Returns the bridge type.
        /// \return `BridgeType::LEGACY_TRADING_NAMED_PIPE`.
        BridgeType bridge_type() const override {
            return BridgeType::LEGACY_TRADING_NAMED_PIPE;
        }

        std::string named_pipe = "intrade_bar_console_bot"; ///< Named pipe endpoint name.
        BridgeId bridge_id = 0;                             ///< Source bridge ID; must be non-zero.
        double min_payout = 0.0;                            ///< Minimum accepted payout ratio.
        std::size_t buffer_size = 65536;                    ///< Pipe read/write buffer size.
        std::size_t pipe_timeout_ms = 50;                   ///< Pipe wait timeout in milliseconds.
        std::int64_t ping_period_ms = time_shield::MS_PER_15_SEC; ///< Periodic legacy ping interval.
    };

} // namespace optionx::bridges::legacy_trading

#endif // OPTIONX_HEADER_BRIDGES_LEGACY_TRADING_LEGACY_TRADING_BRIDGE_CONFIG_HPP_INCLUDED
