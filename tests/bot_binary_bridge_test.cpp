#include <gtest/gtest.h>

#include <optionx_cpp/bridges.hpp>

#include <asio.hpp>
#include <client_http.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

using HttpClient = SimpleWeb::Client<SimpleWeb::HTTP>;

optionx::bridges::bot_binary::BotBinaryBridgeConfig test_config() {
    optionx::bridges::bot_binary::BotBinaryBridgeConfig config;
    config.address = "127.0.0.1";
    config.port = 0;
    config.bridge_id = 7;
    config.signal_name = "bot_binary_test";
    config.enable_http = true;
    config.enable_file_signal = false;
    config.poll_interval_ms = 10;
    return config;
}

bool wait_for_http_port(const optionx::bridges::bot_binary::BotBinaryBridge& bridge) {
    for (int i = 0; i < 300; ++i) {
        if (bridge.bound_http_port() != 0) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

bool wait_for_count(const std::atomic<int>& count, const int expected) {
    for (int i = 0; i < 300; ++i) {
        if (count.load() >= expected) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

bool wait_for_flag(const std::atomic<bool>& flag) {
    for (int i = 0; i < 300; ++i) {
        if (flag.load()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

std::string request_path(
        const optionx::bridges::bot_binary::BotBinaryBridgeConfig& config,
        const std::string& raw_value) {
    return config.http_path +
           "?request=" +
           optionx::bridges::bot_binary::detail::percent_encode_query_value(raw_value);
}

std::filesystem::path unique_temp_dir(const std::string& name) {
    const auto stamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    return std::filesystem::temp_directory_path() /
           ("optionx_" + name + "_" + std::to_string(stamp));
}

class TempDir final {
public:
    explicit TempDir(std::string name)
        : path(unique_temp_dir(std::move(name))) {
        std::filesystem::create_directories(path);
    }

    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }

    std::filesystem::path path;
};

} // namespace

TEST(BotBinaryBridge, ConfigRoundTripsAndExposesBridgeType) {
    namespace bot = optionx::bridges::bot_binary;

    bot::BotBinaryBridgeConfig config;
    config.bridge_id = 7;
    config.enable_http = false;
    config.enable_file_signal = true;
    config.file_signal_dir = "C:/tmp/Signal";
    config.symbol_map["R_25"] = "VOLATILITY_25";

    nlohmann::json json;
    config.to_json(json);

    bot::BotBinaryBridgeConfig restored;
    restored.from_json(json);

    EXPECT_TRUE(restored.validate().first);
    EXPECT_EQ(restored.bridge_type(), optionx::BridgeType::BOT_BINARY);
    EXPECT_EQ(restored.file_signal_dir, "C:/tmp/Signal");
    EXPECT_EQ(restored.symbol_map.at("R_25"), "VOLATILITY_25");
}

TEST(BotBinaryBridge, ConfigRejectsRemoteHttpBindUnlessExplicitlyAllowed) {
    namespace bot = optionx::bridges::bot_binary;

    auto config = test_config();
    config.address = "0.0.0.0";

    auto validation = config.validate();
    EXPECT_FALSE(validation.first);
    EXPECT_NE(
        validation.second.find("allow_insecure_remote"),
        std::string::npos);

    config.allow_insecure_remote = true;
    validation = config.validate();
    EXPECT_TRUE(validation.first);
}

TEST(BotBinaryBridge, RunRequiresTradeSignalCallback) {
    namespace bot = optionx::bridges::bot_binary;

    auto config = test_config();
    bot::BotBinaryBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<bot::BotBinaryBridgeConfig>(config)));

    std::atomic<bool> start_failed{false};
    bridge.on_signal_id() = []() { return optionx::SignalId{10}; };
    bridge.on_status_update() = [&start_failed](const optionx::BridgeStatusUpdate& update) {
        if (update.status == optionx::BridgeStatus::SERVER_START_FAILED) {
            start_failed.store(true);
        }
    };

    bridge.run();
    EXPECT_TRUE(wait_for_flag(start_failed));
    EXPECT_EQ(bridge.bound_http_port(), 0);
}

TEST(BotBinaryBridge, ReceivesHttpRequestQuery) {
    namespace bot = optionx::bridges::bot_binary;

    auto config = test_config();
    config.symbol_map["R_25"] = "VOLATILITY_25";

    bot::BotBinaryBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<bot::BotBinaryBridgeConfig>(config)));

    std::atomic<optionx::SignalId> next_signal_id{100};
    std::atomic<int> signal_count{0};
    std::unique_ptr<optionx::TradeSignal> dispatched;

    bridge.on_signal_id() = [&next_signal_id]() {
        return next_signal_id.fetch_add(1);
    };
    bridge.on_trade_signal() = [&](std::unique_ptr<optionx::TradeSignal> signal) {
        dispatched = signal->clone();
        ++signal_count;
    };

    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));

    HttpClient client(config.address + ":" + std::to_string(bridge.bound_http_port()));
    const auto response = client.request(
        "GET",
        request_path(config, "R_25=CALL=1.00=duration=1=m=legacy-http"));
    const auto body = nlohmann::json::parse(response->content.string());
    bridge.shutdown();

    EXPECT_TRUE(body.at("ok").get<bool>());
    EXPECT_TRUE(body.at("accepted").get<bool>());
    EXPECT_TRUE(wait_for_count(signal_count, 1));
    ASSERT_TRUE(dispatched);
    EXPECT_EQ(dispatched->signal_id, 100);
    EXPECT_EQ(dispatched->bridge_id, config.bridge_id);
    EXPECT_EQ(dispatched->symbol, "VOLATILITY_25");
    EXPECT_EQ(dispatched->signal_name, "bot_binary_test");
    EXPECT_EQ(dispatched->order_type, optionx::OrderType::BUY);
    EXPECT_EQ(dispatched->option_type, optionx::OptionType::SPRINT);
    EXPECT_DOUBLE_EQ(dispatched->amount, 1.0);
    EXPECT_EQ(dispatched->duration, 60u);
}

TEST(BotBinaryBridge, RejectsDuplicateHttpTransportKey) {
    namespace bot = optionx::bridges::bot_binary;

    auto config = test_config();

    bot::BotBinaryBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<bot::BotBinaryBridgeConfig>(config)));

    std::atomic<optionx::SignalId> next_signal_id{200};
    std::atomic<int> signal_count{0};
    bridge.on_signal_id() = [&next_signal_id]() {
        return next_signal_id.fetch_add(1);
    };
    bridge.on_trade_signal() = [&signal_count](std::unique_ptr<optionx::TradeSignal>) {
        ++signal_count;
    };

    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));

    HttpClient client(config.address + ":" + std::to_string(bridge.bound_http_port()));
    const auto path = request_path(config, "R_25=PUT=1=duration=1=m=legacy-dup");
    const auto first = nlohmann::json::parse(
        client.request("GET", path)->content.string());
    const auto second = nlohmann::json::parse(
        client.request("GET", path)->content.string());
    bridge.shutdown();

    EXPECT_TRUE(first.at("accepted").get<bool>());
    EXPECT_FALSE(second.at("accepted").get<bool>());
    EXPECT_TRUE(second.at("duplicate").get<bool>());
    EXPECT_EQ(signal_count.load(), 1);
}

TEST(BotBinaryBridge, FailedCallbackDoesNotCorruptDedupeRetry) {
    namespace bot = optionx::bridges::bot_binary;

    auto config = test_config();
    config.dedupe_cache_size = 1;

    bot::BotBinaryBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<bot::BotBinaryBridgeConfig>(config)));

    std::atomic<optionx::SignalId> next_signal_id{225};
    std::atomic<int> callback_calls{0};
    bridge.on_signal_id() = [&next_signal_id]() {
        return next_signal_id.fetch_add(1);
    };
    bridge.on_trade_signal() = [&callback_calls](std::unique_ptr<optionx::TradeSignal>) {
        if (callback_calls.fetch_add(1) == 0) {
            throw std::runtime_error("synthetic dispatch failure");
        }
    };

    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));

    HttpClient client(config.address + ":" + std::to_string(bridge.bound_http_port()));
    const auto path = request_path(config, "R_25=CALL=1=duration=1=m=retry-after-fail");
    const auto first = nlohmann::json::parse(
        client.request("GET", path)->content.string());
    const auto second = nlohmann::json::parse(
        client.request("GET", path)->content.string());
    const auto third = nlohmann::json::parse(
        client.request("GET", path)->content.string());
    bridge.shutdown();

    EXPECT_FALSE(first.at("accepted").get<bool>());
    EXPECT_EQ(first.at("reason").get<std::string>(), "trade_signal_callback_failed");
    EXPECT_TRUE(second.at("accepted").get<bool>());
    EXPECT_FALSE(third.at("accepted").get<bool>());
    EXPECT_TRUE(third.at("duplicate").get<bool>());
    EXPECT_EQ(callback_calls.load(), 2);
}

TEST(BotBinaryBridge, ThrowingSignalIdAllocatorDoesNotReserveDedupeKey) {
    namespace bot = optionx::bridges::bot_binary;

    auto config = test_config();
    config.dedupe_cache_size = 1;

    bot::BotBinaryBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<bot::BotBinaryBridgeConfig>(config)));

    std::atomic<optionx::SignalId> next_signal_id{240};
    std::atomic<int> allocator_calls{0};
    std::atomic<int> signal_count{0};
    bridge.on_signal_id() = [&]() {
        if (allocator_calls.fetch_add(1) == 0) {
            throw std::runtime_error("synthetic allocator failure");
        }
        return next_signal_id.fetch_add(1);
    };
    bridge.on_trade_signal() = [&signal_count](std::unique_ptr<optionx::TradeSignal>) {
        ++signal_count;
    };

    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));

    HttpClient client(config.address + ":" + std::to_string(bridge.bound_http_port()));
    const auto path = request_path(config, "R_25=CALL=1=duration=1=m=allocator-retry");
    const auto first = nlohmann::json::parse(
        client.request("GET", path)->content.string());
    const auto second = nlohmann::json::parse(
        client.request("GET", path)->content.string());
    const auto third = nlohmann::json::parse(
        client.request("GET", path)->content.string());
    bridge.shutdown();

    EXPECT_FALSE(first.at("accepted").get<bool>());
    EXPECT_EQ(first.at("reason").get<std::string>(), "signal_id_allocator_failed");
    EXPECT_TRUE(second.at("accepted").get<bool>());
    EXPECT_FALSE(third.at("accepted").get<bool>());
    EXPECT_TRUE(third.at("duplicate").get<bool>());
    EXPECT_EQ(signal_count.load(), 1);
    EXPECT_EQ(allocator_calls.load(), 2);
}

TEST(BotBinaryBridge, ShutdownFromSignalIdAllocatorDoesNotBreakHttpResponse) {
    namespace bot = optionx::bridges::bot_binary;

    auto config = test_config();

    bot::BotBinaryBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<bot::BotBinaryBridgeConfig>(config)));

    std::atomic<int> signal_count{0};
    bridge.on_signal_id() = [&bridge]() {
        bridge.shutdown();
        return optionx::SignalId{255};
    };
    bridge.on_trade_signal() = [&signal_count](std::unique_ptr<optionx::TradeSignal>) {
        ++signal_count;
    };

    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));

    HttpClient client(config.address + ":" + std::to_string(bridge.bound_http_port()));
    const auto response = client.request(
        "GET",
        request_path(config, "R_25=CALL=1=duration=1=m=allocator-shutdown"));
    const auto body = nlohmann::json::parse(response->content.string());

    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));
    bridge.shutdown();

    EXPECT_TRUE(body.at("accepted").get<bool>());
    EXPECT_EQ(body.at("signal_id").get<optionx::SignalId>(), 255);
    EXPECT_EQ(signal_count.load(), 1);
}

TEST(BotBinaryBridge, MissingCallbackDoesNotReserveDedupeKey) {
    namespace bot = optionx::bridges::bot_binary;

    auto config = test_config();
    bot::BotBinaryBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<bot::BotBinaryBridgeConfig>(config)));

    std::atomic<optionx::SignalId> next_signal_id{250};
    std::atomic<int> signal_count{0};
    bridge.on_signal_id() = [&next_signal_id]() {
        return next_signal_id.fetch_add(1);
    };
    bridge.on_trade_signal() = [&signal_count](std::unique_ptr<optionx::TradeSignal>) {
        ++signal_count;
    };

    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));

    HttpClient client(config.address + ":" + std::to_string(bridge.bound_http_port()));
    const auto path = request_path(config, "R_25=CALL=1=duration=1=m=callback-required");

    bridge.on_trade_signal() = {};
    const auto first = nlohmann::json::parse(
        client.request("GET", path)->content.string());

    bridge.on_trade_signal() = [&signal_count](std::unique_ptr<optionx::TradeSignal>) {
        ++signal_count;
    };
    const auto second = nlohmann::json::parse(
        client.request("GET", path)->content.string());
    const auto third = nlohmann::json::parse(
        client.request("GET", path)->content.string());
    bridge.shutdown();

    EXPECT_FALSE(first.at("accepted").get<bool>());
    EXPECT_EQ(first.at("reason").get<std::string>(), "missing_trade_signal_callback");
    EXPECT_TRUE(second.at("accepted").get<bool>());
    EXPECT_FALSE(third.at("accepted").get<bool>());
    EXPECT_TRUE(third.at("duplicate").get<bool>());
    EXPECT_EQ(signal_count.load(), 1);
}

TEST(BotBinaryBridge, PollsFileSignalAndDeletesProcessedFile) {
    namespace bot = optionx::bridges::bot_binary;

    TempDir temp("bot_binary_file");
    auto config = test_config();
    config.enable_http = false;
    config.enable_file_signal = true;
    config.file_signal_dir = temp.path.u8string();
    config.delete_processed_files = true;

    bot::BotBinaryBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<bot::BotBinaryBridgeConfig>(config)));

    std::atomic<optionx::SignalId> next_signal_id{300};
    std::atomic<int> signal_count{0};
    std::unique_ptr<optionx::TradeSignal> dispatched;

    bridge.on_signal_id() = [&next_signal_id]() {
        return next_signal_id.fetch_add(1);
    };
    bridge.on_trade_signal() = [&](std::unique_ptr<optionx::TradeSignal> signal) {
        dispatched = signal->clone();
        ++signal_count;
    };

    bridge.run();

    const auto file_name = std::string("R_50=PUT=0.50=endtime=1538264736=s=file-signal.txt");
    const auto signal_file = temp.path / file_name;
    {
        std::ofstream output(signal_file);
        ASSERT_TRUE(output.good());
    }

    ASSERT_TRUE(wait_for_count(signal_count, 1));
    bridge.shutdown();

    EXPECT_FALSE(std::filesystem::exists(signal_file));
    ASSERT_TRUE(dispatched);
    EXPECT_EQ(dispatched->signal_id, 300);
    EXPECT_EQ(dispatched->symbol, "R_50");
    EXPECT_EQ(dispatched->order_type, optionx::OrderType::SELL);
    EXPECT_EQ(dispatched->option_type, optionx::OptionType::CLASSIC);
    EXPECT_DOUBLE_EQ(dispatched->amount, 0.50);
    EXPECT_EQ(dispatched->expiry_time, 1538264736);
}

TEST(BotBinaryBridge, ReceivesHttpAndFileSignalsTogether) {
    namespace bot = optionx::bridges::bot_binary;

    TempDir temp("bot_binary_both");
    auto config = test_config();
    config.enable_http = true;
    config.enable_file_signal = true;
    config.file_signal_dir = temp.path.u8string();
    config.delete_processed_files = true;

    bot::BotBinaryBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<bot::BotBinaryBridgeConfig>(config)));

    std::atomic<optionx::SignalId> next_signal_id{350};
    std::atomic<int> signal_count{0};
    std::atomic<int> status_count{0};
    std::mutex mutex;
    std::vector<std::string> symbols;

    bridge.on_signal_id() = [&next_signal_id]() {
        return next_signal_id.fetch_add(1);
    };
    bridge.on_status_update() = [&status_count](const optionx::BridgeStatusUpdate&) {
        ++status_count;
    };
    bridge.on_trade_signal() = [&](std::unique_ptr<optionx::TradeSignal> signal) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            symbols.push_back(signal->symbol);
        }
        ++signal_count;
    };

    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));

    HttpClient client(config.address + ":" + std::to_string(bridge.bound_http_port()));
    const auto response = client.request(
        "GET",
        request_path(config, "R_25=CALL=1=duration=1=m=both-http"));
    const auto body = nlohmann::json::parse(response->content.string());

    const auto file_name = std::string("R_50=PUT=1=duration=30=s=both-file.txt");
    const auto signal_file = temp.path / file_name;
    {
        std::ofstream output(signal_file);
        ASSERT_TRUE(output.good());
    }

    ASSERT_TRUE(wait_for_count(signal_count, 2));
    bridge.shutdown();

    EXPECT_TRUE(body.at("accepted").get<bool>());
    EXPECT_FALSE(std::filesystem::exists(signal_file));
    std::lock_guard<std::mutex> lock(mutex);
    EXPECT_EQ(symbols.size(), 2u);
    EXPECT_NE(
        std::find(symbols.begin(), symbols.end(), "R_25"),
        symbols.end());
    EXPECT_NE(
        std::find(symbols.begin(), symbols.end(), "R_50"),
        symbols.end());
    EXPECT_GE(status_count.load(), 2);
}

TEST(BotBinaryBridge, RejectsDuplicateAcrossHttpAndFileTransportKey) {
    namespace bot = optionx::bridges::bot_binary;

    TempDir temp("bot_binary_cross_transport_dup");
    auto config = test_config();
    config.enable_http = true;
    config.enable_file_signal = true;
    config.file_signal_dir = temp.path.u8string();
    config.delete_processed_files = true;

    bot::BotBinaryBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<bot::BotBinaryBridgeConfig>(config)));

    std::atomic<optionx::SignalId> next_signal_id{375};
    std::atomic<int> signal_count{0};
    std::atomic<int> duplicate_reports{0};

    bridge.on_signal_id() = [&next_signal_id]() {
        return next_signal_id.fetch_add(1);
    };
    bridge.on_trade_signal() = [&signal_count](std::unique_ptr<optionx::TradeSignal>) {
        ++signal_count;
    };
    bridge.on_signal_report() = [&duplicate_reports](const optionx::BridgeSignalReport& report) {
        if (report.status == optionx::BridgeSignalReportStatus::DUPLICATE) {
            ++duplicate_reports;
        }
    };

    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));

    HttpClient client(config.address + ":" + std::to_string(bridge.bound_http_port()));
    const auto response = client.request(
        "GET",
        request_path(config, "R_25=CALL=1=duration=1=m=cross-transport-key"));
    const auto body = nlohmann::json::parse(response->content.string());

    const auto file_name = std::string("R_50=PUT=1=duration=30=s=cross-transport-key.txt");
    const auto signal_file = temp.path / file_name;
    {
        std::ofstream output(signal_file);
        ASSERT_TRUE(output.good());
    }

    ASSERT_TRUE(wait_for_count(duplicate_reports, 1));
    bridge.shutdown();

    EXPECT_TRUE(body.at("accepted").get<bool>());
    EXPECT_EQ(signal_count.load(), 1);
    EXPECT_TRUE(std::filesystem::exists(signal_file));
}

TEST(BotBinaryBridge, ReportsInvalidHttpCommand) {
    namespace bot = optionx::bridges::bot_binary;

    auto config = test_config();

    bot::BotBinaryBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<bot::BotBinaryBridgeConfig>(config)));

    std::atomic<int> reports{0};
    bridge.on_signal_id() = []() { return optionx::SignalId{400}; };
    bridge.on_trade_signal() = [](std::unique_ptr<optionx::TradeSignal>) {};
    bridge.on_signal_report() = [&](const optionx::BridgeSignalReport& report) {
        EXPECT_EQ(report.status, optionx::BridgeSignalReportStatus::INVALID);
        EXPECT_EQ(report.reason_code, "invalid_bot_binary_command");
        ++reports;
    };

    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));

    HttpClient client(config.address + ":" + std::to_string(bridge.bound_http_port()));
    const auto response = client.request(
        "GET",
        request_path(config, "R_25=BUY=1=duration=1=m="));
    const auto body = nlohmann::json::parse(response->content.string());
    bridge.shutdown();

    EXPECT_FALSE(body.at("accepted").get<bool>());
    EXPECT_EQ(body.at("reason").get<std::string>(), "invalid_bot_binary_command");
    EXPECT_EQ(reports.load(), 1);
}

TEST(BotBinaryBridge, InvalidMappedBusinessSignalDoesNotReserveDedupeKey) {
    namespace bot = optionx::bridges::bot_binary;

    auto config = test_config();
    config.symbol_map["R_25"] = " \t ";

    bot::BotBinaryBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<bot::BotBinaryBridgeConfig>(config)));

    std::atomic<int> signal_count{0};
    std::atomic<int> invalid_reports{0};
    bridge.on_signal_id() = []() { return optionx::SignalId{410}; };
    bridge.on_trade_signal() = [&signal_count](std::unique_ptr<optionx::TradeSignal>) {
        ++signal_count;
    };
    bridge.on_signal_report() = [&](const optionx::BridgeSignalReport& report) {
        if (report.status == optionx::BridgeSignalReportStatus::INVALID) {
            EXPECT_EQ(report.reason_code, "invalid_trade_signal");
            ++invalid_reports;
        }
    };

    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));

    HttpClient client(config.address + ":" + std::to_string(bridge.bound_http_port()));
    constexpr const char* invalid_request = "R_25=CALL=1=duration=1=m=same-key";
    constexpr const char* valid_request = "R_50=CALL=1=duration=1=m=same-key";
    const auto invalid_response = client.request("GET", request_path(config, invalid_request));
    const auto invalid_body = nlohmann::json::parse(invalid_response->content.string());
    const auto accepted_response = client.request("GET", request_path(config, valid_request));
    const auto accepted_body = nlohmann::json::parse(accepted_response->content.string());
    bridge.shutdown();

    EXPECT_FALSE(invalid_body.at("accepted").get<bool>());
    EXPECT_EQ(invalid_body.at("reason").get<std::string>(), "invalid_trade_signal");
    EXPECT_TRUE(accepted_body.at("accepted").get<bool>());
    EXPECT_EQ(signal_count.load(), 1);
    EXPECT_EQ(invalid_reports.load(), 1);
}

TEST(BotBinaryBridge, ShutdownAndRunFromTradeCallbackDoNotDeadlock) {
    namespace bot = optionx::bridges::bot_binary;

    auto config = test_config();

    bot::BotBinaryBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<bot::BotBinaryBridgeConfig>(config)));

    std::atomic<optionx::SignalId> next_signal_id{500};
    std::atomic<int> signal_count{0};
    bridge.on_signal_id() = [&next_signal_id]() {
        return next_signal_id.fetch_add(1);
    };
    bridge.on_trade_signal() = [&](std::unique_ptr<optionx::TradeSignal>) {
        ++signal_count;
        bridge.shutdown();
        bridge.run();
    };

    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));

    HttpClient client(config.address + ":" + std::to_string(bridge.bound_http_port()));
    const auto response = client.request(
        "GET",
        request_path(config, "R_25=CALL=1=duration=1=m=reentrant-shutdown"));
    const auto body = nlohmann::json::parse(response->content.string());

    bridge.shutdown();
    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));
    bridge.shutdown();

    EXPECT_TRUE(body.at("accepted").get<bool>());
    EXPECT_EQ(signal_count.load(), 1);
}

TEST(BotBinaryBridge, HttpBindFailureFinalizesLifecycleAndAllowsRestart) {
    namespace bot = optionx::bridges::bot_binary;

    asio::io_context io_context;
    asio::ip::tcp::acceptor occupied_port(io_context);
    occupied_port.open(asio::ip::tcp::v4());
    occupied_port.set_option(asio::socket_base::reuse_address(false));
    occupied_port.bind(asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), 0));
    occupied_port.listen();

    auto config = test_config();
    config.port = occupied_port.local_endpoint().port();

    bot::BotBinaryBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<bot::BotBinaryBridgeConfig>(config)));

    std::atomic<optionx::SignalId> next_signal_id{700};
    std::atomic<bool> start_failed{false};
    std::atomic<int> signal_count{0};
    bridge.on_signal_id() = [&next_signal_id]() {
        return next_signal_id.fetch_add(1);
    };
    bridge.on_trade_signal() = [&signal_count](std::unique_ptr<optionx::TradeSignal>) {
        ++signal_count;
    };
    bridge.on_status_update() = [&start_failed](const optionx::BridgeStatusUpdate& update) {
        if (update.status == optionx::BridgeStatus::SERVER_START_FAILED) {
            start_failed.store(true);
        }
    };

    bridge.run();
    ASSERT_TRUE(wait_for_flag(start_failed));

    occupied_port.close();
    config.port = 0;
    ASSERT_TRUE(bridge.configure(std::make_unique<bot::BotBinaryBridgeConfig>(config)));
    bridge.run();
    ASSERT_TRUE(wait_for_http_port(bridge));

    HttpClient client(config.address + ":" + std::to_string(bridge.bound_http_port()));
    const auto response = client.request(
        "GET",
        request_path(config, "R_25=CALL=1=duration=1=m=bind-failure-retry"));
    const auto body = nlohmann::json::parse(response->content.string());
    bridge.shutdown();

    EXPECT_TRUE(body.at("accepted").get<bool>());
    EXPECT_EQ(signal_count.load(), 1);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
