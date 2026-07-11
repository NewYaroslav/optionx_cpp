#include <optionx_cpp/bridges/TradingView/TradingViewExtensionBridge.hpp>
#include <optionx_cpp/utils/json_comments.hpp>

#include <client_http.hpp>

#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>

namespace {

using optionx::bridges::tradingview::TradingViewExtensionBridge;
using optionx::bridges::tradingview::TradingViewExtensionBridgeConfig;
using HttpClient = SimpleWeb::Client<SimpleWeb::HTTP>;

bool has_arg(int argc, char** argv, const std::string& value) {
    for (int i = 1; i < argc; ++i) {
        if (argv[i] == value) {
            return true;
        }
    }
    return false;
}

std::string option_value(int argc, char** argv, const std::string& name) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (argv[i] == name) {
            return argv[i + 1];
        }
    }
    return {};
}

TradingViewExtensionBridgeConfig default_config(bool self_test) {
    TradingViewExtensionBridgeConfig config;
    config.address = "127.0.0.1";
    config.port = self_test ? 0 : 6560;
    config.bridge_id = 1;
    config.secret = "local-secret";
    config.fixed_amount = 1.0;
    config.duration = 60;
    config.symbol_map["FX:EURUSD"] = "EURUSD";
    return config;
}

bool load_config(
        const std::string& path,
        TradingViewExtensionBridgeConfig& config) {
    if (path.empty()) {
        return true;
    }

    std::ifstream input(path);
    if (!input) {
        std::cerr << "Could not open config: " << path << '\n';
        return false;
    }

    try {
        std::ostringstream buffer;
        buffer << input.rdbuf();
        const auto json = optionx::utils::parse_json_with_comments(buffer.str());
        config.from_json(json);
    } catch (const std::exception& ex) {
        std::cerr << "Could not parse config: " << ex.what() << '\n';
        return false;
    }
    return true;
}

bool wait_for_port(const TradingViewExtensionBridge& bridge) {
    for (int i = 0; i < 200; ++i) {
        if (bridge.bound_port() != 0) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

nlohmann::json make_self_test_payload(const TradingViewExtensionBridgeConfig& config) {
    (void)config;
    return nlohmann::json{
        {"source", "tradingview"},
        {"signal_name", "smoke_noisy_rsi_test"},
        {"action", "buy"},
        {"symbol", "FX:EURUSD"},
        {"tickerid", "FX:EURUSD"},
        {"price", 1.14055},
        {"time", 1783476660000LL},
        {"event_id", "smoke:tradingview:eurusd:buy"}
    };
}

} // namespace

int main(int argc, char** argv) {
    const bool self_test = has_arg(argc, argv, "--self-test");
    auto config = default_config(self_test);
    if (!load_config(option_value(argc, argv, "--config"), config)) {
        return 2;
    }

    const auto validation = config.validate();
    if (!validation.first) {
        std::cerr << "Invalid config: " << validation.second << '\n';
        return 2;
    }

    TradingViewExtensionBridge bridge;
    if (!bridge.configure(std::make_unique<TradingViewExtensionBridgeConfig>(config))) {
        std::cerr << "Bridge configuration failed\n";
        return 2;
    }

    std::atomic<optionx::SignalId> next_signal_id{1};
    std::atomic<int> received_signals{0};

    bridge.on_signal_id() = [&next_signal_id]() {
        return next_signal_id.fetch_add(1);
    };
    bridge.on_status_update() = [](const optionx::BridgeStatusUpdate& update) {
        std::cout << "status=" << optionx::to_str(update.status);
        if (!update.connection_id.empty()) {
            std::cout << " connection=" << update.connection_id;
        }
        if (!update.message.empty()) {
            std::cout << " message=" << update.message;
        }
        std::cout << '\n';
    };
    bridge.on_trade_signal() = [&received_signals](std::unique_ptr<optionx::TradeSignal> signal) {
        ++received_signals;
        nlohmann::json json = *signal;
        std::cout << "signal:\n" << json.dump(2) << '\n';
    };

    bridge.run();
    if (!wait_for_port(bridge)) {
        std::cerr << "Bridge did not bind a HTTP port\n";
        bridge.shutdown();
        return 3;
    }

    std::cout << "TradingView extension bridge is listening at http://"
              << config.address << ':' << bridge.bound_port()
              << config.signal_path << '\n';

    if (self_test) {
        try {
            HttpClient client(config.address + ":" + std::to_string(bridge.bound_port()));
            SimpleWeb::CaseInsensitiveMultimap headers;
            headers.emplace("Content-Type", "application/json");
            headers.emplace("X-OptionX-Secret", config.secret);
            auto response =
                client.request(
                    "POST",
                    config.signal_path,
                    make_self_test_payload(config).dump(),
                    headers);
            std::cout << "self-test response: " << response->content.string() << '\n';
        } catch (const std::exception& ex) {
            std::cerr << "Self-test request failed: " << ex.what() << '\n';
            bridge.shutdown();
            return 4;
        }

        for (int i = 0; i < 100 && received_signals.load() == 0; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        bridge.shutdown();
        return received_signals.load() > 0 ? 0 : 5;
    }

    std::cout << "Press Enter to stop...\n";
    std::string line;
    std::getline(std::cin, line);
    bridge.shutdown();
    return 0;
}
