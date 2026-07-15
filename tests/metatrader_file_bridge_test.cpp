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
    config.max_line_bytes = 4096;
    config.enable_events = true;
    config.enable_state_snapshot = false;

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
    EXPECT_EQ(restored.max_line_bytes, 4096u);
    EXPECT_TRUE(restored.enable_events);
    EXPECT_FALSE(restored.enable_state_snapshot);
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

TEST(MetaTraderFileBridgeConfig, AcceptsDotRootAndTrailingSeparators) {
    namespace mtfile = optionx::bridges::metatrader_file;

    mtfile::MetaTraderFileBridgeConfig dot_root;
    dot_root.common_files_root = ".";
    dot_root.bridge_id = 1;
    dot_root.client_id = "terminal-01";
    EXPECT_TRUE(dot_root.validate().first);

    mtfile::MetaTraderFileBridgeConfig slash_root;
    const auto root = make_temp_root();
    slash_root.common_files_root = (root / "").u8string();
    slash_root.bridge_id = 1;
    slash_root.client_id = "terminal-01";
    EXPECT_TRUE(slash_root.validate().first);
    EXPECT_TRUE(mtfile::path_is_within_or_equal(root, slash_root.client_root()));
    EXPECT_FALSE(mtfile::path_is_within_or_equal(root, root.parent_path() / "outside"));

#if defined(_WIN32)
    mtfile::MetaTraderFileBridgeConfig windows_root;
    windows_root.common_files_root = "C:/OptionXRoot/";
    windows_root.bridge_id = 1;
    windows_root.client_id = "terminal-01";
    EXPECT_TRUE(windows_root.validate().first);
#endif
}

TEST(MetaTraderFileProtocol, BuildsNdjsonLayout) {
    namespace protocol = optionx::bridges::metatrader_file::detail;

    optionx::bridges::metatrader_file::MetaTraderFileBridgeConfig config;
    config.common_files_root = "C:/Users/User/AppData/Roaming/MetaQuotes/Terminal/Common/Files";
    config.bridge_id = 11;
    config.client_id = "terminal-01";

    const auto layout = protocol::make_layout(config);
    EXPECT_EQ(layout.commands_log().filename().u8string(), "commands.ndjson");
    EXPECT_EQ(layout.events_log().filename().u8string(), "events.ndjson");
    EXPECT_EQ(layout.state_snapshot().filename().u8string(), "state.json");
    EXPECT_EQ(layout.commands_checkpoint().filename().u8string(), "commands.checkpoint.json");
    EXPECT_EQ(layout.events_checkpoint().filename().u8string(), "events.checkpoint.json");
}

TEST(MetaTraderFileProtocol, AppendsReadsAndClearsNdjsonLogs) {
    namespace protocol = optionx::bridges::metatrader_file::detail;

    const auto root = make_temp_root();
    ScopedPathCleanup cleanup(root);

    optionx::bridges::metatrader_file::MetaTraderFileBridgeConfig config;
    config.common_files_root = root.u8string();
    config.bridge_id = 11;
    config.client_id = "terminal-01";

    const auto layout = protocol::make_layout(config);
    protocol::ensure_runtime_directories(layout);

    const auto request1 = protocol::make_file_jsonrpc_request(
        1,
        "req-1",
        "account.balance.get",
        nlohmann::json{{"account_id", "1"}});

    const auto request2 = protocol::make_file_jsonrpc_request(
        2,
        "req-2",
        "trade.active.query");

    protocol::append_json_line(layout.commands_log(), request1, config.max_line_bytes);
    protocol::append_json_line(layout.commands_log(), request2, config.max_line_bytes);
    {
        std::ofstream out(layout.commands_log(), std::ios::binary | std::ios::app);
        out << "{\"file_seq\":3,\"jsonrpc\":\"2.0\"";
    }

    const auto parsed = protocol::read_ndjson_from_offset(
        layout.commands_log(),
        0,
        config.max_line_bytes);
    ASSERT_EQ(parsed.records.size(), 2u);
    EXPECT_TRUE(parsed.incomplete_tail);
    EXPECT_EQ(parsed.records[0].file_seq, 1u);
    EXPECT_EQ(parsed.records[0].document.at("id").get<std::string>(), "req-1");
    EXPECT_EQ(parsed.records[1].file_seq, 2u);
    EXPECT_EQ(parsed.records[1].document.at("method").get<std::string>(), "trade.active.query");

    const auto first_only = protocol::read_ndjson_from_offset(
        layout.commands_log(),
        0,
        config.max_line_bytes,
        1);
    ASSERT_EQ(first_only.records.size(), 1u);
    EXPECT_EQ(first_only.records[0].file_seq, 1u);
    EXPECT_FALSE(first_only.incomplete_tail);

    const auto since_one = protocol::read_ndjson_since_file_seq(
        layout.commands_log(),
        1,
        config.max_line_bytes);
    ASSERT_EQ(since_one.size(), 1u);
    EXPECT_EQ(since_one[0].file_seq, 2u);

    protocol::clear_file_atomic(layout.commands_log());
    protocol::append_json_line(
        layout.commands_log(),
        protocol::make_file_jsonrpc_request(3, "req-3", "account.balance.get"),
        config.max_line_bytes);

    const auto after_cleanup = protocol::read_ndjson_since_file_seq(
        layout.commands_log(),
        2,
        config.max_line_bytes);
    ASSERT_EQ(after_cleanup.size(), 1u);
    EXPECT_EQ(after_cleanup[0].file_seq, 3u);
    EXPECT_EQ(after_cleanup[0].document.at("id").get<std::string>(), "req-3");

    const auto checkpoint = protocol::make_log_checkpoint(3);
    EXPECT_EQ(checkpoint.at("last_file_seq").get<std::uint64_t>(), 3u);
    EXPECT_FALSE(checkpoint.contains("offset"));

    const auto offset_checkpoint = protocol::make_log_checkpoint_with_offset("gen-1", 128, 3);
    EXPECT_EQ(offset_checkpoint.at("log_generation").get<std::string>(), "gen-1");
    EXPECT_EQ(offset_checkpoint.at("offset").get<std::uint64_t>(), 128u);
    EXPECT_EQ(offset_checkpoint.at("last_file_seq").get<std::uint64_t>(), 3u);
}

TEST(MetaTraderFileProtocol, ReadsByFileSequenceAfterClearAndRegrow) {
    namespace protocol = optionx::bridges::metatrader_file::detail;

    const auto root = make_temp_root();
    ScopedPathCleanup cleanup(root);
    const auto log = root / "commands.ndjson";
    constexpr std::size_t max_line_bytes = 4096;

    for (std::uint64_t seq = 1; seq <= 20; ++seq) {
        protocol::append_json_line(
            log,
            protocol::make_file_jsonrpc_request(seq, "old-" + std::to_string(seq), "trade.open"),
            max_line_bytes);
    }
    const auto old_size = std::filesystem::file_size(log);
    const auto checkpoint = protocol::make_log_checkpoint(20);

    protocol::clear_file_atomic(log);
    for (std::uint64_t seq = 21; seq <= 50; ++seq) {
        protocol::append_json_line(
            log,
            protocol::make_file_jsonrpc_request(seq, "new-" + std::to_string(seq), "trade.open"),
            max_line_bytes);
    }
    ASSERT_GT(std::filesystem::file_size(log), old_size);

    const auto records = protocol::read_ndjson_since_file_seq(
        log,
        checkpoint.at("last_file_seq").get<std::uint64_t>(),
        max_line_bytes);
    ASSERT_EQ(records.size(), 30u);
    EXPECT_EQ(records.front().file_seq, 21u);
    EXPECT_EQ(records.front().document.at("id").get<std::string>(), "new-21");
    EXPECT_EQ(records.back().file_seq, 50u);
}

TEST(MetaTraderFileProtocol, RepairsIncompleteTailBeforeAppend) {
    namespace protocol = optionx::bridges::metatrader_file::detail;

    const auto root = make_temp_root();
    ScopedPathCleanup cleanup(root);
    const auto log = root / "commands.ndjson";
    constexpr std::size_t max_line_bytes = 4096;

    protocol::append_json_line(
        log,
        protocol::make_file_jsonrpc_request(1, "req-1", "trade.open"),
        max_line_bytes);
    {
        std::ofstream out(log, std::ios::binary | std::ios::app);
        out << "{\"file_seq\":2,\"jsonrpc\":\"2.0\"";
    }

    protocol::append_json_line(
        log,
        protocol::make_file_jsonrpc_request(3, "req-3", "trade.open"),
        max_line_bytes);

    const auto parsed = protocol::read_ndjson_from_offset(log, 0, max_line_bytes);
    ASSERT_EQ(parsed.records.size(), 2u);
    EXPECT_TRUE(parsed.malformed_records.empty());
    EXPECT_FALSE(parsed.incomplete_tail);
    EXPECT_EQ(parsed.records[0].file_seq, 1u);
    EXPECT_EQ(parsed.records[1].file_seq, 3u);
}

TEST(MetaTraderFileProtocol, SkipsMalformedCompleteLinesAndBoundsTail) {
    namespace protocol = optionx::bridges::metatrader_file::detail;

    const auto root = make_temp_root();
    ScopedPathCleanup cleanup(root);
    const auto log = root / "commands.ndjson";
    constexpr std::size_t max_line_bytes = 128;

    protocol::append_json_line(
        log,
        protocol::make_file_jsonrpc_request(1, "req-1", "trade.open"),
        max_line_bytes);
    {
        std::ofstream out(log, std::ios::binary | std::ios::app);
        out << "{\"file_seq\":2,\"jsonrpc\":\"2.0\"\n";
    }
    protocol::append_json_line(
        log,
        protocol::make_file_jsonrpc_request(3, "req-3", "trade.open"),
        max_line_bytes);

    const auto parsed = protocol::read_ndjson_from_offset(log, 0, max_line_bytes);
    ASSERT_EQ(parsed.records.size(), 2u);
    ASSERT_EQ(parsed.malformed_records.size(), 1u);
    EXPECT_EQ(parsed.records[0].file_seq, 1u);
    EXPECT_EQ(parsed.records[1].file_seq, 3u);
    EXPECT_GT(parsed.malformed_records[0].next_offset, parsed.malformed_records[0].start_offset);

    const auto huge_tail = root / "huge-tail.ndjson";
    {
        std::ofstream out(huge_tail, std::ios::binary);
        out << std::string(max_line_bytes + 1, 'x');
    }
    EXPECT_THROW(
        protocol::read_ndjson_from_offset(huge_tail, 0, max_line_bytes),
        std::runtime_error);
}

TEST(MetaTraderFileProtocol, InvalidFileSequenceIsMalformedAndDoesNotBlockLaterRecords) {
    namespace protocol = optionx::bridges::metatrader_file::detail;

    const auto root = make_temp_root();
    ScopedPathCleanup cleanup(root);
    const auto log = root / "commands.ndjson";
    constexpr std::size_t max_line_bytes = 4096;
    std::filesystem::create_directories(root);

    {
        std::ofstream out(log, std::ios::binary | std::ios::app);
        out << "{\"jsonrpc\":\"2.0\",\"method\":\"missing.seq\"}\n";
        out << "{\"file_seq\":0,\"jsonrpc\":\"2.0\",\"method\":\"zero.seq\"}\n";
        out << "{\"file_seq\":-1,\"jsonrpc\":\"2.0\",\"method\":\"negative.seq\"}\n";
        out << "{\"file_seq\":1.5,\"jsonrpc\":\"2.0\",\"method\":\"float.seq\"}\n";
        out << "{\"file_seq\":\"42\",\"jsonrpc\":\"2.0\",\"method\":\"string.seq\"}\n";
    }
    protocol::append_json_line(
        log,
        protocol::make_file_jsonrpc_request(42, "req-42", "valid.command"),
        max_line_bytes);

    const auto first_complete = protocol::read_ndjson_from_offset(log, 0, max_line_bytes, 1);
    EXPECT_TRUE(first_complete.records.empty());
    ASSERT_EQ(first_complete.malformed_records.size(), 1u);
    EXPECT_GT(first_complete.next_offset, 0u);

    const auto parsed = protocol::read_ndjson_from_offset(log, 0, max_line_bytes);
    ASSERT_EQ(parsed.records.size(), 1u);
    ASSERT_EQ(parsed.malformed_records.size(), 5u);
    EXPECT_EQ(parsed.records[0].file_seq, 42u);
    EXPECT_EQ(parsed.records[0].document.at("id").get<std::string>(), "req-42");

    const auto filtered = protocol::read_ndjson_since_file_seq(log, 0, max_line_bytes);
    ASSERT_EQ(filtered.size(), 1u);
    EXPECT_EQ(filtered[0].file_seq, 42u);
}

TEST(MetaTraderFileProtocol, SequenceWindowBoundsScannedAndReturnedRecords) {
    namespace protocol = optionx::bridges::metatrader_file::detail;

    const auto root = make_temp_root();
    ScopedPathCleanup cleanup(root);
    const auto log = root / "commands.ndjson";
    constexpr std::size_t max_line_bytes = 4096;
    std::filesystem::create_directories(root);

    protocol::append_json_line(
        log,
        protocol::make_file_jsonrpc_request(1, "old-1", "old.command"),
        max_line_bytes);
    {
        std::ofstream out(log, std::ios::binary | std::ios::app);
        out << "{\"jsonrpc\":\"2.0\",\"method\":\"bad.command\"}\n";
    }
    protocol::append_json_line(
        log,
        protocol::make_file_jsonrpc_request(2, "new-2", "new.command"),
        max_line_bytes);
    protocol::append_json_line(
        log,
        protocol::make_file_jsonrpc_request(3, "new-3", "new.command"),
        max_line_bytes);

    const auto first = protocol::read_ndjson_sequence_window(
        log,
        0,
        1,
        max_line_bytes,
        2);
    EXPECT_TRUE(first.records.empty());
    ASSERT_EQ(first.malformed_records.size(), 1u);
    EXPECT_EQ(first.scanned_records, 2u);
    EXPECT_TRUE(first.has_more);
    EXPECT_GT(first.next_offset, 0u);

    const auto second = protocol::read_ndjson_sequence_window(
        log,
        first.next_offset,
        1,
        max_line_bytes,
        10,
        1);
    ASSERT_EQ(second.records.size(), 1u);
    EXPECT_EQ(second.records[0].file_seq, 2u);
    EXPECT_TRUE(second.has_more);

    const auto third = protocol::read_ndjson_sequence_window(
        log,
        second.next_offset,
        1,
        max_line_bytes,
        10,
        1);
    ASSERT_EQ(third.records.size(), 1u);
    EXPECT_EQ(third.records[0].file_seq, 3u);
    EXPECT_FALSE(third.has_more);
}

TEST(MetaTraderFileProtocol, ComputesNextFileSequenceFromLogAndCheckpoint) {
    namespace protocol = optionx::bridges::metatrader_file::detail;

    const auto root = make_temp_root();
    ScopedPathCleanup cleanup(root);
    const auto log = root / "events.ndjson";
    constexpr std::size_t max_line_bytes = 4096;

    EXPECT_EQ(protocol::next_file_seq_after_checkpoint(log, 7, max_line_bytes), 8u);

    protocol::append_json_line(
        log,
        protocol::make_file_jsonrpc_notification(10, "balance.updated"),
        max_line_bytes);
    protocol::append_json_line(
        log,
        protocol::make_file_jsonrpc_notification(12, "trade.updated"),
        max_line_bytes);

    EXPECT_EQ(protocol::max_file_seq_in_ndjson(log, max_line_bytes), 12u);
    EXPECT_EQ(protocol::next_file_seq_after_checkpoint(log, 5, max_line_bytes), 13u);
    EXPECT_EQ(protocol::next_file_seq_after_checkpoint(log, 20, max_line_bytes), 21u);

    const auto first_after_ten = protocol::read_ndjson_since_file_seq(log, 10, max_line_bytes, 1);
    ASSERT_EQ(first_after_ten.size(), 1u);
    EXPECT_EQ(first_after_ten[0].file_seq, 12u);
}

TEST(MetaTraderFileProtocol, BoundedJsonReadRejectsOversizedSnapshots) {
    namespace protocol = optionx::bridges::metatrader_file::detail;

    const auto root = make_temp_root();
    ScopedPathCleanup cleanup(root);
    const auto json_file = root / "state.json";

    protocol::write_text_file_atomic(json_file, "{\"ok\":true}");
    const auto parsed = protocol::read_json_file(json_file, 32);
    EXPECT_TRUE(parsed.at("ok").get<bool>());

    protocol::write_text_file_atomic(json_file, "{\"value\":\"123456789\"}");
    EXPECT_THROW(
        protocol::read_json_file(json_file, 8),
        std::runtime_error);
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

    const auto root = make_temp_root();
    ScopedPathCleanup cleanup(root);
    optionx::bridges::metatrader_file::MetaTraderFileBridgeConfig config;
    config.common_files_root = root.u8string();
    config.bridge_id = 11;
    config.client_id = "terminal-01";
    const auto layout = protocol::make_layout(config);

    protocol::append_json_line(
        layout.events_log(),
        protocol::with_file_seq(balance_event, 1),
        config.max_line_bytes);
    const auto events = protocol::read_ndjson_since_file_seq(
        layout.events_log(),
        0,
        config.max_line_bytes);
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].document.at("method").get<std::string>(), "balance.updated");

    const auto state = protocol::make_state_snapshot(
        7,
        1783476720120LL,
        "connected",
        nlohmann::json::array({{
            {"account_id", "1"},
            {"balance", {{"value", "1024.50"}, {"currency", "USD"}}}
        }}));
    protocol::write_json_file_atomic(layout.state_snapshot(), state);
    const auto parsed_state = protocol::read_json_file(layout.state_snapshot(), config.max_line_bytes);
    EXPECT_EQ(parsed_state.at("version").get<std::uint64_t>(), 7u);
    EXPECT_EQ(parsed_state.at("connection").get<std::string>(), "connected");

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

    optionx::TradeResult cancelled;
    cancelled.trade_id = 88;
    cancelled.trade_state = optionx::TradeState::CANCELED_TRADE;
    const auto cancelled_snapshot = protocol::make_protocol_trade_snapshot(cancelled);
    EXPECT_EQ(cancelled_snapshot.at("state").get<std::string>(), "cancelled");
    EXPECT_EQ(cancelled_snapshot.at("outcome").get<std::string>(), "unknown");
    EXPECT_TRUE(cancelled_snapshot.at("final").get<bool>());
    EXPECT_TRUE(cancelled_snapshot.at("failure").is_null());

    cancelled.error_desc = "Cancelled by user.";
    const auto cancelled_with_reason = protocol::make_protocol_trade_snapshot(cancelled);
    EXPECT_EQ(cancelled_with_reason.at("failure").at("code").get<std::string>(), "cancelled");
    EXPECT_EQ(cancelled_with_reason.at("failure").at("message").get<std::string>(), "Cancelled by user.");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
