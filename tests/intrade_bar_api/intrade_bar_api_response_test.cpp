#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <future>
#include <limits>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <server_http.hpp>
#include <server_ws.hpp>

#include <optionx_cpp/platforms.hpp>

using namespace optionx;
using namespace optionx::platforms;
using namespace optionx::platforms::intrade_bar;

namespace {

enum class TestRateLimitType : std::uint32_t {
    GENERAL
};

class TestHttpClientComponent : public optionx::components::BaseHttpClientComponent {
public:
    explicit TestHttpClientComponent(optionx::utils::EventBus& bus)
        : optionx::components::BaseHttpClientComponent(bus) {}

    void add_general_limit() {
        set_rate_limit_rps(TestRateLimitType::GENERAL, 1);
    }

    std::size_t rate_limit_count() const {
        return m_rate_limits.size();
    }

    std::uint32_t general_limit_id() const {
        return get_rate_limit(TestRateLimitType::GENERAL);
    }
};

class TestPlatform : public optionx::platforms::BaseTradingPlatform {
public:
    TestPlatform()
        : optionx::platforms::BaseTradingPlatform(
            std::make_shared<optionx::platforms::intrade_bar::AccountInfoData>()) {}

    optionx::PlatformType platform_type() const override {
        return optionx::PlatformType::INTRADE_BAR;
    }
};

class UnsupportedEndpointConfig final : public optionx::IEndpointConfig {
};

SingleTick make_market_data_tick(const std::string& symbol, double bid, double ask) {
    SingleTick tick;
    tick.symbol = symbol;
    tick.provider = to_str(PlatformType::INTRADE_BAR);
    tick.price_digits = price_digits_for_symbol(symbol);
    tick.volume_digits = 0;
    tick.tick.bid = bid;
    tick.tick.ask = ask;
    tick.tick.time_ms = 1000;
    tick.tick.received_ms = 1001;
    tick.set_flag(TickStatusFlags::INITIALIZED);
    tick.set_flag(TickStatusFlags::REALTIME);
    return tick;
}

void publish_account_status(
        IntradeBarPlatform& platform,
        AccountUpdateStatus status) {
    platform.event_bus().notify_async(
        std::make_unique<events::AccountInfoUpdateEvent>(
            std::make_shared<AccountInfoData>(),
            status));
    platform.event_bus().drain();
}

template <class Predicate>
bool wait_for_platform(
        IntradeBarPlatform& platform,
        Predicate&& predicate,
        std::chrono::milliseconds timeout = std::chrono::seconds(5)) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!predicate() && std::chrono::steady_clock::now() < deadline) {
        platform.event_bus().drain();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    platform.event_bus().drain();
    return predicate();
}

using TradeHistoryHttpServer = SimpleWeb::Server<SimpleWeb::HTTP>;
using MarketDataWsServer = SimpleWeb::SocketServer<SimpleWeb::WS>;

class BoundPortHttpServer : public TradeHistoryHttpServer {
public:
    unsigned short bound_port() const noexcept {
        return m_bound_port.load();
    }

protected:
    void after_bind() override {
        m_bound_port.store(acceptor->local_endpoint().port());
    }

private:
    std::atomic<unsigned short> m_bound_port{0};
};

struct LocalTradeHistoryServer {
    explicit LocalTradeHistoryServer(
            long csv_status_code,
            std::string csv_response_body,
            long html_status_code,
            std::string html_response_body)
        : csv_status(csv_status_code),
          csv_body(std::move(csv_response_body)),
          html_status(html_status_code),
          html_body(std::move(html_response_body)) {}

    ~LocalTradeHistoryServer() {
        stop();
    }

    bool start() {
        server.config.address = "127.0.0.1";
        server.config.port = 0;

        server.resource["^/health$"]["GET"] = [](
                std::shared_ptr<TradeHistoryHttpServer::Response> response,
                std::shared_ptr<TradeHistoryHttpServer::Request>) {
            response->write(SimpleWeb::StatusCode::success_ok, "ok");
        };

        server.resource["^/stat_trade_export\\.php$"]["POST"] = [this](
                std::shared_ptr<TradeHistoryHttpServer::Response> response,
                std::shared_ptr<TradeHistoryHttpServer::Request>) {
            ++csv_requests;
            response->write(static_cast<SimpleWeb::StatusCode>(csv_status), csv_body);
        };

        server.resource["^/$"]["GET"] = [this](
                std::shared_ptr<TradeHistoryHttpServer::Response> response,
                std::shared_ptr<TradeHistoryHttpServer::Request>) {
            ++html_requests;
            response->write(static_cast<SimpleWeb::StatusCode>(html_status), html_body);
        };

        thread = std::thread([this]() {
            server.start();
        });

        if (!wait_until_ready()) {
            stop();
            return false;
        }
        return true;
    }

    void stop() {
        server.stop();
        if (thread.joinable()) thread.join();
    }

    std::string host() const {
        return "http://127.0.0.1:" + std::to_string(server.bound_port());
    }

    bool wait_until_ready() const {
        for (int i = 0; i < 100; ++i) {
            if (server.bound_port() == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            try {
                kurlyk::HttpClient client(host());
                client.set_timeout(1);
                client.set_connect_timeout(1);
                auto future = client.get("/health", {}, {});
                if (future.wait_for(std::chrono::seconds(1)) == std::future_status::ready) {
                    auto response = future.get();
                    if (response && response->status_code == 200) return true;
                }
            } catch (...) {
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        return false;
    }

    BoundPortHttpServer server;
    std::thread thread;
    long csv_status = 200;
    std::string csv_body;
    long html_status = 200;
    std::string html_body;
    std::atomic<int> csv_requests{0};
    std::atomic<int> html_requests{0};
};

struct LocalFxConnectServer {
    ~LocalFxConnectServer() {
        stop();
    }

    bool start() {
        server.config.address = "127.0.0.1";
        server.config.port = 0;
        server.config.thread_pool_size = 1;

        auto& fx = server.endpoint["^/fxconnect/?$"];
        fx.on_message = [this](
                std::shared_ptr<MarketDataWsServer::Connection> connection,
                std::shared_ptr<MarketDataWsServer::InMessage> message) {
            {
                std::lock_guard<std::mutex> lock(mutex);
                last_subscription = message->string();
            }
            ++subscription_count;
            connection->send(
                R"({"Updates":1783028728,"ask":1.10002,"bid":1.10001,"symbol":"EUR\/USD"})");
        };

        auto port_promise = std::make_shared<std::promise<unsigned short>>();
        auto port_future = port_promise->get_future();
        thread = std::thread([this, port_promise]() {
            server.start([port_promise](unsigned short port) {
                try {
                    port_promise->set_value(port);
                } catch (...) {
                }
            });
        });

        if (port_future.wait_for(std::chrono::seconds(5)) != std::future_status::ready) {
            stop();
            return false;
        }

        port = port_future.get();
        return port != 0;
    }

    void stop() {
        server.stop();
        if (thread.joinable()) thread.join();
    }

    std::string host() const {
        return "http://127.0.0.1:" + std::to_string(port);
    }

    std::string subscribed_symbol() const {
        std::lock_guard<std::mutex> lock(mutex);
        return last_subscription;
    }

    MarketDataWsServer server;
    std::thread thread;
    unsigned short port = 0;
    std::atomic<int> subscription_count{0};
    mutable std::mutex mutex;
    std::string last_subscription;
};

struct CombinedHistoryTestResult {
    TradeHistoryApiResult result;
    int callback_count = 0;
    bool callback_received = false;
};

struct BarHistoryTestResult {
    BarHistoryApiResult result;
    int callback_count = 0;
    bool callback_received = false;
};

struct LocalBarHistoryServer {
    explicit LocalBarHistoryServer(std::vector<std::string> response_bodies)
        : bodies(std::move(response_bodies)) {}

    ~LocalBarHistoryServer() {
        stop();
    }

    bool start() {
        server.config.address = "127.0.0.1";
        server.config.port = 0;

        server.resource["^/health$"]["GET"] = [](
                std::shared_ptr<TradeHistoryHttpServer::Response> response,
                std::shared_ptr<TradeHistoryHttpServer::Request>) {
            response->write(SimpleWeb::StatusCode::success_ok, "ok");
        };

        server.resource["^/fxhis/?$"]["GET"] = [this](
                std::shared_ptr<TradeHistoryHttpServer::Response> response,
                std::shared_ptr<TradeHistoryHttpServer::Request> request) {
            const auto index = fxhis_requests.fetch_add(1);
            {
                std::lock_guard<std::mutex> lock(mutex);
                queries.push_back(request->query_string);
            }

            const auto body_index =
                static_cast<std::size_t>(std::min<int>(
                    index,
                    static_cast<int>(bodies.size()) - 1));
            response->write(SimpleWeb::StatusCode::success_ok, bodies[body_index]);
        };

        thread = std::thread([this]() {
            server.start();
        });

        if (!wait_until_ready()) {
            stop();
            return false;
        }
        return true;
    }

    void stop() {
        server.stop();
        if (thread.joinable()) thread.join();
    }

    std::string host() const {
        return "http://127.0.0.1:" + std::to_string(server.bound_port());
    }

    bool wait_until_ready() const {
        for (int i = 0; i < 100; ++i) {
            if (server.bound_port() == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            try {
                kurlyk::HttpClient client(host());
                client.set_timeout(1);
                client.set_connect_timeout(1);
                auto future = client.get("/health", {}, {});
                if (future.wait_for(std::chrono::seconds(1)) == std::future_status::ready) {
                    auto response = future.get();
                    if (response && response->status_code == 200) return true;
                }
            } catch (...) {
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        return false;
    }

    BoundPortHttpServer server;
    std::thread thread;
    std::vector<std::string> bodies;
    std::atomic<int> fxhis_requests{0};
    mutable std::mutex mutex;
    std::vector<std::string> queries;
};

std::string valid_trade_history_csv() {
    return
        "id;Type;Asset;Direction;Open;Close;Open quote;Close quote;Amount;Result\n"
        "123;Sprint;BTCUSD;Up;16:34:33, 23 Jun 21;16:39:33, 23 Jun 21;"
        "62830.01;62850.00;1 USD;1.79 USD\n";
}

std::string empty_trade_history_html() {
    return R"HTML(
        <div id="trade_close_block" class="hide">
            <table class="">
                <tbody class="table_tbody" id="trade_close">
                </tbody>
            </table>
            <div class="text-center">
                <a id="trade_btn_load_more" data-last=""></a>
            </div>
        </div>
    )HTML";
}

std::string fxhis_response(std::int64_t timestamp, double bid_open = 0.56880) {
    const double bid_close = bid_open - 0.00005;
    const double bid_high = bid_open + 0.00010;
    const double bid_low = bid_open - 0.00011;
    const double ask_open = bid_open + 0.00003;
    const double ask_close = bid_close + 0.00001;
    const double ask_high = bid_high + 0.00003;
    const double ask_low = bid_low + 0.00003;

    std::ostringstream out;
    out << R"({"response":{"error":"","executed":true},"instrument_id":"NZD\/USD","period_id":"m1","candles":[[)"
        << timestamp << ','
        << bid_open << ','
        << bid_close << ','
        << bid_high << ','
        << bid_low << ','
        << ask_open << ','
        << ask_close << ','
        << ask_high << ','
        << ask_low << ",104]]}";
    return out.str();
}

CombinedHistoryTestResult request_trade_history(
        LocalTradeHistoryServer& server,
        TradeHistorySource source = TradeHistorySource::HTML_CSV,
        TradeHistoryRequest request = TradeHistoryRequest::all(),
        AccountType account_type = AccountType::DEMO) {
    TestPlatform platform;
    HttpClientComponent http_client(platform);
    RequestManager request_manager(platform, http_client);

    auto auth_data = std::make_shared<AuthData>();
    auth_data->host = server.host();
    auth_data->trade_history_source = source;

    events::AuthDataEvent auth_event(auth_data);
    request_manager.on_event(&auth_event);
    http_client.get_http_client().set_retry_attempts(0, 0);
    request_manager.set_auth_credentials("866188", "test_hash");

    CombinedHistoryTestResult call;
    std::atomic<int> callback_count{0};

    request_manager.request_trade_history_result(
        request,
        account_type,
        [&call, &callback_count](TradeHistoryApiResult history_result) {
            call.result = std::move(history_result);
            ++callback_count;
        });

    for (int i = 0; i < 500; ++i) {
        http_client.process();
        if (callback_count.load() != 0) {
            call.callback_received = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    for (int i = 0; i < 20; ++i) {
        http_client.process();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    call.callback_count = callback_count.load();
    platform.shutdown();
    return call;
}

BarHistoryTestResult request_bar_history(
        LocalBarHistoryServer& server,
        BarHistoryRequest request) {
    TestPlatform platform;
    HttpClientComponent http_client(platform);
    RequestManager request_manager(platform, http_client);

    auto auth_data = std::make_shared<AuthData>();
    auth_data->host = server.host();

    events::AuthDataEvent auth_event(auth_data);
    request_manager.on_event(&auth_event);
    http_client.get_http_client().set_retry_attempts(0, 0);

    BarHistoryTestResult call;
    std::atomic<int> callback_count{0};

    request_manager.request_bar_history_result(
        request,
        [&call, &callback_count](BarHistoryApiResult result) {
            call.result = std::move(result);
            ++callback_count;
        });

    for (int i = 0; i < 500; ++i) {
        http_client.process();
        if (callback_count.load() != 0) {
            call.callback_received = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    for (int i = 0; i < 20; ++i) {
        http_client.process();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    call.callback_count = callback_count.load();
    platform.shutdown();
    return call;
}

CombinedHistoryTestResult request_trade_history_without_http(
        TradeHistoryRequest request,
        AccountType account_type) {
    TestPlatform platform;
    HttpClientComponent http_client(platform);
    RequestManager request_manager(platform, http_client);

    CombinedHistoryTestResult call;
    int callback_count = 0;

    request_manager.request_trade_history_result(
        request,
        account_type,
        [&call, &callback_count](TradeHistoryApiResult history_result) {
            call.result = std::move(history_result);
            ++callback_count;
        });

    call.callback_count = callback_count;
    call.callback_received = callback_count != 0;
    platform.shutdown();
    return call;
}

} // namespace

TEST(BaseHttpClientComponent, ShutdownClearsRateLimitsAndIsRepeatable) {
    optionx::utils::EventBus bus;
    TestHttpClientComponent component(bus);

    component.add_general_limit();
    EXPECT_EQ(component.rate_limit_count(), 1u);
    EXPECT_NE(component.general_limit_id(), 0u);
    const auto limit_id = component.general_limit_id();
    EXPECT_TRUE(static_cast<bool>(kurlyk::get_rate_limit(limit_id)));

    component.shutdown();
    EXPECT_EQ(component.rate_limit_count(), 0u);
    EXPECT_EQ(component.general_limit_id(), 0u);
    EXPECT_FALSE(static_cast<bool>(kurlyk::get_rate_limit(limit_id)));

    component.shutdown();
    EXPECT_EQ(component.rate_limit_count(), 0u);
    EXPECT_EQ(component.general_limit_id(), 0u);
    EXPECT_FALSE(static_cast<bool>(kurlyk::get_rate_limit(limit_id)));
}

TEST(BaseHttpClientComponent, DestructorClearsRateLimits) {
    optionx::utils::EventBus bus;
    std::uint32_t limit_id = 0;

    {
        TestHttpClientComponent component(bus);
        component.add_general_limit();
        limit_id = component.general_limit_id();
        ASSERT_NE(limit_id, 0u);
        EXPECT_TRUE(static_cast<bool>(kurlyk::get_rate_limit(limit_id)));
    }

    EXPECT_FALSE(static_cast<bool>(kurlyk::get_rate_limit(limit_id)));
}

TEST(BaseHttpClientComponent, ReplacingRateLimitClearsPreviousHandle) {
    optionx::utils::EventBus bus;
    TestHttpClientComponent component(bus);

    component.add_general_limit();
    const auto first_limit_id = component.general_limit_id();
    ASSERT_NE(first_limit_id, 0u);
    EXPECT_TRUE(static_cast<bool>(kurlyk::get_rate_limit(first_limit_id)));

    component.add_general_limit();
    const auto second_limit_id = component.general_limit_id();
    ASSERT_NE(second_limit_id, 0u);
    EXPECT_NE(first_limit_id, second_limit_id);

    EXPECT_FALSE(static_cast<bool>(kurlyk::get_rate_limit(first_limit_id)));
    EXPECT_TRUE(static_cast<bool>(kurlyk::get_rate_limit(second_limit_id)));

    component.shutdown();
    EXPECT_FALSE(static_cast<bool>(kurlyk::get_rate_limit(second_limit_id)));
}

TEST(IntradeBarApiResponses, ApiResultCarriesTypedSuccessPayload) {
    auto result = BalanceInfoResult::ok(BalanceInfo{42.5, CurrencyType::USD}, 200);

    ASSERT_TRUE(result);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.status_code, 200);
    EXPECT_TRUE(result.has_http_status());
    EXPECT_DOUBLE_EQ(result.value.balance, 42.5);
    EXPECT_EQ(result.value.currency, CurrencyType::USD);
    EXPECT_TRUE(result.error_message.empty());
}

TEST(IntradeBarApiResponses, ApiResultCarriesTypedFailure) {
    auto result = TradeOpenResult::fail("blocked", 451);

    EXPECT_FALSE(result);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.status_code, 451);
    EXPECT_TRUE(result.has_http_status());
    EXPECT_EQ(result.error_message, "blocked");
    EXPECT_EQ(result.value.option_id, 0);
}

TEST(IntradeBarApiResponses, ApiResultMarksMissingHttpStatusExplicitly) {
    auto success = HostAvailabilityResult::ok(HostAvailability{true});
    EXPECT_TRUE(success);
    EXPECT_EQ(success.status_code, HostAvailabilityResult::NO_HTTP_STATUS);
    EXPECT_FALSE(success.has_http_status());

    auto failure = ProfileInfoResult::fail("no response");
    EXPECT_FALSE(failure);
    EXPECT_EQ(failure.status_code, ProfileInfoResult::NO_RESPONSE_STATUS);
    EXPECT_FALSE(failure.has_http_status());
}

TEST(IntradeBarApiResponses, IntradeBarPlatformExposesEndpointTradingAndMarketDataRoles) {
    IntradeBarPlatform platform;

    BaseEndpoint* endpoint = &platform;
    BaseTradingApi* trading_api = &platform;
    market_data::BaseMarketDataProvider* market_data_provider = &platform;

    EXPECT_FALSE(endpoint->is_connected());
    EXPECT_NE(trading_api, nullptr);
    EXPECT_FALSE(market_data_provider->fetch_bar_history(BarHistoryRequest{}, nullptr));
    EXPECT_FALSE(endpoint->configure(std::make_unique<UnsupportedEndpointConfig>()));
    EXPECT_TRUE(endpoint->configure(std::make_unique<AuthData>()));

    platform.shutdown();
}

TEST(IntradeBarApiResponses, IntradeBarTickSubscriptionRoutesMatchingPriceEvents) {
    IntradeBarPlatform platform;
    market_data::TickDataBatch delivered_batch;
    int tick_callback_count = 0;
    market_data::MarketDataSubscriptionResult subscription_result;

    platform.on_tick_data() =
        [&delivered_batch, &tick_callback_count](std::unique_ptr<market_data::TickDataBatch> batch) {
            if (batch) {
                delivered_batch = std::move(*batch);
            }
            ++tick_callback_count;
        };

    const bool accepted = platform.subscribe_ticks(
        market_data::TickSubscriptionRequest(
            "EUR/USD",
            market_data::MarketDataTransport::POLLING),
        [&subscription_result](market_data::MarketDataSubscriptionResult result) {
            subscription_result = std::move(result);
        });

    EXPECT_TRUE(accepted);
    ASSERT_TRUE(subscription_result);
    EXPECT_EQ(subscription_result.status, market_data::MarketDataSubscriptionStatus::SUBSCRIBED);
    EXPECT_TRUE(subscription_result.subscription.valid());
    EXPECT_EQ(subscription_result.subscription.provider_id, platform.provider_id());
    EXPECT_EQ(subscription_result.subscription.symbol, "EURUSD");
    EXPECT_EQ(subscription_result.subscription.stream_type, market_data::MarketDataType::TICKS);

    std::vector<SingleTick> event_ticks = {
        make_market_data_tick("EURUSD", 1.0, 1.1),
        make_market_data_tick("EUR/USD", 1.2, 1.3),
        make_market_data_tick("BTCUSDT", 60000.0, 60001.0)
    };

    platform.event_bus().notify_async(
        std::make_unique<events::PriceUpdateEvent>(std::move(event_ticks)));
    platform.event_bus().process();

    ASSERT_EQ(tick_callback_count, 1);
    ASSERT_EQ(delivered_batch.items.size(), 2u);
    EXPECT_EQ(delivered_batch.symbol, "EURUSD");
    EXPECT_EQ(delivered_batch.subscription.id, subscription_result.subscription.id);
    EXPECT_TRUE(delivered_batch.items[0].has_flag(MarketDataFlags::REALTIME));
    EXPECT_TRUE(delivered_batch.items[1].has_flag(MarketDataFlags::REALTIME));

    platform.shutdown();
}

TEST(IntradeBarApiResponses, IntradeBarTickSubscriptionStopsAfterUnsubscribe) {
    IntradeBarPlatform platform;
    int tick_callback_count = 0;
    market_data::MarketDataSubscriptionResult subscription_result;
    market_data::MarketDataSubscriptionResult unsubscribe_result;

    platform.on_tick_data() =
        [&tick_callback_count](std::unique_ptr<market_data::TickDataBatch> batch) {
            if (batch && !batch->items.empty()) {
                ++tick_callback_count;
            }
        };

    ASSERT_TRUE(platform.subscribe_ticks(
        market_data::TickSubscriptionRequest("BTC/USD"),
        [&subscription_result](market_data::MarketDataSubscriptionResult result) {
            subscription_result = std::move(result);
        }));

    EXPECT_TRUE(platform.unsubscribe(
        subscription_result.subscription,
        [&unsubscribe_result](market_data::MarketDataSubscriptionResult result) {
            unsubscribe_result = std::move(result);
        }));

    EXPECT_TRUE(unsubscribe_result);
    EXPECT_EQ(unsubscribe_result.status, market_data::MarketDataSubscriptionStatus::UNSUBSCRIBED);

    std::vector<SingleTick> event_ticks = {
        make_market_data_tick("BTCUSDT", 60000.0, 60001.0)
    };

    platform.event_bus().notify_async(
        std::make_unique<events::PriceUpdateEvent>(std::move(event_ticks)));
    platform.event_bus().process();

    EXPECT_EQ(tick_callback_count, 0);

    platform.shutdown();
}

TEST(IntradeBarApiResponses, IntradeBarAppliesTickSubscriptionBatchAtomically) {
    IntradeBarPlatform platform;
    std::vector<market_data::TickDataBatch> delivered_batches;
    market_data::MarketDataSubscriptionBatchResult apply_result;
    market_data::MarketDataSubscriptionBatchResult unsubscribe_result;

    platform.on_tick_data() =
        [&delivered_batches](std::unique_ptr<market_data::TickDataBatch> batch) {
            if (batch) {
                delivered_batches.push_back(std::move(*batch));
            }
        };

    market_data::MarketDataSubscriptionBatch batch;
    batch.subscribe_ticks(market_data::TickSubscriptionRequest(
        "EUR/USD",
        market_data::MarketDataTransport::POLLING));
    batch.subscribe_ticks(market_data::TickSubscriptionRequest(
        "BTC/USD",
        market_data::MarketDataTransport::POLLING));

    ASSERT_TRUE(platform.apply_subscriptions(
        std::move(batch),
        [&apply_result](market_data::MarketDataSubscriptionBatchResult result) {
            apply_result = std::move(result);
        }));

    ASSERT_TRUE(apply_result);
    ASSERT_EQ(apply_result.results.size(), 2u);
    EXPECT_EQ(apply_result.status, market_data::MarketDataSubscriptionStatus::APPLIED);
    EXPECT_EQ(apply_result.results[0].subscription.symbol, "EURUSD");
    EXPECT_EQ(apply_result.results[1].subscription.symbol, "BTCUSDT");

    std::vector<SingleTick> event_ticks = {
        make_market_data_tick("EURUSD", 1.0, 1.1),
        make_market_data_tick("BTCUSDT", 60000.0, 60001.0)
    };

    platform.event_bus().notify_async(
        std::make_unique<events::PriceUpdateEvent>(std::move(event_ticks)));
    platform.event_bus().process();

    ASSERT_EQ(delivered_batches.size(), 2u);
    EXPECT_EQ(delivered_batches[0].items.size(), 1u);
    EXPECT_EQ(delivered_batches[1].items.size(), 1u);

    EXPECT_TRUE(platform.unsubscribe_all(
        [&unsubscribe_result](market_data::MarketDataSubscriptionBatchResult result) {
            unsubscribe_result = std::move(result);
        }));
    EXPECT_TRUE(unsubscribe_result);
    EXPECT_EQ(unsubscribe_result.results.size(), 2u);

    platform.shutdown();
}

TEST(IntradeBarApiResponses, IntradeBarRejectedSubscriptionBatchDoesNotPartiallySubscribe) {
    IntradeBarPlatform platform;
    int tick_callback_count = 0;
    market_data::MarketDataSubscriptionBatchResult result;

    platform.on_tick_data() =
        [&tick_callback_count](std::unique_ptr<market_data::TickDataBatch> batch) {
            if (batch && !batch->items.empty()) {
                ++tick_callback_count;
            }
        };

    market_data::MarketDataSubscriptionBatch batch;
    batch.subscribe_ticks(market_data::TickSubscriptionRequest(
        "EUR/USD",
        market_data::MarketDataTransport::POLLING));
    batch.subscribe_bars(market_data::BarSubscriptionRequest("EUR/USD", 60));

    EXPECT_FALSE(platform.apply_subscriptions(
        std::move(batch),
        [&result](market_data::MarketDataSubscriptionBatchResult batch_result) {
            result = std::move(batch_result);
        }));

    EXPECT_FALSE(result);
    EXPECT_EQ(result.status, market_data::MarketDataSubscriptionStatus::UNSUPPORTED);

    std::vector<SingleTick> event_ticks = {
        make_market_data_tick("EURUSD", 1.0, 1.1)
    };
    platform.event_bus().notify_async(
        std::make_unique<events::PriceUpdateEvent>(std::move(event_ticks)));
    platform.event_bus().process();

    EXPECT_EQ(tick_callback_count, 0);

    platform.shutdown();
}

TEST(IntradeBarApiResponses, IntradeBarBarSubscriptionsReportUnsupportedTypedResult) {
    IntradeBarPlatform platform;
    market_data::MarketDataSubscriptionResult result;

    const bool accepted = platform.subscribe_bars(
        market_data::BarSubscriptionRequest("EUR/USD", 60),
        [&result](market_data::MarketDataSubscriptionResult subscription_result) {
            result = std::move(subscription_result);
        });

    EXPECT_FALSE(accepted);
    EXPECT_FALSE(result);
    EXPECT_EQ(result.status, market_data::MarketDataSubscriptionStatus::UNSUPPORTED);
    EXPECT_EQ(result.subscription.symbol, "EURUSD");
    EXPECT_EQ(result.subscription.stream_type, market_data::MarketDataType::BARS);

    platform.shutdown();
}

TEST(IntradeBarApiResponses, PriceDigitsMatchBrokerSymbols) {
    EXPECT_EQ(price_digits_for_symbol("BTCUSD"), 2);
    EXPECT_EQ(price_digits_for_symbol("BTCUSDT"), 2);
    EXPECT_EQ(price_digits_for_symbol("EUR/JPY"), 3);
    EXPECT_EQ(price_digits_for_symbol("EURUSD"), 5);

    SpreadPack spread;
    set_zero_spread_for_symbol(spread, "BTCUSD");
    EXPECT_EQ(spread.digits, 2);
    EXPECT_DOUBLE_EQ(spread.open_spread(), 0.0);
    EXPECT_DOUBLE_EQ(spread.close_spread(), 0.0);
}

TEST(IntradeBarSymbols, NormalizesBtcAliases) {
    EXPECT_EQ(normalize_symbol_name("BTCUSD"), "BTCUSDT");
    EXPECT_EQ(normalize_symbol_name("btcusd"), "BTCUSDT");
    EXPECT_EQ(normalize_symbol_name("BTC/USDT"), "BTCUSDT");
    EXPECT_EQ(normalize_symbol_name("btc/usdt"), "BTCUSDT");
    EXPECT_EQ(normalize_symbol_name("BTC/USD"), "BTCUSDT");
    EXPECT_EQ(normalize_symbol_name("btc/usd"), "BTCUSDT");
    EXPECT_EQ(normalize_symbol_name("EUR/USD"), "EURUSD");
    EXPECT_EQ(normalize_symbol_name("eur/usd"), "EURUSD");

    EXPECT_TRUE(is_btc_symbol("BTCUSD"));
    EXPECT_TRUE(is_btc_symbol("btcusd"));
    EXPECT_TRUE(is_btc_symbol("BTCUSDT"));
    EXPECT_TRUE(is_btc_symbol("BTC/USD"));
    EXPECT_TRUE(is_btc_symbol("btc/usd"));
    EXPECT_FALSE(is_btc_symbol("EURUSD"));
}

TEST(IntradeBarSymbols, FormatsFxConnectSymbols) {
    EXPECT_EQ(make_fxconnect_symbol("EURUSD"), "EUR/USD");
    EXPECT_EQ(make_fxconnect_symbol("eur/usd"), "EUR/USD");
    EXPECT_EQ(make_fxconnect_symbol("NZD/USD"), "NZD/USD");
    EXPECT_TRUE(is_fxconnect_supported_symbol("USDJPY"));
    EXPECT_FALSE(is_fxconnect_supported_symbol("ABCDEF"));
    EXPECT_EQ(make_fxconnect_symbol("ABCDEF"), "");
    EXPECT_EQ(make_fxconnect_symbol("XAUUSD"), "");
    EXPECT_EQ(make_fxconnect_symbol("BTCUSD"), "");
    EXPECT_EQ(make_fxconnect_symbol("BTCUSDT"), "");
    EXPECT_EQ(make_fxconnect_symbol(""), "");
}

TEST(IntradeBarApiResponses, ParsesFxConnectTickMessage) {
    const std::string message =
        R"({"Updates":1783028728,"ask":0.56971,"bid":0.5693,"symbol":"NZD\/USD"})";

    SingleTick tick;
    ASSERT_TRUE(parse_fxconnect_tick(message, tick));

    EXPECT_EQ(tick.symbol, "NZDUSD");
    EXPECT_EQ(tick.provider, to_str(PlatformType::INTRADE_BAR));
    EXPECT_EQ(tick.price_digits, 5u);
    EXPECT_EQ(tick.volume_digits, 0u);
    EXPECT_DOUBLE_EQ(tick.tick.ask, 0.56971);
    EXPECT_DOUBLE_EQ(tick.tick.bid, 0.5693);
    EXPECT_DOUBLE_EQ(tick.tick.volume, 0.0);
    EXPECT_EQ(tick.tick.time_ms, 1783028728000ULL);
    EXPECT_TRUE(tick.tick.has_flag(TickUpdateFlags::ASK_UPDATED));
    EXPECT_TRUE(tick.tick.has_flag(TickUpdateFlags::BID_UPDATED));
    EXPECT_FALSE(tick.tick.has_flag(TickUpdateFlags::VOLUME_UPDATED));
    EXPECT_TRUE(tick.has_flag(TickStatusFlags::INITIALIZED));
    EXPECT_TRUE(tick.has_flag(TickStatusFlags::REALTIME));
}

TEST(IntradeBarApiResponses, RejectsFxConnectBtcTickMessage) {
    const std::string message =
        R"({"Updates":1783028728,"ask":61521.35,"bid":61521.34,"symbol":"BTC\/USD"})";

    SingleTick tick;
    EXPECT_FALSE(parse_fxconnect_tick(message, tick));
}

TEST(IntradeBarApiResponses, ParsesBtcusdtWebSocketTickWithEpochMilliseconds) {
    const std::string message =
        R"({"stream":"btcusdt@aggTrade","data":{"e":"aggTrade","E":1783028778697,"s":"BTCUSDT","a":4005288360,"p":"61521.34000000","q":"0.00017000","f":6473852503,"l":6473852503,"T":1783028778697,"m":false,"M":true}})";

    SingleTick tick;
    ASSERT_TRUE(parse_btcusdt_tick(message, tick));

    EXPECT_EQ(tick.symbol, "BTCUSDT");
    EXPECT_EQ(tick.provider, to_str(PlatformType::INTRADE_BAR));
    EXPECT_EQ(tick.price_digits, 2u);
    EXPECT_EQ(tick.volume_digits, 5u);
    EXPECT_DOUBLE_EQ(tick.tick.ask, 61521.34);
    EXPECT_DOUBLE_EQ(tick.tick.bid, 61521.34);
    EXPECT_DOUBLE_EQ(tick.tick.volume, 0.00017);
    EXPECT_EQ(tick.tick.time_ms, 1783028778697ULL);
    EXPECT_TRUE(tick.tick.has_flag(TickUpdateFlags::ASK_UPDATED));
    EXPECT_TRUE(tick.tick.has_flag(TickUpdateFlags::BID_UPDATED));
    EXPECT_TRUE(tick.tick.has_flag(TickUpdateFlags::VOLUME_UPDATED));
    EXPECT_EQ(tick.tick.price_type(), MarketPriceType::LAST);
    EXPECT_TRUE(tick.has_flag(TickStatusFlags::INITIALIZED));
    EXPECT_TRUE(tick.has_flag(TickStatusFlags::REALTIME));
}

TEST(IntradeBarApiResponses, FxWebSocketSubscriptionReceivesLocalTick) {
    LocalFxConnectServer server;
    ASSERT_TRUE(server.start());

    IntradeBarPlatform platform;
    market_data::TickDataBatch delivered_batch;
    int callback_count = 0;
    platform.on_tick_data() =
        [&delivered_batch, &callback_count](std::unique_ptr<market_data::TickDataBatch> batch) {
            ++callback_count;
            if (batch) {
                delivered_batch = std::move(*batch);
            }
        };

    auto auth = std::make_unique<AuthData>();
    auth->set_email_password("user@example.test", "unused");
    auth->host = server.host();
    ASSERT_TRUE(platform.configure_auth(std::move(auth)));
    platform.event_bus().drain();

    market_data::MarketDataSubscriptionResult subscription_result;
    const bool accepted = platform.subscribe_ticks(
        market_data::TickSubscriptionRequest(
            "EUR/USD",
            market_data::MarketDataTransport::WEBSOCKET),
        [&subscription_result](market_data::MarketDataSubscriptionResult result) {
            subscription_result = std::move(result);
        });

    ASSERT_TRUE(accepted);
    ASSERT_TRUE(subscription_result);
    ASSERT_TRUE(subscription_result.subscription.valid());

    publish_account_status(platform, AccountUpdateStatus::CONNECTED);
    ASSERT_TRUE(wait_for_platform(
        platform,
        [&]() {
            return !delivered_batch.items.empty();
        }));

    EXPECT_EQ(server.subscribed_symbol(), "EUR/USD");
    ASSERT_EQ(delivered_batch.items.size(), 1u);
    EXPECT_EQ(callback_count, 1);
    EXPECT_EQ(delivered_batch.symbol, "EURUSD");
    EXPECT_EQ(delivered_batch.subscription.id, subscription_result.subscription.id);
    EXPECT_DOUBLE_EQ(delivered_batch.items[0].ask, 1.10002);
    EXPECT_DOUBLE_EQ(delivered_batch.items[0].bid, 1.10001);
    EXPECT_TRUE(delivered_batch.items[0].has_flag(TickUpdateFlags::ASK_UPDATED));
    EXPECT_TRUE(delivered_batch.items[0].has_flag(TickUpdateFlags::BID_UPDATED));
    EXPECT_TRUE(delivered_batch.items[0].has_flag(MarketDataFlags::REALTIME));

    market_data::MarketDataSubscriptionResult unsubscribe_result;
    EXPECT_TRUE(platform.unsubscribe(
        subscription_result.subscription,
        [&unsubscribe_result](market_data::MarketDataSubscriptionResult result) {
            unsubscribe_result = std::move(result);
        }));
    EXPECT_TRUE(unsubscribe_result);

    platform.shutdown();
}

TEST(IntradeBarApiResponses, WebsocketSubscriptionIgnoresPollingSnapshotForSameSymbol) {
    IntradeBarPlatform platform;
    int callback_count = 0;
    market_data::TickDataBatch delivered_batch;
    platform.on_tick_data() =
        [&callback_count, &delivered_batch](std::unique_ptr<market_data::TickDataBatch> batch) {
            ++callback_count;
            if (batch) {
                delivered_batch = std::move(*batch);
            }
        };

    market_data::MarketDataSubscriptionResult subscription_result;
    ASSERT_TRUE(platform.subscribe_ticks(
        market_data::TickSubscriptionRequest(
            "EUR/USD",
            market_data::MarketDataTransport::WEBSOCKET),
        [&subscription_result](market_data::MarketDataSubscriptionResult result) {
            subscription_result = std::move(result);
        }));
    ASSERT_TRUE(subscription_result);

    std::vector<SingleTick> polling_ticks;
    polling_ticks.push_back(make_market_data_tick("EURUSD", 1.10001, 1.10002));
    platform.event_bus().notify_async(
        std::make_unique<events::PriceUpdateEvent>(std::move(polling_ticks)));
    platform.event_bus().drain();

    EXPECT_EQ(callback_count, 0);
    EXPECT_TRUE(delivered_batch.items.empty());

    std::vector<SingleTick> websocket_ticks;
    websocket_ticks.push_back(make_market_data_tick("EURUSD", 1.10003, 1.10004));
    platform.event_bus().notify_async(
        std::make_unique<events::PriceUpdateEvent>(
            std::move(websocket_ticks),
            MarketDataUpdateSource::WEBSOCKET));
    platform.event_bus().drain();

    EXPECT_EQ(callback_count, 1);
    ASSERT_EQ(delivered_batch.items.size(), 1u);
    EXPECT_DOUBLE_EQ(delivered_batch.items[0].bid, 1.10003);
    EXPECT_DOUBLE_EQ(delivered_batch.items[0].ask, 1.10004);

    platform.shutdown();
}

TEST(IntradeBarApiResponses, RejectsUnsupportedFxWebsocketSymbol) {
    IntradeBarPlatform platform;

    market_data::MarketDataSubscriptionResult subscription_result;
    const bool accepted = platform.subscribe_ticks(
        market_data::TickSubscriptionRequest(
            "ABCDEF",
            market_data::MarketDataTransport::WEBSOCKET),
        [&subscription_result](market_data::MarketDataSubscriptionResult result) {
            subscription_result = std::move(result);
        });

    EXPECT_FALSE(accepted);
    EXPECT_FALSE(subscription_result);
    EXPECT_EQ(
        subscription_result.status,
        market_data::MarketDataSubscriptionStatus::UNSUPPORTED);
    EXPECT_FALSE(subscription_result.subscription.valid());
}

TEST(IntradeBarApiResponses, PollingSubscriptionDoesNotOpenFxWebSocket) {
    LocalFxConnectServer server;
    ASSERT_TRUE(server.start());

    IntradeBarPlatform platform;
    auto auth = std::make_unique<AuthData>();
    auth->set_email_password("user@example.test", "unused");
    auth->host = server.host();
    ASSERT_TRUE(platform.configure_auth(std::move(auth)));
    platform.event_bus().drain();

    market_data::MarketDataSubscriptionResult subscription_result;
    ASSERT_TRUE(platform.subscribe_ticks(
        market_data::TickSubscriptionRequest(
            "EUR/USD",
            market_data::MarketDataTransport::POLLING),
        [&subscription_result](market_data::MarketDataSubscriptionResult result) {
            subscription_result = std::move(result);
        }));
    ASSERT_TRUE(subscription_result);

    publish_account_status(platform, AccountUpdateStatus::CONNECTED);
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    platform.event_bus().drain();

    EXPECT_EQ(server.subscription_count.load(), 0);

    platform.shutdown();
}

TEST(IntradeBarApiResponses, FxWebSocketSubscriptionsAreRefCountedBySymbol) {
    LocalFxConnectServer server;
    ASSERT_TRUE(server.start());

    IntradeBarPlatform platform;
    auto auth = std::make_unique<AuthData>();
    auth->set_email_password("user@example.test", "unused");
    auth->host = server.host();
    ASSERT_TRUE(platform.configure_auth(std::move(auth)));
    platform.event_bus().drain();

    market_data::MarketDataSubscriptionResult first_result;
    market_data::MarketDataSubscriptionResult second_result;
    ASSERT_TRUE(platform.subscribe_ticks(
        market_data::TickSubscriptionRequest(
            "EUR/USD",
            market_data::MarketDataTransport::WEBSOCKET),
        [&first_result](market_data::MarketDataSubscriptionResult result) {
            first_result = std::move(result);
        }));
    ASSERT_TRUE(platform.subscribe_ticks(
        market_data::TickSubscriptionRequest(
            "EURUSD",
            market_data::MarketDataTransport::WEBSOCKET),
        [&second_result](market_data::MarketDataSubscriptionResult result) {
            second_result = std::move(result);
        }));
    ASSERT_TRUE(first_result);
    ASSERT_TRUE(second_result);

    publish_account_status(platform, AccountUpdateStatus::CONNECTED);
    ASSERT_TRUE(wait_for_platform(
        platform,
        [&]() {
            return server.subscription_count.load() >= 1;
        }));

    EXPECT_EQ(server.subscription_count.load(), 1);

    market_data::MarketDataSubscriptionResult unsubscribe_result;
    EXPECT_TRUE(platform.unsubscribe(
        first_result.subscription,
        [&unsubscribe_result](market_data::MarketDataSubscriptionResult result) {
            unsubscribe_result = std::move(result);
        }));
    EXPECT_TRUE(unsubscribe_result);

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    platform.event_bus().drain();
    EXPECT_EQ(server.subscription_count.load(), 1);

    EXPECT_TRUE(platform.unsubscribe(
        second_result.subscription,
        [&unsubscribe_result](market_data::MarketDataSubscriptionResult result) {
            unsubscribe_result = std::move(result);
        }));

    platform.shutdown();
}

TEST(IntradeBarApiResponses, FxWebSocketReconnectsDesiredSubscriptions) {
    LocalFxConnectServer server;
    ASSERT_TRUE(server.start());

    IntradeBarPlatform platform;
    auto auth = std::make_unique<AuthData>();
    auth->set_email_password("user@example.test", "unused");
    auth->host = server.host();
    ASSERT_TRUE(platform.configure_auth(std::move(auth)));
    platform.event_bus().drain();

    market_data::MarketDataSubscriptionResult subscription_result;
    ASSERT_TRUE(platform.subscribe_ticks(
        market_data::TickSubscriptionRequest(
            "EUR/USD",
            market_data::MarketDataTransport::WEBSOCKET),
        [&subscription_result](market_data::MarketDataSubscriptionResult result) {
            subscription_result = std::move(result);
        }));
    ASSERT_TRUE(subscription_result);

    publish_account_status(platform, AccountUpdateStatus::CONNECTED);
    ASSERT_TRUE(wait_for_platform(
        platform,
        [&]() {
            return server.subscription_count.load() >= 1;
        }));

    publish_account_status(platform, AccountUpdateStatus::DISCONNECTED);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    platform.event_bus().drain();

    publish_account_status(platform, AccountUpdateStatus::CONNECTED);
    EXPECT_TRUE(wait_for_platform(
        platform,
        [&]() {
            return server.subscription_count.load() >= 2;
        }));

    platform.shutdown();
}

TEST(IntradeBarAccountInfo, AcceptsBtcAliasAndUsesBtcDurationRules) {
    AccountInfoData account;
    const int64_t day_timestamp = 1712345600;
    const int64_t day_start = time_shield::start_of_day(day_timestamp);

    AccountInfoRequest request;
    request.type = AccountInfoType::SYMBOL_AVAILABILITY;
    request.symbol = "BTCUSD";
    EXPECT_TRUE(account.get_info<bool>(request));

    request.symbol = "btcusd";
    EXPECT_TRUE(account.get_info<bool>(request));

    request.symbol = "BTC/USD";
    EXPECT_TRUE(account.get_info<bool>(request));

    request.symbol = "BTCUSDT";
    EXPECT_TRUE(account.get_info<bool>(request));

    request.type = AccountInfoType::MIN_DURATION;
    request.symbol = "BTCUSD";
    EXPECT_EQ(account.get_info<int64_t>(request), account.min_btc_duration);

    request.type = AccountInfoType::MAX_DURATION;
    request.symbol = "BTCUSD";
    request.timestamp = account.end_time - 30;
    EXPECT_EQ(account.get_info<int64_t>(request), account.max_duration);

    request.symbol = "EURUSD";
    EXPECT_LT(account.get_info<int64_t>(request), account.max_duration);

    request.timestamp = day_timestamp;
    request.symbol = "btc/usd";
    request.type = AccountInfoType::START_TIME;
    EXPECT_EQ(account.get_info<int64_t>(request), day_start + account.start_btc_time);
    request.type = AccountInfoType::END_TIME;
    EXPECT_EQ(account.get_info<int64_t>(request), day_start + account.end_btc_time);

    request.symbol = "EURUSD";
    request.type = AccountInfoType::START_TIME;
    EXPECT_EQ(account.get_info<int64_t>(request), day_start + account.start_time);
    request.type = AccountInfoType::END_TIME;
    EXPECT_EQ(account.get_info<int64_t>(request), day_start + account.end_time);
}

TEST(IntradeBarAccountInfo, AccountInfoRequestKeepsTradeDurationWidth) {
    TradeRequest trade_request;
    trade_request.duration =
        static_cast<int64_t>(std::numeric_limits<int>::max()) + 42;

    const AccountInfoRequest by_reference(
        trade_request,
        AccountInfoType::DURATION_AVAILABLE);
    EXPECT_EQ(by_reference.duration, trade_request.duration);

    const AccountInfoRequest by_pointer(
        &trade_request,
        AccountInfoType::DURATION_AVAILABLE);
    EXPECT_EQ(by_pointer.duration, trade_request.duration);
}

TEST(IntradeBarTradeExecution, NormalizesBtcAliasBeforeQueueProcessing) {
    IntradeBarPlatform platform;

    bool callback_called = false;
    std::string callback_symbol;
    TradeErrorCode callback_error = TradeErrorCode::INVALID_REQUEST;

    platform.on_trade_result() = [&](
            std::unique_ptr<TradeRequest> request,
            std::unique_ptr<TradeResult> result) {
        callback_called = true;
        callback_symbol = request ? request->symbol : std::string();
        callback_error = result ? result->error_code : TradeErrorCode::INVALID_REQUEST;
    };

    auto request = std::make_unique<TradeRequest>();
    request->symbol = "btc/usd";
    request->option_type = OptionType::SPRINT;
    request->order_type = OrderType::BUY;
    request->account_type = AccountType::DEMO;
    request->currency = CurrencyType::USD;
    request->amount = 1.0;
    request->duration = 300;

    platform.run(false);
    ASSERT_TRUE(platform.place_trade(std::move(request)));

    for (int i = 0; i < 200 && !callback_called; ++i) {
        platform.process();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    platform.shutdown();

    ASSERT_TRUE(callback_called);
    EXPECT_EQ(callback_symbol, "BTCUSDT");
    EXPECT_EQ(callback_error, TradeErrorCode::NO_CONNECTION);
}

TEST(IntradeBarAuthData, KeepsDisconnectedDomainRetryPeriodInConfig) {
    AuthData auth_data;
    auth_data.set_email_password("user@example.test", "secret");
    auth_data.account_type = AccountType::DEMO;
    auth_data.currency = CurrencyType::USD;
    auth_data.disconnected_domain_retry_period_ms = 12345;
    auth_data.order_interval_ms = 250;
    auth_data.trade_history_source = TradeHistorySource::HTML_CSV;

    nlohmann::json json;
    auth_data.to_json(json);
    ASSERT_EQ(json.at("disconnected_domain_retry_period_ms").get<int64_t>(), 12345);
    ASSERT_EQ(json.at("order_interval_ms").get<int64_t>(), 250);
    ASSERT_EQ(json.at("trade_history_source").get<std::string>(), "HTML_CSV");

    AuthData restored;
    restored.from_json(json);
    EXPECT_EQ(restored.disconnected_domain_retry_period_ms, 12345);
    EXPECT_EQ(restored.order_interval_ms, 250);
    EXPECT_EQ(restored.trade_history_source, TradeHistorySource::HTML_CSV);

    auto [valid, message] = restored.validate();
    EXPECT_TRUE(valid) << message;

    restored.disconnected_domain_retry_period_ms = 0;
    auto [invalid, invalid_message] = restored.validate();
    EXPECT_FALSE(invalid);
    EXPECT_EQ(invalid_message, "Disconnected domain retry period must be positive");

    restored.disconnected_domain_retry_period_ms = 12345;
    restored.order_interval_ms = -1;
    auto [invalid_order_interval, order_interval_message] = restored.validate();
    EXPECT_FALSE(invalid_order_interval);
    EXPECT_EQ(order_interval_message, "Order interval must be non-negative");
}

TEST(IntradeBarApiResponses, ParsesBalanceWithCommaDotIntegerAndUtf8Ruble) {
    const auto comma_usd = parse_balance("12,34 $");
    ASSERT_TRUE(comma_usd.has_value());
    EXPECT_DOUBLE_EQ(comma_usd->first, 12.34);
    EXPECT_EQ(comma_usd->second, CurrencyType::USD);

    const auto dot_usd = parse_balance("12.34 USD");
    ASSERT_TRUE(dot_usd.has_value());
    EXPECT_DOUBLE_EQ(dot_usd->first, 12.34);
    EXPECT_EQ(dot_usd->second, CurrencyType::USD);

    const auto integer_usd = parse_balance("12 $");
    ASSERT_TRUE(integer_usd.has_value());
    EXPECT_DOUBLE_EQ(integer_usd->first, 12.0);
    EXPECT_EQ(integer_usd->second, CurrencyType::USD);

    const auto rub = parse_balance(std::string("123,45 ") + "\xE2\x82\xBD");
    ASSERT_TRUE(rub.has_value());
    EXPECT_DOUBLE_EQ(rub->first, 123.45);
    EXPECT_EQ(rub->second, CurrencyType::RUB);
}

TEST(IntradeBarApiResponses, TradeWorkflowPayloadsKeepBrokerSpecificFieldsTyped) {
    TradeOpenInfo opened;
    opened.option_id = 123;
    opened.open_date = 456;
    opened.open_price = 1.2345;

    auto open_result = TradeOpenResult::ok(opened, 200);
    ASSERT_TRUE(open_result);
    EXPECT_EQ(open_result.value.option_id, 123);
    EXPECT_EQ(open_result.value.open_date, 456);
    EXPECT_DOUBLE_EQ(open_result.value.open_price, 1.2345);

    auto check_result = TradeCheckResult::ok(TradeCheckInfo{1.2350, 18.0}, 200);
    ASSERT_TRUE(check_result);
    EXPECT_DOUBLE_EQ(check_result.value.price, 1.2350);
    EXPECT_DOUBLE_EQ(check_result.value.profit, 18.0);
}

TEST(IntradeBarApiResponses, AppliesTradeCheckInfoToTradeResultOutcome) {
    TradeResult standoff;
    standoff.amount = 10.0;
    ASSERT_TRUE(apply_trade_check_info_to_result(TradeCheckInfo{1.2345, 10.0}, standoff));
    EXPECT_EQ(standoff.trade_state, TradeState::STANDOFF);
    EXPECT_EQ(standoff.live_state, TradeState::STANDOFF);
    EXPECT_DOUBLE_EQ(standoff.close_price, 1.2345);
    EXPECT_DOUBLE_EQ(standoff.profit, 0.0);
    EXPECT_EQ(standoff.error_code, TradeErrorCode::SUCCESS);

    TradeResult loss;
    loss.amount = 10.0;
    ASSERT_TRUE(apply_trade_check_info_to_result(TradeCheckInfo{1.2340, 0.0}, loss));
    EXPECT_EQ(loss.trade_state, TradeState::LOSS);
    EXPECT_EQ(loss.live_state, TradeState::LOSS);
    EXPECT_DOUBLE_EQ(loss.profit, -10.0);

    TradeResult win;
    win.amount = 10.0;
    ASSERT_TRUE(apply_trade_check_info_to_result(TradeCheckInfo{1.2350, 18.0}, win));
    EXPECT_EQ(win.trade_state, TradeState::WIN);
    EXPECT_EQ(win.live_state, TradeState::WIN);
    EXPECT_DOUBLE_EQ(win.profit, 8.0);
    EXPECT_DOUBLE_EQ(win.payout, 0.8);
}

TEST(IntradeBarApiResponses, RequiresAmountToClassifyTradeCheckInfo) {
    TradeResult result;

    EXPECT_FALSE(apply_trade_check_info_to_result(TradeCheckInfo{1.2345, 1.0}, result));
    EXPECT_EQ(result.trade_state, TradeState::CHECK_ERROR);
    EXPECT_EQ(result.live_state, TradeState::CHECK_ERROR);
    EXPECT_EQ(result.error_code, TradeErrorCode::INVALID_REQUEST);
    EXPECT_EQ(
        result.error_desc,
        "Trade amount is required to classify Intrade Bar trade result.");
}

TEST(IntradeBarApiResponses, ParsesSuccessfulSettingsSwitchResponse) {
    const auto result = parse_settings_switch_response("ok", 200, "currency");

    ASSERT_TRUE(result);
    EXPECT_EQ(result.status_code, 200);
    EXPECT_EQ(result.value.failure_reason, SettingsSwitchFailureReason::NONE);
    EXPECT_FALSE(result.value.should_retry());
    EXPECT_TRUE(result.value.response_body.empty());
}

TEST(IntradeBarApiResponses, ClassifiesBrokerRejectedSettingsSwitchAsRetryable) {
    const auto result = parse_settings_switch_response("error", 200, "account type");

    EXPECT_FALSE(result);
    EXPECT_EQ(result.status_code, 200);
    EXPECT_EQ(result.value.failure_reason, SettingsSwitchFailureReason::BROKER_REJECTED);
    EXPECT_TRUE(result.value.should_retry());
    EXPECT_EQ(result.value.response_body, "error");
    EXPECT_NE(result.error_message.find("active trades"), std::string::npos);
}

TEST(IntradeBarApiResponses, TrimsSettingsSwitchResponseBeforeClassification) {
    const auto ok_result = parse_settings_switch_response(" ok\n", 200, "currency");
    ASSERT_TRUE(ok_result);
    EXPECT_EQ(ok_result.value.failure_reason, SettingsSwitchFailureReason::NONE);

    const auto error_result = parse_settings_switch_response(" error\r\n", 200, "account type");
    EXPECT_FALSE(error_result);
    EXPECT_EQ(error_result.value.failure_reason, SettingsSwitchFailureReason::BROKER_REJECTED);
    EXPECT_TRUE(error_result.value.should_retry());
    EXPECT_EQ(error_result.value.response_body, " error\r\n");
}

TEST(IntradeBarApiResponses, ClassifiesUnexpectedSettingsSwitchResponseAsNonRetryable) {
    const auto result = parse_settings_switch_response("session expired", 200, "currency");

    EXPECT_FALSE(result);
    EXPECT_EQ(result.status_code, 200);
    EXPECT_EQ(result.value.failure_reason, SettingsSwitchFailureReason::UNEXPECTED_RESPONSE);
    EXPECT_FALSE(result.value.should_retry());
    EXPECT_EQ(result.value.response_body, "session expired");
}

TEST(IntradeBarApiResponses, ParsesTradeHistorySourceConfigValues) {
    EXPECT_EQ(trade_history_source_from_string("HTML"), TradeHistorySource::HTML);
    EXPECT_EQ(trade_history_source_from_string("CSV"), TradeHistorySource::CSV);
    EXPECT_EQ(trade_history_source_from_string("html+csv"), TradeHistorySource::HTML_CSV);
    EXPECT_EQ(trade_history_source_from_string("html_csv"), TradeHistorySource::HTML_CSV);
    EXPECT_EQ(
        trade_history_source_from_string("bad", TradeHistorySource::HTML),
        TradeHistorySource::HTML);
}

TEST(IntradeBarApiResponses, CombinedTradeHistoryStatusPrefersFailedSource) {
    EXPECT_EQ(select_combined_trade_history_status(true, 200, false, 451), 451);
    EXPECT_EQ(select_combined_trade_history_status(false, 500, true, 200), 500);
    EXPECT_EQ(select_combined_trade_history_status(false, -1, false, 451), 451);
    EXPECT_EQ(select_combined_trade_history_status(false, -1, false, -2), -1);
    EXPECT_EQ(select_combined_trade_history_status(false, 500, false, 451), 500);
    EXPECT_EQ(select_combined_trade_history_status(true, 200, true, 200), 200);
}

TEST(IntradeBarApiResponses, ParsesFxHisBidAskAndMidBars) {
    BarHistoryRequest request("NZDUSD", 60, 1782980700, 1782980700);
    request.price_source = BarPriceSource::MID;

    const auto mid_sequence = parse_fxhis_bar_history(
        fxhis_response(1782980700),
        request);

    ASSERT_EQ(mid_sequence.bars.size(), 1u);
    EXPECT_EQ(mid_sequence.symbol, "NZDUSD");
    EXPECT_EQ(mid_sequence.provider, to_str(PlatformType::INTRADE_BAR));
    EXPECT_EQ(mid_sequence.price_source, BarPriceSource::MID);
    EXPECT_EQ(mid_sequence.bars[0].time_ms, 1782980700000ull);
    EXPECT_DOUBLE_EQ(mid_sequence.bars[0].open, (0.56880 + 0.56883) / 2.0);
    EXPECT_DOUBLE_EQ(mid_sequence.bars[0].high, (0.56890 + 0.56893) / 2.0);
    EXPECT_DOUBLE_EQ(mid_sequence.bars[0].low, (0.56869 + 0.56872) / 2.0);
    EXPECT_DOUBLE_EQ(mid_sequence.bars[0].close, (0.56875 + 0.56876) / 2.0);
    EXPECT_DOUBLE_EQ(mid_sequence.bars[0].volume, 104.0);

    request.price_source = BarPriceSource::BID;
    const auto bid_sequence = parse_fxhis_bar_history(
        fxhis_response(1782980700),
        request);
    ASSERT_EQ(bid_sequence.bars.size(), 1u);
    EXPECT_EQ(bid_sequence.price_source, BarPriceSource::BID);
    EXPECT_DOUBLE_EQ(bid_sequence.bars[0].open, 0.56880);
    EXPECT_DOUBLE_EQ(bid_sequence.bars[0].high, 0.56890);
    EXPECT_DOUBLE_EQ(bid_sequence.bars[0].low, 0.56869);
    EXPECT_DOUBLE_EQ(bid_sequence.bars[0].close, 0.56875);

    request.price_source = BarPriceSource::ASK;
    const auto ask_sequence = parse_fxhis_bar_history(
        fxhis_response(1782980700),
        request);
    ASSERT_EQ(ask_sequence.bars.size(), 1u);
    EXPECT_EQ(ask_sequence.price_source, BarPriceSource::ASK);
    EXPECT_DOUBLE_EQ(ask_sequence.bars[0].open, 0.56883);
    EXPECT_DOUBLE_EQ(ask_sequence.bars[0].high, 0.56893);
    EXPECT_DOUBLE_EQ(ask_sequence.bars[0].low, 0.56872);
    EXPECT_DOUBLE_EQ(ask_sequence.bars[0].close, 0.56876);
}

TEST(IntradeBarApiResponses, RejectsFxHisLastPriceSource) {
    BarHistoryRequest request("NZDUSD", 60, 1782980700, 1782980700);
    request.price_source = BarPriceSource::LAST;

    EXPECT_THROW(
        parse_fxhis_bar_history(fxhis_response(1782980700), request),
        std::runtime_error);
}

TEST(IntradeBarApiResponses, ParsesBinanceKlinesAsLastPriceBars) {
    const std::string payload =
        R"([[1783016040000,"61521.34","61530.00","61500.00","61510.00","0.25",1783016099999]])";

    BarHistoryRequest request("BTCUSDT", 60, 1783016040, 1783016040);
    request.price_source = BarPriceSource::MID;

    const auto sequence = parse_binance_klines_bar_history(payload, request);

    ASSERT_EQ(sequence.bars.size(), 1u);
    EXPECT_EQ(sequence.symbol, "BTCUSDT");
    EXPECT_EQ(sequence.price_source, BarPriceSource::LAST);
    EXPECT_EQ(sequence.bars[0].time_ms, 1783016040000ull);
    EXPECT_DOUBLE_EQ(sequence.bars[0].open, 61521.34);
    EXPECT_DOUBLE_EQ(sequence.bars[0].high, 61530.00);
    EXPECT_DOUBLE_EQ(sequence.bars[0].low, 61500.00);
    EXPECT_DOUBLE_EQ(sequence.bars[0].close, 61510.00);
    EXPECT_DOUBLE_EQ(sequence.bars[0].volume, 0.25);
}

TEST(IntradeBarApiResponses, UsesKnownBarHistoryStartLimits) {
    EXPECT_EQ(minimum_bar_history_from_ts("EUR/USD"), 1007337600);
    EXPECT_EQ(minimum_bar_history_from_ts("NZDUSD"), 1007424000);
    EXPECT_EQ(minimum_bar_history_from_ts("AUDCAD"), 1059523200);
    EXPECT_EQ(minimum_bar_history_from_ts("BTCUSDT"), 1502942400);
    EXPECT_EQ(minimum_bar_history_from_ts("UNKNOWN"), 0);
}

TEST(IntradeBarApiResponses, PlatformBarHistoryResultPreservesFailureReason) {
    IntradeBarPlatform platform;
    BarHistoryResult result;
    int callback_count = 0;

    EXPECT_TRUE(platform.fetch_bar_history(
        BarHistoryRequest("", time_shield::SEC_PER_MIN, 1000, 2000),
        [&result, &callback_count](BarHistoryResult history_result) {
            result = std::move(history_result);
            ++callback_count;
        }));

    platform.shutdown();

    ASSERT_EQ(callback_count, 1);
    EXPECT_FALSE(result);
    EXPECT_EQ(result.status_code, BarHistoryResult::NO_RESPONSE_STATUS);
    EXPECT_NE(result.error_desc.find("symbol"), std::string::npos);
    EXPECT_TRUE(result.sequence.bars.empty());
}

TEST(IntradeBarApiResponses, PlatformBarHistoryRejectsNegativeTimeframe) {
    IntradeBarPlatform platform;
    BarHistoryResult result;
    int callback_count = 0;

    EXPECT_TRUE(platform.fetch_bar_history(
        BarHistoryRequest("EURUSD", -16, 1000, 2000),
        [&result, &callback_count](BarHistoryResult history_result) {
            result = std::move(history_result);
            ++callback_count;
        }));

    platform.shutdown();

    ASSERT_EQ(callback_count, 1);
    EXPECT_FALSE(result);
    EXPECT_EQ(result.status_code, BarHistoryResult::NO_RESPONSE_STATUS);
    EXPECT_NE(result.error_desc.find("timeframe"), std::string::npos);
}

TEST(IntradeBarApiResponses, SplitsFxHisBarHistoryIntoSequentialRequests) {
    const std::int64_t first_ts = 1782980700;
    const std::int64_t second_ts =
        first_ts + FX_HISTORY_MAX_BARS_PER_REQUEST * time_shield::SEC_PER_MIN;

    LocalBarHistoryServer server({
        fxhis_response(first_ts, 0.56880),
        fxhis_response(second_ts, 0.57000)
    });
    ASSERT_TRUE(server.start());

    const BarHistoryRequest request(
        "NZD/USD",
        time_shield::SEC_PER_MIN,
        first_ts,
        second_ts);

    const auto call = request_bar_history(server, request);

    ASSERT_TRUE(call.callback_received);
    ASSERT_EQ(call.callback_count, 1);
    ASSERT_TRUE(call.result);
    EXPECT_EQ(server.fxhis_requests.load(), 2);
    ASSERT_EQ(call.result.value.bars.size(), 2u);
    EXPECT_EQ(call.result.value.symbol, "NZDUSD");
    EXPECT_EQ(call.result.value.price_source, BarPriceSource::MID);
    EXPECT_EQ(call.result.value.bars[0].time_ms, time_shield::sec_to_ms(first_ts));
    EXPECT_EQ(call.result.value.bars[1].time_ms, time_shield::sec_to_ms(second_ts));
}

TEST(IntradeBarApiResponses, CombinedTradeHistoryRequestReportsHtmlFailureStatus) {
    LocalTradeHistoryServer server(
        200,
        valid_trade_history_csv(),
        451,
        "blocked");
    ASSERT_TRUE(server.start());

    const auto call = request_trade_history(server);

    ASSERT_TRUE(call.callback_received);
    ASSERT_EQ(call.callback_count, 1);
    EXPECT_FALSE(call.result);
    EXPECT_EQ(call.result.status_code, 451);
    EXPECT_NE(
        call.result.error_message.find("HTML trade history"),
        std::string::npos);
    EXPECT_NE(
        call.result.error_message.find("failed validation"),
        std::string::npos);
    EXPECT_EQ(server.csv_requests.load(), 1);
    EXPECT_EQ(server.html_requests.load(), 1);
}

TEST(IntradeBarApiResponses, CombinedTradeHistoryRequestReportsCsvFailureStatus) {
    LocalTradeHistoryServer server(
        500,
        "server error",
        200,
        empty_trade_history_html());
    ASSERT_TRUE(server.start());

    const auto call = request_trade_history(server);

    ASSERT_TRUE(call.callback_received);
    ASSERT_EQ(call.callback_count, 1);
    EXPECT_FALSE(call.result);
    EXPECT_EQ(call.result.status_code, 500);
    EXPECT_NE(
        call.result.error_message.find("CSV trade history"),
        std::string::npos);
    EXPECT_NE(
        call.result.error_message.find("failed validation"),
        std::string::npos);
    EXPECT_EQ(server.csv_requests.load(), 1);
    EXPECT_EQ(server.html_requests.load(), 1);
}

TEST(IntradeBarApiResponses, CombinedTradeHistoryRequestReportsBothFailureMessages) {
    LocalTradeHistoryServer server(
        500,
        "csv failed",
        451,
        "blocked");
    ASSERT_TRUE(server.start());

    const auto call = request_trade_history(server);

    ASSERT_TRUE(call.callback_received);
    ASSERT_EQ(call.callback_count, 1);
    EXPECT_FALSE(call.result);
    EXPECT_EQ(call.result.status_code, 500);
    EXPECT_NE(
        call.result.error_message.find("CSV trade history"),
        std::string::npos);
    EXPECT_NE(
        call.result.error_message.find("HTML trade history"),
        std::string::npos);
    EXPECT_NE(
        call.result.error_message.find("failed validation"),
        std::string::npos);
    EXPECT_EQ(server.csv_requests.load(), 1);
    EXPECT_EQ(server.html_requests.load(), 1);
}

TEST(IntradeBarApiResponses, CsvTradeHistoryRequestReportsDirectFailure) {
    LocalTradeHistoryServer server(
        500,
        "server error",
        200,
        empty_trade_history_html());
    ASSERT_TRUE(server.start());

    const auto call = request_trade_history(server, TradeHistorySource::CSV);

    ASSERT_TRUE(call.callback_received);
    ASSERT_EQ(call.callback_count, 1);
    EXPECT_FALSE(call.result);
    EXPECT_EQ(call.result.status_code, 500);
    EXPECT_EQ(
        call.result.error_message,
        "CSV trade history HTTP response failed validation.");
    EXPECT_EQ(server.csv_requests.load(), 1);
    EXPECT_EQ(server.html_requests.load(), 0);
}

TEST(IntradeBarApiResponses, HtmlTradeHistoryRequestReportsDirectFailure) {
    LocalTradeHistoryServer server(
        200,
        valid_trade_history_csv(),
        451,
        "blocked");
    ASSERT_TRUE(server.start());

    const auto call = request_trade_history(server, TradeHistorySource::HTML);

    ASSERT_TRUE(call.callback_received);
    ASSERT_EQ(call.callback_count, 1);
    EXPECT_FALSE(call.result);
    EXPECT_EQ(call.result.status_code, 451);
    EXPECT_EQ(
        call.result.error_message,
        "HTML trade history HTTP response failed validation.");
    EXPECT_EQ(server.csv_requests.load(), 0);
    EXPECT_EQ(server.html_requests.load(), 1);
}

TEST(IntradeBarApiResponses, TradeHistoryRequestRejectsInvalidRangeOrAccount) {
    TradeHistoryRequest invalid_range = TradeHistoryRequest::all();
    invalid_range.range_mode = TimeRangeMode::CLOSED;
    invalid_range.start_ms = 2000;
    invalid_range.stop_ms = 1000;

    const auto invalid_range_call = request_trade_history_without_http(
        invalid_range,
        AccountType::DEMO);

    ASSERT_TRUE(invalid_range_call.callback_received);
    ASSERT_EQ(invalid_range_call.callback_count, 1);
    EXPECT_FALSE(invalid_range_call.result);
    EXPECT_EQ(
        invalid_range_call.result.status_code,
        TradeHistoryApiResult::NO_RESPONSE_STATUS);
    EXPECT_EQ(
        invalid_range_call.result.error_message,
        "Invalid trade history request range or account type.");

    const auto unknown_account_call = request_trade_history_without_http(
        TradeHistoryRequest::all(),
        AccountType::UNKNOWN);

    ASSERT_TRUE(unknown_account_call.callback_received);
    ASSERT_EQ(unknown_account_call.callback_count, 1);
    EXPECT_FALSE(unknown_account_call.result);
    EXPECT_EQ(
        unknown_account_call.result.status_code,
        TradeHistoryApiResult::NO_RESPONSE_STATUS);
    EXPECT_EQ(
        unknown_account_call.result.error_message,
        "Invalid trade history request range or account type.");
}

TEST(IntradeBarApiResponses, HistoryRangeFilterExcludesRecordsWithoutSelectedTimestamp) {
    TradeHistoryRequest request;
    request.start_ms = 1000;
    request.stop_ms = 2000;
    request.range_mode = TimeRangeMode::CLOSED;
    request.time_field = TradeRecordTimeField::PLACE_DATE;

    TradeRecord record;
    record.close_date = 1500;

    auto filtered = filter_trade_history_range({record}, request);
    EXPECT_TRUE(filtered.empty());

    request.time_field = TradeRecordTimeField::CLOSE_DATE;
    filtered = filter_trade_history_range({record}, request);
    ASSERT_EQ(filtered.size(), 1u);
    EXPECT_EQ(filtered[0].close_date, 1500);
}

TEST(IntradeBarApiResponses, ParsesTradeHistoryCsvExport) {
    const std::string csv =
        "id;Type;Asset;Direction;Open;Close;Open quote;Close quote;Amount;Result\n"
        ";Sprint;EUR/GBP;Down;14:44:43, 14 Dec 20;14:47:43, 14 Dec 20;0.90492;0.90512;1 USD;1.82 USD\n"
        "123;Sprint;AUD/NZD;Up;20:52:19, 28 Jun 21;20:55:19, 28 Jun 21;1.07411;1.07417;500 USD;0 USD\n"
        ";Sprint;AUD/CAD;Up;16:34:33, 23 Jun 21;16:37:33, 23 Jun 21;0.93034;0.93008;50 RUB;50 RUB\n"
        ";Sprint;BTCUSD;Up;16:34:33, 23 Jun 21;16:39:33, 23 Jun 21;62830.01;62850.00;1 USD;1.79 USD\n"
        ";Sprint;EUR/USD;Up;16:34:33, 23 Jun 21;16:37:33, 23 Jun 21;1.16001;1.16002;100 \xE2\x82\xBD;180 \xE2\x82\xBD\n";

    const auto trades = parse_trade_history_csv_export(csv, AccountType::DEMO);
    ASSERT_EQ(trades.size(), 5u);

    EXPECT_EQ(trades[0].symbol, "EURGBP");
    EXPECT_EQ(trades[0].option_type, OptionType::SPRINT);
    EXPECT_EQ(trades[0].order_type, OrderType::SELL);
    EXPECT_EQ(trades[0].trade_state, TradeState::WIN);
    EXPECT_EQ(trades[0].live_state, TradeState::WIN);
    EXPECT_DOUBLE_EQ(trades[0].amount, 1.0);
    EXPECT_DOUBLE_EQ(trades[0].profit, 0.82);
    EXPECT_DOUBLE_EQ(trades[0].payout, 0.82);
    EXPECT_EQ(trades[0].currency, CurrencyType::USD);
    EXPECT_EQ(trades[0].account_type, AccountType::DEMO);
    EXPECT_EQ(trades[0].platform_type, PlatformType::INTRADE_BAR);
    EXPECT_EQ(trades[0].duration, 180);
    EXPECT_EQ(trades[0].spread.digits, 5);
    EXPECT_DOUBLE_EQ(trades[0].spread.open_spread(), 0.0);
    EXPECT_DOUBLE_EQ(trades[0].spread.close_spread(), 0.0);

    EXPECT_EQ(trades[1].option_id, 123);
    EXPECT_EQ(trades[1].symbol, "AUDNZD");
    EXPECT_EQ(trades[1].order_type, OrderType::BUY);
    EXPECT_EQ(trades[1].trade_state, TradeState::LOSS);
    EXPECT_DOUBLE_EQ(trades[1].profit, -500.0);

    EXPECT_EQ(trades[2].symbol, "AUDCAD");
    EXPECT_EQ(trades[2].currency, CurrencyType::RUB);
    EXPECT_EQ(trades[2].trade_state, TradeState::STANDOFF);
    EXPECT_DOUBLE_EQ(trades[2].profit, 0.0);

    EXPECT_EQ(trades[3].symbol, "BTCUSDT");
    EXPECT_EQ(trades[3].trade_state, TradeState::WIN);
    EXPECT_EQ(trades[3].duration, 300);
    EXPECT_EQ(trades[3].spread.digits, 2);

    EXPECT_EQ(trades[4].symbol, "EURUSD");
    EXPECT_EQ(trades[4].currency, CurrencyType::RUB);
    EXPECT_EQ(trades[4].trade_state, TradeState::WIN);
    EXPECT_DOUBLE_EQ(trades[4].profit, 80.0);
    EXPECT_EQ(trades[4].spread.digits, 5);
}

TEST(IntradeBarApiResponses, ParsesTradeHistoryHtmlSnapshotAndMergesWithCsv) {
    const std::string html = R"HTML(
        <tbody class="table_tbody" id="trade_history">
            <tr id="trade_inv_224157357" data-id="224157357" data-option="BTCUSDT" data-rate="62830.01" data-timeopen="1719146073" data-timeclose="1719146373" data-status="1" data-contract="0">
            </tr>
            <tr id="trade_inv_224157358" data-id="224157358" data-option="EUR/USD" data-rate="1.07001" data-timeopen="1719146074" data-status="2" data-contract="1">
            </tr>
        </tbody>
    )HTML";

    const auto html_trades = parse_trade_history_html_snapshot(html, AccountType::DEMO);
    ASSERT_EQ(html_trades.size(), 2u);
    EXPECT_EQ(html_trades[0].option_id, 224157357);
    EXPECT_EQ(html_trades[0].symbol, "BTCUSDT");
    EXPECT_EQ(html_trades[0].order_type, OrderType::BUY);
    EXPECT_EQ(html_trades[0].option_type, OptionType::SPRINT);
    EXPECT_EQ(html_trades[0].open_date, time_shield::sec_to_ms(1719146073));
    EXPECT_EQ(html_trades[0].close_date, time_shield::sec_to_ms(1719146373));
    EXPECT_EQ(html_trades[0].duration, 300);
    EXPECT_EQ(html_trades[0].spread.digits, 2);
    EXPECT_DOUBLE_EQ(html_trades[0].spread.open_spread(), 0.0);
    EXPECT_DOUBLE_EQ(html_trades[0].spread.close_spread(), 0.0);

    EXPECT_EQ(html_trades[1].symbol, "EURUSD");
    EXPECT_EQ(html_trades[1].spread.digits, 5);

    std::vector<TradeRecord> csv_trades;
    TradeRecord csv_trade;
    csv_trade.symbol = "BTCUSDT";
    csv_trade.open_date = time_shield::sec_to_ms(1719146074);
    csv_trade.open_price = 62830.01;
    csv_trade.amount = 1.0;
    csv_trade.trade_state = TradeState::WIN;
    csv_trades.push_back(csv_trade);
    TradeRecord csv_only_trade;
    csv_only_trade.symbol = "GBPUSD";
    csv_only_trade.open_date = time_shield::sec_to_ms(1719146074);
    csv_only_trade.open_price = 1.25001;
    csv_only_trade.amount = 1.0;
    csv_trades.push_back(csv_only_trade);

    const auto merged = merge_trade_history_csv_with_html(
        std::move(csv_trades),
        html_trades);
    ASSERT_EQ(merged.size(), 1u);
    EXPECT_EQ(merged[0].option_id, 224157357);
    EXPECT_EQ(merged[0].symbol, "BTCUSDT");
    EXPECT_EQ(merged[0].duration, 300);
}

TEST(IntradeBarApiResponses, DoesNotFuzzyMatchDifferentBrokerIds) {
    std::vector<TradeRecord> csv_trades;
    TradeRecord first_csv;
    first_csv.option_id = 1;
    first_csv.symbol = "BTCUSDT";
    first_csv.open_date = time_shield::sec_to_ms(1000);
    first_csv.open_price = 64006.0;
    csv_trades.push_back(first_csv);

    TradeRecord second_csv;
    second_csv.option_id = 2;
    second_csv.symbol = "BTCUSDT";
    second_csv.open_date = time_shield::sec_to_ms(1004);
    second_csv.open_price = 64006.0;
    csv_trades.push_back(second_csv);

    std::vector<TradeRecord> html_trades;
    TradeRecord second_html;
    second_html.option_id = 2;
    second_html.symbol = "BTCUSDT";
    second_html.open_date = time_shield::sec_to_ms(1004);
    second_html.open_price = 64006.0;
    second_html.duration = 300;
    html_trades.push_back(second_html);

    TradeRecord first_html;
    first_html.option_id = 1;
    first_html.symbol = "BTCUSDT";
    first_html.open_date = time_shield::sec_to_ms(1000);
    first_html.open_price = 64006.0;
    first_html.duration = 300;
    html_trades.push_back(first_html);

    const auto merged = merge_trade_history_csv_with_html(
        std::move(csv_trades),
        html_trades);
    ASSERT_EQ(merged.size(), 2u);
    EXPECT_EQ(merged[0].option_id, 1);
    EXPECT_EQ(merged[1].option_id, 2);
}

TEST(IntradeBarApiResponses, ParsesTradeCloseHtmlPageAndNextCursor) {
    const std::string html = R"HTML(
        <div id="trade_close_block" class="hide">
            <table class="">
                <tbody class="table_tbody" id="trade_close">
                    <tr class="trade_list_type trade_list_type_1" >
                        <th class="center"><div class="trading-table__up-td"></div></th>
                        <th>
                            224157357
                            <br>
                            19:06:42, 22.06.26
                            <br>
                            19:11:42, 22.06.26
                        </th>
                        <th>
                            BTC/USDT
                            <br>
                            64708.01
                            <br>
                            64735.64
                        </th>
                        <th>
                            <br>
                            1 $
                            <br>
                            1.79 $
                        </th>
                    </tr>
                    <tr class="trade_list_type trade_list_type_1" >
                        <th class="center"><div class="trading-table__down-td"></div></th>
                        <th>
                            224130715
                            <br>
                            09:57:43, 19.06.26
                            <br>
                            10:03:43, 19.06.26
                        </th>
                        <th>
                            BTC/USDT
                            <br>
                            62884.8
                            <br>
                            62854.73
                        </th>
                        <th>
                            <br>
                            1 $
                            <br>
                            1.79 $
                        </th>
                    </tr>
                    <tr class="trade_list_type trade_list_type_1" >
                        <th class="center"><div class="trading-table__up-td"></div></th>
                        <th>
                            224130651
                            <br>
                            09:29:34, 19.06.26
                            <br>
                            09:34:34, 19.06.26
                        </th>
                        <th>
                            BTC/USDT
                            <br>
                            62830.01
                            <br>
                            62825.99
                        </th>
                        <th>
                            <br>
                            1 $
                            <br>
                            0 $
                        </th>
                    </tr>
                </tbody>
            </table>
            <div class="text-center">
                <a class="trading-tables__btn btn btn--gray trade_btn_load_more" id="trade_btn_load_more" data-last="224130496">load more</a>
            </div>
        </div>
    )HTML";

    const auto page = parse_trade_history_html_page(html, AccountType::DEMO);
    ASSERT_EQ(page.records.size(), 3u);
    EXPECT_EQ(page.next_last, "224130496");

    EXPECT_EQ(page.records[0].option_id, 224157357);
    EXPECT_EQ(page.records[0].symbol, "BTCUSDT");
    EXPECT_EQ(page.records[0].order_type, OrderType::BUY);
    EXPECT_EQ(page.records[0].trade_state, TradeState::WIN);
    EXPECT_EQ(page.records[0].duration, 300);
    EXPECT_DOUBLE_EQ(page.records[0].amount, 1.0);
    EXPECT_DOUBLE_EQ(page.records[0].profit, 0.79);
    EXPECT_DOUBLE_EQ(page.records[0].payout, 0.79);
    EXPECT_EQ(page.records[0].currency, CurrencyType::USD);

    EXPECT_EQ(page.records[1].option_id, 224130715);
    EXPECT_EQ(page.records[1].order_type, OrderType::SELL);
    EXPECT_EQ(page.records[1].duration, 360);
    EXPECT_EQ(page.records[1].trade_state, TradeState::WIN);

    EXPECT_EQ(page.records[2].option_id, 224130651);
    EXPECT_EQ(page.records[2].order_type, OrderType::BUY);
    EXPECT_EQ(page.records[2].trade_state, TradeState::LOSS);
    EXPECT_DOUBLE_EQ(page.records[2].profit, -1.0);
}

TEST(IntradeBarApiResponses, ParsesTradeLoadMoreRowsWithoutTableWrapper) {
    const std::string html = R"HTML(
        <tr class="trade_list_type trade_list_type_1" >
            <th class="center"><div class="trading-table__up-td"></div></th>
            <th>
                224130496
                <br>
                08:02:55, 19.06.26
                <br>
                08:07:55, 19.06.26
            </th>
            <th>
                BTC/USDT
                <br>
                62577.98
                <br>
                62615.99
            </th>
            <th>
                <br>
                1 $
                <br>
                1.6 $
            </th>
        </tr>
        <script>
            $('#trade_btn_load_more').attr('data-last', '')
        </script>
    )HTML";

    const auto page = parse_trade_history_html_page(html, AccountType::DEMO);
    ASSERT_EQ(page.records.size(), 1u);
    EXPECT_TRUE(page.next_last.empty());
    EXPECT_EQ(page.records[0].option_id, 224130496);
    EXPECT_EQ(page.records[0].trade_state, TradeState::WIN);
    EXPECT_DOUBLE_EQ(page.records[0].profit, 0.6);
}

TEST(IntradeBarApiResponses, ParsesActiveTradesAndLatestCloseTime) {
    const std::string html = R"HTML(
        <tbody class="table_tbody" id="trade_active">
            <tr id="trade_inv_224130651" data-id="224130651" data-option="BTCUSDT" data-rate="62830.01" data-timeopen="1781850574" data-status="1" data-contract="0">
                <script async>
                    time_time_224130651 = 283;
                    timer224130651 = setInterval(showRemaining, 1000, "timer_224130651", window.time_time_224130651, 224130651,'1', '1781850874');
                </script>
            </tr>
            <tr id="trade_inv_224130777" data-id="224130777" data-option="BTCUSDT" data-rate="62831.25" data-timeopen="1781850580" data-status="2" data-contract="0">
                <script async>
                    time_time_224130777 = 343;
                    timer224130777 = setInterval(showRemaining, 1000, "timer_224130777", window.time_time_224130777, 224130777,'2', '1781850940');
                </script>
            </tr>
        </tbody>
    )HTML";

    const auto trades = parse_active_trades_snapshot(html);
    ASSERT_EQ(trades.size(), 2u);

    EXPECT_EQ(trades[0].id, 224130651);
    EXPECT_EQ(trades[0].symbol, "BTCUSDT");
    EXPECT_DOUBLE_EQ(trades[0].open_price, 62830.01);
    EXPECT_EQ(trades[0].open_time_ms, time_shield::sec_to_ms(1781850574));
    EXPECT_EQ(trades[0].close_time_ms, time_shield::sec_to_ms(1781850874));
    EXPECT_EQ(trades[0].status, 1);
    EXPECT_EQ(trades[0].contract, 0);

    int64_t latest_close_ms = 0;
    for (const auto& trade : trades) {
        if (trade.close_time_ms > latest_close_ms) {
            latest_close_ms = trade.close_time_ms;
        }
    }
    EXPECT_EQ(latest_close_ms, time_shield::sec_to_ms(1781850940));
}

TEST(IntradeBarApiResponses, ParsesActiveTradeRowsWithFlexibleTrAttributes) {
    const std::string html = R"HTML(
        <tbody class="table_tbody" id="trade_active">
            <tr class="trade_graph_tick" data-id = "224130651" id="trade_inv_224130651" data-option = "BTCUSDT" data-rate = "62830.01" data-timeopen = "1781850574" data-status = "1" data-contract = "0">
            </tr>
            <tr  id='trade_inv_224130777' data-id='224130777' data-option='BTCUSDT' data-rate='62831.25' data-timeopen='1781850580' data-status='2' data-contract='0'>
            </tr>
            <tr class="ignored" id="not_trade_inv_224130999" data-id="224130999">
            </tr>
        </tbody>
    )HTML";

    const auto trades = parse_active_trades_snapshot(html);
    ASSERT_EQ(trades.size(), 2u);

    EXPECT_EQ(trades[0].id, 224130651);
    EXPECT_EQ(trades[0].symbol, "BTCUSDT");
    EXPECT_DOUBLE_EQ(trades[0].open_price, 62830.01);
    EXPECT_EQ(trades[0].status, 1);

    EXPECT_EQ(trades[1].id, 224130777);
    EXPECT_EQ(trades[1].symbol, "BTCUSDT");
    EXPECT_DOUBLE_EQ(trades[1].open_price, 62831.25);
    EXPECT_EQ(trades[1].status, 2);
}

TEST(IntradeBarApiResponses, RejectsActiveTradesPageWithoutActiveBlock) {
    const std::string html = R"HTML(
        <tbody class="table_tbody" id="trade_history">
            <tr id="trade_inv_224130999" data-id="224130999" data-option="BTCUSDT" data-rate="62830.01" data-timeopen="1781850574">
                <script async>
                    timer224130999 = setInterval(showRemaining, 1000, "timer_224130999", 1, 224130999,'1', '1781850874');
                </script>
            </tr>
        </tbody>
    )HTML";

    EXPECT_THROW(
        static_cast<void>(parse_active_trades_snapshot(html)),
        std::runtime_error);
}

TEST(IntradeBarApiResponses, IgnoresTradeRowsOutsideActiveBlock) {
    const std::string html = R"HTML(
        <tbody class="table_tbody" id="trade_history">
            <tr id="trade_inv_224130999" data-id="224130999" data-option="BTCUSDT" data-rate="62830.01" data-timeopen="1781850574">
                <script async>
                    timer224130999 = setInterval(showRemaining, 1000, "timer_224130999", 1, 224130999,'1', '1781850874');
                </script>
            </tr>
        </tbody>
        <tbody class="table_tbody" id="trade_active">
        </tbody>
    )HTML";

    const auto trades = parse_active_trades_snapshot(html);
    EXPECT_TRUE(trades.empty());
}

TEST(IntradeBarApiResponses, SkipsActiveTradeRowsWithMalformedId) {
    const std::string html = R"HTML(
        <tbody class="table_tbody" id="trade_active">
            <tr id="trade_inv_224130651" data-id="224130651junk" data-option="BTCUSDT" data-rate="62830.01" data-timeopen="1781850574" data-status="1" data-contract="0">
            </tr>
        </tbody>
    )HTML";

    const auto trades = parse_active_trades_snapshot(html);
    EXPECT_TRUE(trades.empty());
}

TEST(IntradeBarApiResponses, RejectsPartialNumericActiveTradeFields) {
    const std::string html = R"HTML(
        <tbody class="table_tbody" id="trade_active">
            <tr id="trade_inv_224130651" data-id="224130651" data-option="BTCUSDT" data-rate="62830.01junk" data-timeopen="1781850574x" data-status="1x" data-contract="0x">
                <script async>
                    time_time_224130651 = 283x;
                </script>
            </tr>
        </tbody>
    )HTML";

    const auto trades = parse_active_trades_snapshot(html);
    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].id, 224130651);
    EXPECT_EQ(trades[0].symbol, "BTCUSDT");
    EXPECT_DOUBLE_EQ(trades[0].open_price, 0.0);
    EXPECT_EQ(trades[0].open_time_ms, 0);
    EXPECT_EQ(trades[0].status, 0);
    EXPECT_EQ(trades[0].contract, 0);
    EXPECT_EQ(trades[0].close_time_ms, 0);
}

TEST(IntradeBarApiResponses, EmptyTradeOpenResponseHasSpecificError) {
    bool called = false;
    bool success = true;
    long status_code = 0;
    std::string error_desc;

    parse_execute_trade(
        "",
        200,
        [&](bool parsed,
            long status,
            int64_t,
            int64_t,
            double,
            const std::string& error) {
            called = true;
            success = parsed;
            status_code = status;
            error_desc = error;
        });

    EXPECT_TRUE(called);
    EXPECT_FALSE(success);
    EXPECT_EQ(status_code, 200);
    EXPECT_EQ(
        error_desc,
        "Trade open failed. Server returned an empty response; instrument may be closed or unavailable.");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
