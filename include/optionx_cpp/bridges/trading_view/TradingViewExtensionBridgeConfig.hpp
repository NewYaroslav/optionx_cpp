#pragma once
#ifndef OPTIONX_HEADER_BRIDGES_TRADING_VIEW_TRADING_VIEW_EXTENSION_BRIDGE_CONFIG_HPP_INCLUDED
#define OPTIONX_HEADER_BRIDGES_TRADING_VIEW_TRADING_VIEW_EXTENSION_BRIDGE_CONFIG_HPP_INCLUDED

/// \file TradingViewExtensionBridgeConfig.hpp
/// \brief Defines configuration for the TradingView browser-extension HTTP bridge.

#include <cstdint>
#include <algorithm>
#include <cctype>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace optionx::bridges::tradingview {

    /// \struct TradingViewLevelAlertRule
    /// \brief Maps a TradingView level alert to a concrete trading direction.
    struct TradingViewLevelAlertRule {
        std::string alert_id;         ///< TradingView alert ID/fire ID to match.
        std::string symbol;           ///< Normalized symbol to match, for example `FX:EURUSD`.
        std::string condition_type;   ///< Alert condition type, for example `crossing_up`.
        std::string message_equals;   ///< Full alert message to match.
        std::string message_contains; ///< Message substring to match.
        std::string action;           ///< `buy`, `sell`, or `reject`.
        std::string signal_name;      ///< Optional signal name override.
    };

    /// \brief Serializes a level alert rule.
    inline void to_json(nlohmann::json& j, const TradingViewLevelAlertRule& rule) {
        j = nlohmann::json{
            {"alert_id", rule.alert_id},
            {"symbol", rule.symbol},
            {"condition_type", rule.condition_type},
            {"message_equals", rule.message_equals},
            {"message_contains", rule.message_contains},
            {"action", rule.action},
            {"signal_name", rule.signal_name}
        };
    }

    /// \brief Deserializes a level alert rule.
    inline void from_json(const nlohmann::json& j, TradingViewLevelAlertRule& rule) {
        rule.alert_id = j.value("alert_id", std::string());
        rule.symbol = j.value("symbol", std::string());
        rule.condition_type = j.value("condition_type", std::string());
        rule.message_equals = j.value("message_equals", std::string());
        rule.message_contains = j.value("message_contains", std::string());
        rule.action = j.value("action", std::string());
        rule.signal_name = j.value("signal_name", std::string());
    }

    /// \class TradingViewExtensionBridgeConfig
    /// \brief Configuration for receiving TradingView browser-extension signals over HTTP.
    class TradingViewExtensionBridgeConfig final : public IBridgeConfig {
    public:
        /// \brief Serializes the bridge configuration.
        /// \param j Output JSON object.
        void to_json(nlohmann::json& j) const override {
            j = nlohmann::json{
                {"address", address},
                {"port", port},
                {"signal_path", signal_path},
                {"bridge_id", bridge_id},
                {"secret", secret},
                {"option_type", option_type},
                {"duration", duration},
                {"min_payout", min_payout},
                {"symbol_map", symbol_map},
                {"dedupe_cache_size", dedupe_cache_size},
                {"request_body_limit", request_body_limit},
                {"allow_cors", allow_cors},
                {"allowed_origin", allowed_origin},
                {"allow_body_secret_fallback", allow_body_secret_fallback},
                {"sizing", {
                    {"mode", sizing_mode},
                    {"fixed_amount", fixed_amount},
                    {"balance_percent", balance_percent},
                    {"min_amount", min_amount},
                    {"max_amount", max_amount}
                }},
                {"action_keywords", {
                    {"use_defaults", use_default_action_keywords},
                    {"buy", buy_action_keywords},
                    {"sell", sell_action_keywords}
                }},
                {"study_alerts", {
                    {"mode", study_alert_mode}
                }},
                {"level_alert_rules", {
                    {"default_action", default_level_action},
                    {"rules", level_alert_rules}
                }}
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
            if (j.contains("signal_path")) {
                signal_path = j.at("signal_path").get<std::string>();
            }
            if (j.contains("bridge_id")) {
                bridge_id = j.at("bridge_id").get<BridgeId>();
            }
            if (j.contains("secret")) {
                secret = j.at("secret").get<std::string>();
            }
            if (j.contains("option_type")) {
                option_type = j.at("option_type").get<OptionType>();
            }
            if (j.contains("duration")) {
                duration = j.at("duration").get<std::uint32_t>();
            }
            if (j.contains("min_payout")) {
                min_payout = j.at("min_payout").get<double>();
            }
            if (j.contains("symbol_map")) {
                symbol_map = j.at("symbol_map").get<std::unordered_map<std::string, std::string>>();
            }
            if (j.contains("dedupe_cache_size")) {
                dedupe_cache_size = j.at("dedupe_cache_size").get<std::size_t>();
            }
            if (j.contains("request_body_limit")) {
                request_body_limit = j.at("request_body_limit").get<std::size_t>();
            }
            if (j.contains("allow_cors")) {
                allow_cors = j.at("allow_cors").get<bool>();
            }
            if (j.contains("allowed_origin")) {
                allowed_origin = j.at("allowed_origin").get<std::string>();
            }
            if (j.contains("allow_body_secret_fallback")) {
                allow_body_secret_fallback =
                    j.at("allow_body_secret_fallback").get<bool>();
            }

            if (j.contains("sizing")) {
                const auto& sizing = j.at("sizing");
                sizing_mode = sizing.value("mode", sizing_mode);
                fixed_amount = sizing.value("fixed_amount", fixed_amount);
                balance_percent = sizing.value("balance_percent", balance_percent);
                min_amount = sizing.value("min_amount", min_amount);
                max_amount = sizing.value("max_amount", max_amount);
            } else {
                if (j.contains("sizing_mode")) {
                    sizing_mode = j.at("sizing_mode").get<std::string>();
                }
                if (j.contains("fixed_amount")) {
                    fixed_amount = j.at("fixed_amount").get<double>();
                }
                if (j.contains("balance_percent")) {
                    balance_percent = j.at("balance_percent").get<double>();
                }
            }

            if (j.contains("action_keywords") && j.at("action_keywords").is_object()) {
                const auto& keywords = j.at("action_keywords");
                if (keywords.contains("use_defaults")) {
                    use_default_action_keywords = keywords.at("use_defaults").get<bool>();
                }
                if (keywords.contains("buy")) {
                    buy_action_keywords = keywords.at("buy").get<std::vector<std::string>>();
                }
                if (keywords.contains("sell")) {
                    sell_action_keywords = keywords.at("sell").get<std::vector<std::string>>();
                }
            }
            if (j.contains("use_default_action_keywords")) {
                use_default_action_keywords =
                    j.at("use_default_action_keywords").get<bool>();
            }
            if (j.contains("buy_action_keywords")) {
                buy_action_keywords =
                    j.at("buy_action_keywords").get<std::vector<std::string>>();
            } else if (j.contains("buy_keywords")) {
                buy_action_keywords =
                    j.at("buy_keywords").get<std::vector<std::string>>();
            }
            if (j.contains("sell_action_keywords")) {
                sell_action_keywords =
                    j.at("sell_action_keywords").get<std::vector<std::string>>();
            } else if (j.contains("sell_keywords")) {
                sell_action_keywords =
                    j.at("sell_keywords").get<std::vector<std::string>>();
            }

            if (j.contains("study_alerts") && j.at("study_alerts").is_object()) {
                const auto& study_alerts = j.at("study_alerts");
                study_alert_mode = study_alerts.value("mode", study_alert_mode);
            }
            if (j.contains("study_alert_mode")) {
                study_alert_mode = j.at("study_alert_mode").get<std::string>();
            }

            if (j.contains("level_alert_rules")) {
                const auto& level_alerts = j.at("level_alert_rules");
                if (level_alerts.is_object()) {
                    default_level_action =
                        level_alerts.value("default_action", default_level_action);
                    if (level_alerts.contains("rules")) {
                        level_alert_rules =
                            level_alerts.at("rules").get<std::vector<TradingViewLevelAlertRule>>();
                    }
                } else if (level_alerts.is_array()) {
                    level_alert_rules =
                        level_alerts.get<std::vector<TradingViewLevelAlertRule>>();
                }
            }
            if (j.contains("default_level_action")) {
                default_level_action = j.at("default_level_action").get<std::string>();
            }
        }

        /// \brief Validates the bridge configuration.
        /// \return Pair with success flag and validation message.
        std::pair<bool, std::string> validate() const override {
            if (address.empty()) {
                return {false, "TradingView bridge address is empty."};
            }
            if (signal_path.empty() || signal_path.front() != '/') {
                return {false, "TradingView bridge signal_path must start with '/'."};
            }
            if (bridge_id == 0) {
                return {false, "Bridge ID is required."};
            }
            if (!is_valid_sizing_mode(sizing_mode)) {
                return {false, "TradingView bridge sizing mode must be none, fixed_amount, or balance_percent."};
            }
            if (fixed_amount < 0.0) {
                return {false, "TradingView bridge fixed_amount must not be negative."};
            }
            if (balance_percent < 0.0) {
                return {false, "TradingView bridge balance_percent must not be negative."};
            }
            if (min_amount < 0.0 || max_amount < 0.0) {
                return {false, "TradingView bridge amount bounds must not be negative."};
            }
            if (max_amount > 0.0 && min_amount > max_amount) {
                return {false, "TradingView bridge min_amount must not exceed max_amount."};
            }
            if (min_payout < 0.0) {
                return {false, "TradingView bridge minimum payout must not be negative."};
            }
            if (dedupe_cache_size == 0) {
                return {false, "TradingView bridge dedupe_cache_size must be positive."};
            }
            if (request_body_limit == 0) {
                return {false, "TradingView bridge request_body_limit must be positive."};
            }
            if (allow_cors && allowed_origin.empty()) {
                return {false, "TradingView bridge allowed_origin must not be empty when CORS is enabled."};
            }
            for (const auto& keyword : buy_action_keywords) {
                if (normalize_token(keyword).empty()) {
                    return {false, "TradingView bridge buy action keywords must not contain empty values."};
                }
            }
            for (const auto& keyword : sell_action_keywords) {
                if (normalize_token(keyword).empty()) {
                    return {false, "TradingView bridge sell action keywords must not contain empty values."};
                }
            }
            if (!is_valid_level_action(default_level_action)) {
                return {false, "TradingView bridge default level alert action must be buy, sell, reject, or ignore."};
            }
            if (!is_valid_study_alert_mode(study_alert_mode)) {
                return {false, "TradingView bridge study alert mode must be realtime, fast, or confirmed_only."};
            }
            for (const auto& rule : level_alert_rules) {
                if (rule.action.empty()) {
                    return {false, "TradingView bridge level alert rule action is required."};
                }
                if (!is_valid_level_action(rule.action)) {
                    return {false, "TradingView bridge level alert rule action must be buy, sell, reject, or ignore."};
                }
                if (rule.alert_id.empty() &&
                    rule.symbol.empty() &&
                    rule.condition_type.empty() &&
                    rule.message_equals.empty() &&
                    rule.message_contains.empty()) {
                    return {false, "TradingView bridge level alert rule must define at least one matcher."};
                }
            }
            return {true, std::string()};
        }

        /// \brief Creates a unique pointer clone of this configuration.
        /// \return A unique pointer to a copied configuration.
        std::unique_ptr<IBridgeConfig> clone_unique() const override {
            return std::make_unique<TradingViewExtensionBridgeConfig>(*this);
        }

        /// \brief Creates a shared pointer clone of this configuration.
        /// \return A shared pointer to a copied configuration.
        std::shared_ptr<IBridgeConfig> clone_shared() const override {
            return std::make_shared<TradingViewExtensionBridgeConfig>(*this);
        }

        /// \brief Returns the bridge type.
        /// \return `BridgeType::TRADING_VIEW_EXTENSION_HTTP`.
        BridgeType bridge_type() const override {
            return BridgeType::TRADING_VIEW_EXTENSION_HTTP;
        }

        /// \brief Returns true when the sizing mode is known.
        static bool is_valid_sizing_mode(const std::string& mode) {
            const auto normalized = normalize_token(mode);
            return normalized == "none" ||
                   normalized == "fixed_amount" ||
                   normalized == "balance_percent";
        }

        /// \brief Returns true when the level alert action is known.
        static bool is_valid_level_action(const std::string& action) {
            const auto normalized = normalize_token(action);
            return normalized == "buy" ||
                   normalized == "sell" ||
                   normalized == "reject" ||
                   normalized == "ignore";
        }

        /// \brief Returns true when the study alert handling mode is known.
        static bool is_valid_study_alert_mode(const std::string& mode) {
            const auto normalized = normalize_token(mode);
            return normalized == "realtime" ||
                   normalized == "fast" ||
                   normalized == "confirmed_only";
        }

        /// \brief Default words that make free-form alert text a buy signal.
        static const std::vector<std::string>& default_buy_action_keywords() {
            static const std::vector<std::string> keywords = {
                "buy",
                "call",
                "long",
                u8"\u0431\u0430\u0439",       // buy transliterated in Russian
                u8"\u043A\u0443\u043F\u0438\u0442\u044C",
                u8"\u043F\u043E\u043A\u0443\u043F",
                u8"\u043B\u043E\u043D\u0433"
            };
            return keywords;
        }

        /// \brief Default words that make free-form alert text a sell signal.
        static const std::vector<std::string>& default_sell_action_keywords() {
            static const std::vector<std::string> keywords = {
                "sell",
                "put",
                "short",
                u8"\u0441\u0435\u043B\u043B",
                u8"\u043F\u0440\u043E\u0434\u0430\u0442\u044C",
                u8"\u043F\u0440\u043E\u0434\u0430",
                u8"\u0448\u043E\u0440\u0442"
            };
            return keywords;
        }

        /// \brief Lowercases and trims a small config token.
        static std::string normalize_token(std::string value) {
            const auto first = value.find_first_not_of(" \t\r\n");
            if (first == std::string::npos) {
                return {};
            }
            const auto last = value.find_last_not_of(" \t\r\n");
            value = value.substr(first, last - first + 1);
            std::transform(
                value.begin(),
                value.end(),
                value.begin(),
                [](unsigned char ch) {
                    return static_cast<char>(std::tolower(ch));
                });
            return value;
        }

        std::string address = "127.0.0.1";       ///< HTTP bind address.
        std::uint16_t port = 6560;               ///< HTTP bind port; 0 allows an OS-assigned port.
        std::string signal_path = "/api/v1/tradingview/signal"; ///< Signal endpoint path.
        BridgeId bridge_id = 0;                  ///< Source bridge ID; must be non-zero.
        std::string secret;                      ///< Optional shared secret expected in X-OptionX-Secret.

        std::string sizing_mode = "fixed_amount"; ///< `fixed_amount`, `balance_percent`, or `none`.
        double fixed_amount = 0.0;                 ///< Amount assigned directly to TradeSignal::amount.
        double balance_percent = 0.0;              ///< Percent sizing hint for downstream postprocessing.
        double min_amount = 0.0;                   ///< Optional minimum amount hint.
        double max_amount = 0.0;                   ///< Optional maximum amount hint.

        OptionType option_type = OptionType::SPRINT; ///< Suggested binary option type.
        std::uint32_t duration = 60;                 ///< Suggested duration in seconds.
        double min_payout = 0.0;                     ///< Minimum accepted payout ratio.

        std::unordered_map<std::string, std::string> symbol_map; ///< External-to-platform symbol map.
        bool use_default_action_keywords = true; ///< Enable built-in buy/sell words for alert text.
        std::vector<std::string> buy_action_keywords; ///< Custom buy words; extend or replace defaults.
        std::vector<std::string> sell_action_keywords; ///< Custom sell words; extend or replace defaults.
        std::string study_alert_mode = "realtime"; ///< `realtime`/`fast` accepts all study alerts; `confirmed_only` accepts HIST_CONFIRMED and RT_CONFIRMED.
        std::string default_level_action = "reject"; ///< Fallback for unmapped level alerts.
        std::vector<TradingViewLevelAlertRule> level_alert_rules; ///< User-defined level alert mappings.

        std::size_t dedupe_cache_size = 1024; ///< Recent event IDs retained to reject duplicates.
        std::size_t request_body_limit = 64 * 1024; ///< Maximum accepted request body in bytes.
        bool allow_cors = true; ///< Add permissive local CORS headers for browser-extension clients.
        std::string allowed_origin = "*"; ///< Allowed CORS origin; use chrome-extension://<id> outside dev.
        bool allow_body_secret_fallback = false; ///< Legacy opt-in for JSON body secret auth.
    };

} // namespace optionx::bridges::tradingview

#endif // OPTIONX_HEADER_BRIDGES_TRADING_VIEW_TRADING_VIEW_EXTENSION_BRIDGE_CONFIG_HPP_INCLUDED
