#include <gtest/gtest.h>

#include <optionx_cpp/bridges.hpp>

#include <chrono>
#include <filesystem>
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
    EXPECT_EQ(balance_payload.at("balance").at("value").get<std::string>(), "1024.5");
    EXPECT_EQ(balance_payload.at("balance").at("currency").get<std::string>(), "USD");

    optionx::TradeResult result;
    result.trade_id = 77;
    result.option_hash = "broker-77";
    result.amount = 1.0;
    result.profit = 0.82;
    result.trade_state = optionx::TradeState::WIN;

    const auto trade_event = protocol::make_trade_updated_notification(
        "evt-trade",
        "optionx://test/bridge/file",
        "stream-1",
        8,
        2000,
        2001,
        result);

    EXPECT_EQ(trade_event.at("method").get<std::string>(), "trade.updated");
    EXPECT_EQ(trade_event.at("params").at("subject").at("trade_id").get<std::string>(), "77");
    EXPECT_EQ(
        trade_event.at("params").at("payload").at("trade_result").at("option_hash").get<std::string>(),
        "broker-77");
    EXPECT_EQ(
        trade_event.at("params").at("payload").at("trade_result").at("trade_state").get<optionx::TradeState>(),
        optionx::TradeState::WIN);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
