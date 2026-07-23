#include "bridge_example_utils.hpp"

#include <optionx_cpp/bridges/trading_view.hpp>

#include <client_http.hpp>

#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

namespace {

using optionx::bridges::tradingview::TradingViewExtensionBridge;
using optionx::bridges::tradingview::TradingViewExtensionBridgeConfig;
using HttpClient = SimpleWeb::Client<SimpleWeb::HTTP>;

TradingViewExtensionBridgeConfig default_config(bool self_test);
bool wait_for_port(const TradingViewExtensionBridge& bridge);
nlohmann::json make_self_test_payload(const TradingViewExtensionBridgeConfig& config);

} // namespace

int main(int argc, char** argv) {
    const bool self_test = optionx::examples::has_arg(argc, argv, "--self-test");
    optionx::examples::install_stop_handlers();
    auto config = default_config(self_test);
    if (!optionx::examples::load_json_config(
            optionx::examples::option_value(argc, argv, "--config"), config)) {
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
        optionx::examples::print_status_update(update);
    };
    bridge.on_trade_signal() = [&received_signals](std::unique_ptr<optionx::TradeSignal> signal) {
        ++received_signals;
        nlohmann::json json = *signal;
        std::cout << "signal:\n" << json.dump(2) << '\n';
    };
    bridge.on_signal_report() = [](const optionx::BridgeSignalReport& report) {
        nlohmann::json json = report;
        std::cout << "signal_report:\n" << json.dump(2) << '\n';
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
    std::atomic_bool input_done{false};
    std::thread input_thread([&input_done]() {
        std::string line;
        std::getline(std::cin, line);
        input_done.store(true);
        optionx::examples::request_stop();
    });

    while (!optionx::examples::stop_requested()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (input_thread.joinable()) {
        if (input_done.load()) {
            input_thread.join();
        } else {
            input_thread.detach();
        }
    }
    bridge.shutdown();
    return 0;
}

namespace {

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

bool wait_for_port(const TradingViewExtensionBridge& bridge) {
    return optionx::examples::wait_until([&bridge]() {
        return bridge.bound_port() != 0;
    }, std::chrono::seconds(2));
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
