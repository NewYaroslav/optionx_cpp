#include "example_utils.hpp"

#include <optionx_cpp/platforms.hpp>

#include <atomic>
#include <chrono>
#include <ctime>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

namespace {

void pump(optionx::platforms::IntradeBarPlatform& platform,
          std::chrono::seconds duration);
void print_subscription_result(
    const optionx::market_data::MarketDataSubscriptionResult& result);

} // namespace

int main() {
    using namespace optionx;
    using namespace optionx::platforms;

    IntradeBarPlatform platform;

    // IntradeBarPlatform exposes market-data callbacks directly. A UI or bot
    // can either attach here or route the same events through MarketDataHub.
    platform.on_market_data_status() =
        [](market_data::MarketDataStatusUpdate update) {
            std::cout << "[status] "
                      << update.symbol
                      << " "
                      << market_data::to_str(update.type)
                      << " "
                      << market_data::to_str(update.transport)
                      << " -> "
                      << market_data::to_str(update.status);
            if (!update.message.empty()) {
                std::cout << " (" << update.message << ")";
            }
            std::cout << '\n';
        };

    platform.on_tick_data() =
        [](std::unique_ptr<market_data::TickDataBatch> batch) {
            if (!batch || batch->items.empty()) return;
            const auto& tick = batch->items.back();
            std::cout << "[tick] "
                      << batch->symbol
                      << " bid=" << tick.bid
                      << " ask=" << tick.ask
                      << " items=" << batch->items.size()
                      << '\n';
        };

    platform.on_bar_data() =
        [](std::unique_ptr<market_data::BarDataBatch> batch) {
            if (!batch) return;
            std::cout << "[bars] "
                      << batch->symbol
                      << " timeframe=" << batch->timeframe
                      << " items=" << batch->items.size()
                      << '\n';
        };

    auto auth = std::make_unique<intrade_bar::AuthData>();
    auth->host = optionx::examples::env_or("OPTIONX_INTRADE_HOST", "https://intrade.bar");

    const auto email = optionx::examples::env_or("OPTIONX_INTRADE_EMAIL");
    const auto password = optionx::examples::env_or("OPTIONX_INTRADE_PASSWORD");
    const bool has_credentials = !email.empty() && !password.empty();
    if (has_credentials) {
        auth->set_email_password(email, password);
    } else {
        std::cout
            << "Set OPTIONX_INTRADE_EMAIL and OPTIONX_INTRADE_PASSWORD "
            << "to run authenticated live lifecycle.\n";
    }

    platform.configure_auth(std::move(auth));
    platform.event_bus().drain();

    // The batch API makes subscription changes atomic: accepted items become
    // active together, and rejected items are reported in one callback.
    market_data::MarketDataSubscriptionBatch subscriptions;
    subscriptions
        .subscribe_ticks(market_data::TickSubscriptionRequest(
            "BTCUSDT",
            market_data::MarketDataTransport::WEBSOCKET))
        .subscribe_bars(market_data::BarSubscriptionRequest(
            "BTCUSDT",
            60,
            BarPriceSource::LAST,
            market_data::MarketDataTransport::WEBSOCKET))
        .subscribe_ticks(market_data::TickSubscriptionRequest(
            "EUR/USD",
            market_data::MarketDataTransport::POLLING))
        .subscribe_bars(market_data::BarSubscriptionRequest(
            "EUR/USD",
            60,
            BarPriceSource::MID,
            market_data::MarketDataTransport::POLLING))
        .subscribe_ticks(market_data::TickSubscriptionRequest(
            "EUR/USD",
            market_data::MarketDataTransport::WEBSOCKET));

    platform.apply_subscriptions(
        std::move(subscriptions),
        [](market_data::MarketDataSubscriptionBatchResult result) {
            std::cout << "[subscriptions] "
                      << market_data::to_str(result.status)
                      << " count=" << result.results.size()
                      << '\n';
            if (!result.error_message.empty()) {
                std::cout << "  error=\"" << result.error_message << "\"\n";
            }
            for (const auto& item : result.results) {
                print_subscription_result(item);
            }
        });

    // Direct history requests return one completed sequence.
    const auto now = static_cast<std::int64_t>(std::time(nullptr));
    const BarHistoryRequest btc_history(
        "BTCUSDT",
        60,
        now - 60 * 30,
        now,
        BarPriceSource::LAST);

    platform.fetch_bar_history(
        btc_history,
        [](BarHistoryResult result) {
            if (!result) {
                std::cout << "[history] failed: " << result.error_desc << '\n';
                return;
            }
            std::cout << "[history] "
                      << result.sequence.symbol
                      << " bars=" << result.sequence.bars.size()
                      << '\n';
        });

    // ContinuityService demonstrates a higher-level helper: it asks the
    // platform for missing bars and returns a batch ready for chart storage.
    market_data::MarketDataContinuityService continuity(platform);
    continuity.request_bar_history_batch(
        BarHistoryRequest("EUR/USD", 60, now - 60 * 30, now, BarPriceSource::MID),
        {},
        [](std::unique_ptr<market_data::BarDataBatch> batch) {
            if (!batch) return;
            std::cout << "[backfill] "
                      << batch->symbol
                      << " bars=" << batch->items.size()
                      << '\n';
        },
        [](BarHistoryResult result) {
            std::cout << "[backfill] failed: " << result.error_desc << '\n';
        });

    if (has_credentials) {
        std::atomic<bool> connect_finished{false};
        platform.connect([&connect_finished](ConnectionResult result) {
            std::cout << "[connect] "
                      << (result.success ? "connected" : "failed")
                      << " reason=\"" << result.reason << "\"\n";
            connect_finished = true;
        });

        pump(platform, std::chrono::seconds(30));
        if (!connect_finished.load()) {
            std::cout << "[connect] still pending after example loop\n";
        }
    } else {
        pump(platform, std::chrono::seconds(10));
    }

    platform.shutdown();
    return 0;
}

namespace {

void pump(optionx::platforms::IntradeBarPlatform& platform,
          std::chrono::seconds duration) {
    const auto deadline = std::chrono::steady_clock::now() + duration;
    while (std::chrono::steady_clock::now() < deadline) {
        platform.event_bus().drain();
        platform.process();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    platform.event_bus().drain();
}

void print_subscription_result(
        const optionx::market_data::MarketDataSubscriptionResult& result) {
    std::cout << "  "
              << optionx::market_data::to_str(result.status)
              << " id=" << result.subscription.id
              << " symbol=" << result.subscription.symbol
              << " transport=" << optionx::market_data::to_str(result.subscription.transport);
    if (!result.error_message.empty()) {
        std::cout << " error=\"" << result.error_message << "\"";
    }
    std::cout << '\n';
}

} // namespace
