#include <gtest/gtest.h>

#include <optionx_cpp/bridges.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace {

class ScopedPathCleanup {
public:
    explicit ScopedPathCleanup(std::filesystem::path cleanup_path)
        : path(std::move(cleanup_path)) {}

    ~ScopedPathCleanup() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }

    std::filesystem::path path;
};

std::filesystem::path make_temp_root() {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() /
           ("optionx_metatrader_file_bridge_test_" + std::to_string(stamp));
}

} // namespace

TEST(MetaTraderFileBridgeConfig, RoundTripsJsonAndValidatesSafeClientId) {
    optionx::bridges::metatrader_file::MetaTraderFileBridgeConfig config;
    config.common_files_root = "C:/Users/User/AppData/Roaming/MetaQuotes/Terminal/Common/Files";
    config.bridge_id = 9;
    config.client_id = "terminal-01";
    config.client_secret = "local-secret";
    config.poll_interval_ms = 100;
    config.processing_lease_ms = 5000;
    config.retention_ms = 10000;
    config.request_body_limit = 4096;
    config.max_ready_files = 32;
    config.archive_processed_requests = false;

    nlohmann::json json;
    config.to_json(json);

    optionx::bridges::metatrader_file::MetaTraderFileBridgeConfig restored;
    restored.from_json(json);

    EXPECT_EQ(restored.bridge_type(), optionx::BridgeType::METATRADER_FILE_TRANSPORT);
    EXPECT_EQ(restored.common_files_root, config.common_files_root);
    EXPECT_EQ(restored.bridge_id, 9u);
    EXPECT_EQ(restored.client_id, "terminal-01");
    EXPECT_EQ(restored.client_secret, "local-secret");
    EXPECT_EQ(restored.poll_interval_ms, 100);
    EXPECT_EQ(restored.processing_lease_ms, 5000);
    EXPECT_EQ(restored.retention_ms, 10000);
    EXPECT_EQ(restored.request_body_limit, 4096u);
    EXPECT_EQ(restored.max_ready_files, 32u);
    EXPECT_FALSE(restored.archive_processed_requests);
    EXPECT_TRUE(restored.validate().first);

    restored.client_id = "bad_client";
    EXPECT_FALSE(restored.validate().first);

    restored.client_id = "..";
    EXPECT_FALSE(restored.validate().first);

    restored.client_id = "CON";
    EXPECT_FALSE(restored.validate().first);

    restored.client_id = "terminal-01";
    restored.namespace_subdir = "../outside";
    EXPECT_FALSE(restored.validate().first);

    restored.namespace_subdir = "OptionX/../Bridge";
    EXPECT_FALSE(restored.validate().first);

    restored.namespace_subdir = "/absolute";
    EXPECT_FALSE(restored.validate().first);

    restored.namespace_subdir = "OptionX/Bridge/v1";
    EXPECT_TRUE(restored.validate().first);
}

TEST(MetaTraderFileProtocol, BuildsAndParsesOrderedEventFilenames) {
    namespace protocol = optionx::bridges::metatrader_file::detail;

    const auto filename = protocol::make_event_filename("dq-01", 42, "evt-a");
    EXPECT_EQ(filename, "dq-01_00000000000000000042_evt-a.json");

    const auto parsed = protocol::parse_event_filename(filename);
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->delivery_queue_id, "dq-01");
    EXPECT_EQ(parsed->delivery_seq, 42u);
    EXPECT_EQ(parsed->file_uuid, "evt-a");

    EXPECT_FALSE(protocol::parse_event_filename("dq_01_00000000000000000042_evt-a.json"));
    EXPECT_FALSE(protocol::parse_event_filename("dq-01_42_evt-a.json"));
    EXPECT_THROW(
        protocol::make_event_filename("dq_01", 1, "evt-a"),
        std::invalid_argument);
    EXPECT_THROW(
        protocol::make_event_filename("CON", 1, "evt-a"),
        std::invalid_argument);
    EXPECT_THROW(
        protocol::make_message_filename(1783476720120LL, ".."),
        std::invalid_argument);
    EXPECT_FALSE(protocol::is_safe_transport_filename("CON.json"));
}

TEST(MetaTraderFileProtocol, PublishesClaimsAndReadsJsonAtomically) {
    namespace protocol = optionx::bridges::metatrader_file::detail;

    const auto root = make_temp_root();
    ScopedPathCleanup cleanup(root);

    optionx::bridges::metatrader_file::MetaTraderFileBridgeConfig config;
    config.common_files_root = root.u8string();
    config.bridge_id = 11;
    config.client_id = "terminal-01";

    const auto layout = protocol::make_layout(config);
    protocol::ensure_runtime_directories(layout);

    const auto request = protocol::make_jsonrpc_request(
        "req-1",
        "account.balance.get",
        nlohmann::json{{"account_id", "1"}});
    const auto filename = protocol::make_message_filename(1783476720120LL, "req-a");
    const auto ready_file =
        protocol::publish_json_atomic(layout.requests_ready(), filename, request);

    EXPECT_TRUE(std::filesystem::exists(ready_file));
    EXPECT_FALSE(std::filesystem::exists(layout.requests_ready() / (filename + ".tmp")));

    std::filesystem::path claimed_file;
    ASSERT_TRUE(protocol::claim_ready_file(
        ready_file,
        layout.requests_processing(),
        claimed_file));
    EXPECT_FALSE(std::filesystem::exists(ready_file));
    EXPECT_TRUE(std::filesystem::exists(claimed_file));

    const auto parsed = protocol::read_json_file(claimed_file, 4096);
    EXPECT_EQ(parsed.at("jsonrpc").get<std::string>(), "2.0");
    EXPECT_EQ(parsed.at("id").get<std::string>(), "req-1");
    EXPECT_EQ(parsed.at("method").get<std::string>(), "account.balance.get");

    const auto collision_filename = protocol::make_message_filename(1783476720121LL, "req-b");
    const auto collision_path = layout.responses_ready() / collision_filename;
    {
        std::ofstream out(collision_path, std::ios::binary | std::ios::trunc);
        out << "{\"old\":true}";
    }
    EXPECT_THROW(
        protocol::publish_json_atomic(layout.responses_ready(), collision_filename, request),
        std::runtime_error);
    const auto old_collision = protocol::read_json_file(collision_path, 4096);
    EXPECT_TRUE(old_collision.at("old").get<bool>());

    const auto claim_collision_filename = protocol::make_message_filename(1783476720122LL, "req-c");
    const auto ready_collision = protocol::publish_json_atomic(
        layout.requests_ready(),
        claim_collision_filename,
        request);
    const auto processing_collision = layout.requests_processing() / claim_collision_filename;
    {
        std::ofstream out(processing_collision, std::ios::binary | std::ios::trunc);
        out << "{\"processing\":true}";
    }

    std::filesystem::path ignored_claim;
    EXPECT_FALSE(protocol::claim_ready_file(
        ready_collision,
        layout.requests_processing(),
        ignored_claim));
    EXPECT_TRUE(std::filesystem::exists(ready_collision));
    const auto old_processing = protocol::read_json_file(processing_collision, 4096);
    EXPECT_TRUE(old_processing.at("processing").get<bool>());
}

TEST(MetaTraderFileProtocol, BuildsBalanceAndTradeUpdateNotifications) {
    namespace protocol = optionx::bridges::metatrader_file::detail;

    const auto balance_event = protocol::make_balance_updated_notification(
        "evt-balance",
        "optionx://test/bridge/file",
        "stream-1",
        7,
        1000,
        1001,
        "1",
        1024.5,
        optionx::CurrencyType::USD);

    EXPECT_EQ(balance_event.at("method").get<std::string>(), "balance.updated");
    const auto& balance_payload = balance_event.at("params").at("payload");
    EXPECT_EQ(balance_payload.at("account_id").get<std::string>(), "1");
    EXPECT_EQ(balance_payload.at("balance").at("value").get<std::string>(), "1024.50");
    EXPECT_EQ(balance_payload.at("balance").at("currency").get<std::string>(), "USD");

    optionx::TradeRequest request;
    request.trade_id = 77;
    request.signal_id = 12;
    request.bridge_id = 4;
    request.account_id = 3;
    request.symbol = "BTCUSD";
    request.signal_name = "noisy_rsi_test";
    request.unique_hash = "tv:abc123";
    request.option_type = optionx::OptionType::SPRINT;
    request.order_type = optionx::OrderType::BUY;
    request.account_type = optionx::AccountType::DEMO;
    request.currency = optionx::CurrencyType::USD;
    request.amount = 1.0;
    request.duration = 60;

    optionx::TradeResult result;
    result.trade_id = 77;
    result.option_id = 123456;
    result.option_hash = "broker-77";
    result.amount = 1.0;
    result.payout = 0.82;
    result.profit = 0.82;
    result.currency = optionx::CurrencyType::USD;
    result.trade_state = optionx::TradeState::WIN;
    result.place_date = 2000;
    result.open_date = 2010;
    result.close_date = 62010;

    const auto trade_event = protocol::make_trade_updated_notification(
        "evt-trade",
        "optionx://test/bridge/file",
        "stream-1",
        8,
        2000,
        2001,
        request,
        result);

    EXPECT_EQ(trade_event.at("method").get<std::string>(), "trade.updated");
    EXPECT_EQ(trade_event.at("params").at("subject").at("trade_id").get<std::string>(), "77");
    EXPECT_EQ(trade_event.at("params").at("subject").at("signal_id").get<std::string>(), "12");

    const auto& trade = trade_event.at("params").at("payload").at("trade");
    EXPECT_FALSE(trade_event.at("params").at("payload").contains("trade_result"));
    EXPECT_FALSE(trade.contains("trade_state"));
    EXPECT_EQ(trade.at("trade_id").get<std::string>(), "77");
    EXPECT_EQ(trade.at("broker_option_id").get<std::string>(), "123456");
    EXPECT_EQ(trade.at("broker_option_hash").get<std::string>(), "broker-77");
    EXPECT_EQ(trade.at("account_id").get<std::string>(), "3");
    EXPECT_EQ(trade.at("symbol").get<std::string>(), "BTCUSD");
    EXPECT_EQ(trade.at("state").get<std::string>(), "closed");
    EXPECT_EQ(trade.at("outcome").get<std::string>(), "win");
    EXPECT_TRUE(trade.at("final").get<bool>());
    EXPECT_EQ(trade.at("amount").at("value").get<std::string>(), "1.00");
    EXPECT_EQ(trade.at("profit").at("value").get<std::string>(), "0.82");
    EXPECT_EQ(trade.at("payout").get<std::string>(), "0.820000");
    EXPECT_EQ(trade.at("duration_ms").get<std::uint64_t>(), 60000u);
    EXPECT_EQ(trade.at("origin_signal").at("signal_id").get<std::string>(), "12");
    EXPECT_EQ(trade.at("origin_signal").at("unique_hash").get<std::string>(), "tv:abc123");
    EXPECT_TRUE(trade.at("failure").is_null());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
