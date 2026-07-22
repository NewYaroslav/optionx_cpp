#include <gtest/gtest.h>

#include <optionx_cpp/bridges.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

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

class TestAccountInfo final : public optionx::BaseAccountInfoData {
public:
    std::int64_t user_id = 42;
    double balance = 1234.5;
    optionx::CurrencyType currency = optionx::CurrencyType::USD;
    optionx::AccountType account_type = optionx::AccountType::DEMO;
    bool connected = true;

    std::unique_ptr<optionx::BaseAccountInfoData> clone_unique() const override {
        return std::make_unique<TestAccountInfo>(*this);
    }

    std::shared_ptr<optionx::BaseAccountInfoData> clone_shared() const override {
        return std::make_shared<TestAccountInfo>(*this);
    }

private:
    bool get_info_bool(const optionx::AccountInfoRequest& request) const override {
        return request.type == optionx::AccountInfoType::CONNECTION_STATUS
            ? connected
            : false;
    }

    std::int64_t get_info_int64(const optionx::AccountInfoRequest& request) const override {
        switch (request.type) {
        case optionx::AccountInfoType::USER_ID:
            return user_id;
        case optionx::AccountInfoType::CONNECTION_STATUS:
            return connected ? 1 : 0;
        case optionx::AccountInfoType::ACCOUNT_TYPE:
            return static_cast<std::int64_t>(account_type);
        case optionx::AccountInfoType::CURRENCY:
            return static_cast<std::int64_t>(currency);
        default:
            return 0;
        }
    }

    double get_info_f64(const optionx::AccountInfoRequest& request) const override {
        return request.type == optionx::AccountInfoType::BALANCE ? balance : 0.0;
    }

    std::string get_info_str(const optionx::AccountInfoRequest& request) const override {
        return request.type == optionx::AccountInfoType::USER_ID
            ? std::to_string(user_id)
            : std::string();
    }

    optionx::AccountType get_info_account_type(
            const optionx::AccountInfoRequest&) const override {
        return account_type;
    }

    optionx::CurrencyType get_info_currency(
            const optionx::AccountInfoRequest&) const override {
        return currency;
    }
};

nlohmann::json make_signal_submit_params(
        std::string idempotency_key,
        std::string unique_hash,
        std::string symbol = "EURUSD",
        std::string order_type = "BUY",
        std::int64_t valid_until_ms =
            optionx::bridges::metatrader_file::detail::unix_time_ms() + 60000) {
    return nlohmann::json{
        {"context", {
            {"idempotency_key", std::move(idempotency_key)},
            {"valid_until_ms", valid_until_ms}
        }},
        {"identity", {
            {"unique_hash", std::move(unique_hash)},
            {"signal_name", "noisy_rsi"}
        }},
        {"signal", {
            {"symbol", std::move(symbol)},
            {"order_type", std::move(order_type)},
            {"option_type", "SPRINT"},
            {"amount", {{"value", "1.25"}, {"currency", "USD"}}},
            {"expiry", {{"kind", "duration"}, {"duration_ms", 60000}}}
        }}
    };
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
    config.max_command_log_bytes = 128 * 1024;
    config.max_scanned_records_per_poll = 32;
    config.max_returned_records_per_poll = 8;
    config.max_idempotency_state_bytes = 64 * 1024;
    config.max_idempotency_records = 128;
    config.idempotency_retention_ms = 12 * 60 * 60 * 1000;
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
    EXPECT_EQ(restored.max_command_log_bytes, 128u * 1024u);
    EXPECT_EQ(restored.max_scanned_records_per_poll, 32u);
    EXPECT_EQ(restored.max_returned_records_per_poll, 8u);
    EXPECT_EQ(restored.max_idempotency_state_bytes, 64u * 1024u);
    EXPECT_EQ(restored.max_idempotency_records, 128u);
    EXPECT_EQ(restored.idempotency_retention_ms, 12u * 60u * 60u * 1000u);
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
    restored.max_scanned_records_per_poll = 0;
    EXPECT_FALSE(restored.validate().first);

    restored.max_scanned_records_per_poll = 1;
    EXPECT_FALSE(restored.validate().first);

    restored.max_scanned_records_per_poll = 32;
    restored.max_returned_records_per_poll = 0;
    EXPECT_FALSE(restored.validate().first);

    restored.max_returned_records_per_poll = 8;
    restored.idempotency_retention_ms = 0;
    EXPECT_FALSE(restored.validate().first);

    restored.idempotency_retention_ms = 12 * 60 * 60 * 1000;
    EXPECT_TRUE(restored.validate().first);
}

TEST(MetaTraderFileOperationKey, GeneratesCompactUniqueKeys) {
    namespace mtfile = optionx::bridges::metatrader_file;

    const auto first = mtfile::make_compact_operation_key("mfb", 12);
    const auto second = mtfile::make_compact_operation_key("mfb", 12);

    EXPECT_NE(first, second);
    EXPECT_EQ(first.rfind("mfb_", 0), 0u);
    EXPECT_EQ(second.rfind("mfb_", 0), 0u);
    for (const auto ch : first) {
        EXPECT_TRUE(
            (ch >= '0' && ch <= '9') ||
            (ch >= 'a' && ch <= 'z') ||
            ch == '_');
    }
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

TEST(MetaTraderFileProtocol, SequenceWindowUsesActualStartOffsetForHasMore) {
    namespace protocol = optionx::bridges::metatrader_file::detail;

    const auto root = make_temp_root();
    ScopedPathCleanup cleanup(root);
    const auto log = root / "commands.ndjson";
    constexpr std::size_t max_line_bytes = 4096;

    protocol::append_json_line(
        log,
        protocol::make_file_jsonrpc_request(1, "new-1", "new.command"),
        max_line_bytes);
    protocol::append_json_line(
        log,
        protocol::make_file_jsonrpc_request(2, "new-2", "new.command"),
        max_line_bytes);

    const auto truncated = protocol::read_ndjson_sequence_window(
        log,
        1000,
        0,
        max_line_bytes,
        1);
    EXPECT_TRUE(truncated.source_truncated);
    EXPECT_EQ(truncated.scanned_records, 1u);
    ASSERT_EQ(truncated.records.size(), 1u);
    EXPECT_EQ(truncated.records[0].file_seq, 1u);
    EXPECT_TRUE(truncated.has_more);
    EXPECT_GT(truncated.next_offset, 0u);

    const auto exact_eof = protocol::read_ndjson_sequence_window(
        log,
        0,
        0,
        max_line_bytes,
        2);
    EXPECT_EQ(exact_eof.scanned_records, 2u);
    ASSERT_EQ(exact_eof.records.size(), 2u);
    EXPECT_FALSE(exact_eof.has_more);
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
        optionx::CurrencyType::USD,
        "broker-user-1");

    EXPECT_EQ(balance_event.at("method").get<std::string>(), "balance.updated");
    const auto& balance_payload = balance_event.at("params").at("payload");
    EXPECT_EQ(balance_payload.at("account_id").get<std::string>(), "1");
    EXPECT_EQ(balance_payload.at("user_id").get<std::string>(), "broker-user-1");
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

TEST(MetaTraderFileBridge, ProcessesSignalSubmitAndWritesResponse) {
    namespace mtfile = optionx::bridges::metatrader_file;
    namespace protocol = optionx::bridges::metatrader_file::detail;

    const auto root = make_temp_root();
    ScopedPathCleanup cleanup(root);

    mtfile::MetaTraderFileBridgeConfig config;
    config.common_files_root = root.u8string();
    config.bridge_id = 31;
    config.client_id = "terminal-01";
    config.max_line_bytes = 8192;
    config.max_scanned_records_per_poll = 16;
    config.max_returned_records_per_poll = 4;

    const auto layout = protocol::make_layout(config);
    protocol::ensure_runtime_directories(layout);

    protocol::append_json_line(
        layout.commands_log(),
        protocol::make_file_jsonrpc_request(
            1,
            "cmd-1",
            "signal.submit",
            nlohmann::json{
                {"context", {
                    {"idempotency_key", "mql5:terminal-01:bar-1:buy"},
                    {"valid_until_ms", protocol::unix_time_ms() + 60000}
                }},
                {"routing", {
                    {"selector", {
                        {"kind", "account"},
                        {"account_id", "7"}
                    }}
                }},
                {"identity", {
                    {"unique_hash", "signal-hash-1"},
                    {"signal_name", "noisy_rsi"}
                }},
                {"signal", {
                    {"symbol", "EURUSD"},
                    {"order_type", "BUY"},
                    {"option_type", "SPRINT"},
                    {"amount", {{"value", "1.25"}, {"currency", "USD"}}},
                    {"expiry", {{"kind", "duration"}, {"duration_ms", 60000}}}
                }}
            }),
        config.max_line_bytes);

    mtfile::MetaTraderFileBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<mtfile::MetaTraderFileBridgeConfig>(config)));

    optionx::SignalId next_signal_id = 100;
    bridge.on_signal_id() = [&]() {
        return next_signal_id++;
    };

    std::vector<std::unique_ptr<optionx::TradeSignal>> signals;
    bridge.on_trade_signal() = [&](std::unique_ptr<optionx::TradeSignal> signal) {
        ASSERT_TRUE(signal);
        signals.push_back(std::move(signal));
    };

    bridge.process();

    ASSERT_EQ(signals.size(), 1u);
    EXPECT_EQ(signals[0]->signal_id, 100u);
    EXPECT_EQ(signals[0]->bridge_id, 31u);
    EXPECT_EQ(signals[0]->account_id, 7);
    EXPECT_EQ(signals[0]->symbol, "EURUSD");
    EXPECT_EQ(signals[0]->order_type, optionx::OrderType::BUY);
    EXPECT_EQ(signals[0]->option_type, optionx::OptionType::SPRINT);
    EXPECT_DOUBLE_EQ(signals[0]->amount, 1.25);
    EXPECT_EQ(signals[0]->currency, optionx::CurrencyType::USD);
    EXPECT_EQ(signals[0]->duration, 60u);
    EXPECT_EQ(signals[0]->unique_hash, "signal-hash-1");
    EXPECT_EQ(signals[0]->signal_name, "noisy_rsi");

    const auto checkpoint = protocol::read_json_file(
        layout.commands_checkpoint(),
        config.max_line_bytes);
    EXPECT_EQ(checkpoint.at("last_file_seq").get<std::uint64_t>(), 1u);

    const auto events = protocol::read_ndjson_since_file_seq(
        layout.events_log(),
        0,
        config.max_line_bytes);
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].file_seq, 1u);
    EXPECT_EQ(events[0].document.at("id").get<std::string>(), "cmd-1");
    const auto& result = events[0].document.at("result");
    EXPECT_EQ(result.at("status").get<std::string>(), "accepted");
    EXPECT_EQ(result.at("operation_id").get<std::string>(), "file:31:1");
    EXPECT_EQ(result.at("signal_ref").at("signal_id").get<std::string>(), "100");
}

TEST(MetaTraderFileBridge, ReadsCommandsAfterOwnerClearsAndRegrowsLog) {
    namespace mtfile = optionx::bridges::metatrader_file;
    namespace protocol = optionx::bridges::metatrader_file::detail;

    const auto root = make_temp_root();
    ScopedPathCleanup cleanup(root);

    mtfile::MetaTraderFileBridgeConfig config;
    config.common_files_root = root.u8string();
    config.bridge_id = 33;
    config.client_id = "terminal-clear-regrow";
    config.max_line_bytes = 65536;
    config.max_returned_records_per_poll = 64;

    const auto layout = protocol::make_layout(config);
    protocol::ensure_runtime_directories(layout);

    protocol::append_json_line(
        layout.commands_log(),
        protocol::make_file_jsonrpc_request(
            1,
            "cmd-1",
            "signal.submit",
            make_signal_submit_params("clear-regrow-1", "signal-hash-1")),
        config.max_line_bytes);

    mtfile::MetaTraderFileBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<mtfile::MetaTraderFileBridgeConfig>(config)));

    optionx::SignalId next_signal_id = 100;
    bridge.on_signal_id() = [&]() {
        return next_signal_id++;
    };

    std::vector<std::string> symbols;
    bridge.on_trade_signal() = [&](std::unique_ptr<optionx::TradeSignal> signal) {
        ASSERT_TRUE(signal);
        symbols.push_back(signal->symbol);
    };

    bridge.process();
    ASSERT_EQ(symbols.size(), 1u);
    const auto old_size = std::filesystem::file_size(layout.commands_log());

    protocol::clear_file_atomic(layout.commands_log());

    for (std::uint64_t seq = 2; seq < 20; ++seq) {
        protocol::append_json_line(
            layout.commands_log(),
            protocol::make_file_jsonrpc_request(
                seq,
                "noop-" + std::to_string(seq),
                "protocol.hello"),
            config.max_line_bytes);
    }
    protocol::append_json_line(
        layout.commands_log(),
        protocol::make_file_jsonrpc_request(
            20,
            "cmd-20",
            "signal.submit",
            make_signal_submit_params("clear-regrow-20", "signal-hash-20", "GBPUSD")),
        config.max_line_bytes);
    ASSERT_GE(std::filesystem::file_size(layout.commands_log()), old_size);

    bridge.process();
    ASSERT_EQ(symbols.size(), 2u);
    EXPECT_EQ(symbols[1], "GBPUSD");

    const auto checkpoint = protocol::read_json_file(
        layout.commands_checkpoint(),
        config.max_line_bytes);
    EXPECT_EQ(checkpoint.at("last_file_seq").get<std::uint64_t>(), 20u);
}

TEST(MetaTraderFileBridge, UsesBoundedScanAfterOwnerClearsAndRegrowsLog) {
    namespace mtfile = optionx::bridges::metatrader_file;
    namespace protocol = optionx::bridges::metatrader_file::detail;

    const auto root = make_temp_root();
    ScopedPathCleanup cleanup(root);

    mtfile::MetaTraderFileBridgeConfig config;
    config.common_files_root = root.u8string();
    config.bridge_id = 34;
    config.client_id = "terminal-bounded-regrow";
    config.max_line_bytes = 8192;
    config.max_scanned_records_per_poll = 3;
    config.max_returned_records_per_poll = 8;

    const auto layout = protocol::make_layout(config);
    protocol::ensure_runtime_directories(layout);

    protocol::append_json_line(
        layout.commands_log(),
        protocol::make_file_jsonrpc_request(
            1,
            "cmd-1",
            "signal.submit",
            make_signal_submit_params("bounded-regrow-1", "bounded-regrow-1")),
        config.max_line_bytes);

    mtfile::MetaTraderFileBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<mtfile::MetaTraderFileBridgeConfig>(config)));
    optionx::SignalId next_signal_id = 100;
    bridge.on_signal_id() = [&]() {
        return next_signal_id++;
    };

    std::vector<std::string> symbols;
    bridge.on_trade_signal() = [&](std::unique_ptr<optionx::TradeSignal> signal) {
        ASSERT_TRUE(signal);
        symbols.push_back(signal->symbol);
    };

    bridge.process();
    ASSERT_EQ(symbols.size(), 1u);

    protocol::clear_file_atomic(layout.commands_log());
    for (std::uint64_t seq = 2; seq <= 6; ++seq) {
        protocol::append_json_line(
            layout.commands_log(),
            protocol::make_file_jsonrpc_request(
                seq,
                "noop-" + std::to_string(seq),
                "protocol.hello"),
            config.max_line_bytes);
    }
    protocol::append_json_line(
        layout.commands_log(),
        protocol::make_file_jsonrpc_request(
            7,
            "cmd-7",
            "signal.submit",
            make_signal_submit_params("bounded-regrow-7", "bounded-regrow-7", "GBPUSD")),
        config.max_line_bytes);

    bridge.process();
    EXPECT_EQ(symbols.size(), 1u);

    bridge.process();
    EXPECT_EQ(symbols.size(), 1u);

    bridge.process();
    ASSERT_EQ(symbols.size(), 2u);
    EXPECT_EQ(symbols.back(), "GBPUSD");

    const auto checkpoint = protocol::read_json_file(
        layout.commands_checkpoint(),
        config.max_line_bytes);
    EXPECT_EQ(checkpoint.at("last_file_seq").get<std::uint64_t>(), 7u);
}

TEST(MetaTraderFileBridge, DetectsRegrownLogWhenFirstCompleteLineIsMalformed) {
    namespace mtfile = optionx::bridges::metatrader_file;
    namespace protocol = optionx::bridges::metatrader_file::detail;

    const auto root = make_temp_root();
    ScopedPathCleanup cleanup(root);

    mtfile::MetaTraderFileBridgeConfig config;
    config.common_files_root = root.u8string();
    config.bridge_id = 35;
    config.client_id = "terminal-malformed-regrow";
    config.max_line_bytes = 8192;
    config.max_scanned_records_per_poll = 2;
    config.max_returned_records_per_poll = 1;

    const auto layout = protocol::make_layout(config);
    protocol::ensure_runtime_directories(layout);
    protocol::append_json_line(
        layout.commands_log(),
        protocol::make_file_jsonrpc_request(
            1,
            "cmd-1",
            "signal.submit",
            make_signal_submit_params("malformed-regrow-1", "malformed-regrow-1")),
        config.max_line_bytes);

    mtfile::MetaTraderFileBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<mtfile::MetaTraderFileBridgeConfig>(config)));
    optionx::SignalId next_signal_id = 100;
    bridge.on_signal_id() = [&]() {
        return next_signal_id++;
    };

    std::vector<std::string> symbols;
    bridge.on_trade_signal() = [&](std::unique_ptr<optionx::TradeSignal> signal) {
        ASSERT_TRUE(signal);
        symbols.push_back(signal->symbol);
    };

    bridge.process();
    ASSERT_EQ(symbols.size(), 1u);
    const auto old_size = std::filesystem::file_size(layout.commands_log());

    protocol::clear_file_atomic(layout.commands_log());
    {
        std::ofstream out(layout.commands_log(), std::ios::binary | std::ios::app);
        ASSERT_TRUE(out);
        out << nlohmann::json{{"broken", std::string(1024, 'x')}}.dump(-1) << '\n';
    }
    protocol::append_json_line(
        layout.commands_log(),
        protocol::make_file_jsonrpc_request(
            2,
            "cmd-2",
            "signal.submit",
            make_signal_submit_params("malformed-regrow-2", "malformed-regrow-2", "GBPUSD")),
        config.max_line_bytes);
    ASSERT_GE(std::filesystem::file_size(layout.commands_log()), old_size);

    bridge.process();
    ASSERT_EQ(symbols.size(), 2u);
    EXPECT_EQ(symbols.back(), "GBPUSD");
}

TEST(MetaTraderFileBridge, RejectsTradeAffectingCommandWithoutIdempotencyKey) {
    namespace mtfile = optionx::bridges::metatrader_file;
    namespace protocol = optionx::bridges::metatrader_file::detail;

    const auto root = make_temp_root();
    ScopedPathCleanup cleanup(root);

    mtfile::MetaTraderFileBridgeConfig config;
    config.common_files_root = root.u8string();
    config.bridge_id = 36;
    config.client_id = "terminal-missing-key";
    config.max_line_bytes = 8192;

    const auto layout = protocol::make_layout(config);
    protocol::ensure_runtime_directories(layout);

    protocol::append_json_line(
        layout.commands_log(),
        protocol::make_file_jsonrpc_request(
            1,
            "cmd-1",
            "signal.submit",
            make_signal_submit_params("", "missing-key")),
        config.max_line_bytes);

    mtfile::MetaTraderFileBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<mtfile::MetaTraderFileBridgeConfig>(config)));
    bridge.on_signal_id() = []() {
        return optionx::SignalId{1};
    };

    int signal_count = 0;
    bridge.on_trade_signal() = [&](std::unique_ptr<optionx::TradeSignal>) {
        ++signal_count;
    };

    bridge.process();
    EXPECT_EQ(signal_count, 0);

    const auto events = protocol::read_ndjson_since_file_seq(
        layout.events_log(),
        0,
        config.max_line_bytes);
    ASSERT_EQ(events.size(), 1u);
    const auto& error = events[0].document.at("error");
    EXPECT_EQ(error.at("code").get<int>(), -32602);
}

TEST(MetaTraderFileBridge, RejectsTradeAffectingCommandWithoutValidUntil) {
    namespace mtfile = optionx::bridges::metatrader_file;
    namespace protocol = optionx::bridges::metatrader_file::detail;

    const auto root = make_temp_root();
    ScopedPathCleanup cleanup(root);

    mtfile::MetaTraderFileBridgeConfig config;
    config.common_files_root = root.u8string();
    config.bridge_id = 46;
    config.client_id = "terminal-missing-deadline";
    config.max_line_bytes = 8192;

    const auto layout = protocol::make_layout(config);
    protocol::ensure_runtime_directories(layout);

    auto params = make_signal_submit_params("missing-deadline", "missing-deadline");
    params["context"].erase("valid_until_ms");
    protocol::append_json_line(
        layout.commands_log(),
        protocol::make_file_jsonrpc_request(
            1,
            "cmd-1",
            "signal.submit",
            params),
        config.max_line_bytes);

    mtfile::MetaTraderFileBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<mtfile::MetaTraderFileBridgeConfig>(config)));
    bridge.on_signal_id() = []() {
        return optionx::SignalId{1};
    };

    int signal_count = 0;
    bridge.on_trade_signal() = [&](std::unique_ptr<optionx::TradeSignal>) {
        ++signal_count;
    };

    bridge.process();
    EXPECT_EQ(signal_count, 0);

    const auto events = protocol::read_ndjson_since_file_seq(
        layout.events_log(),
        0,
        config.max_line_bytes);
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].document.at("error").at("code").get<int>(), -32602);
}

TEST(MetaTraderFileBridge, RejectsSignalWhenTradeCallbackIsMissing) {
    namespace mtfile = optionx::bridges::metatrader_file;
    namespace protocol = optionx::bridges::metatrader_file::detail;

    const auto root = make_temp_root();
    ScopedPathCleanup cleanup(root);

    mtfile::MetaTraderFileBridgeConfig config;
    config.common_files_root = root.u8string();
    config.bridge_id = 37;
    config.client_id = "terminal-no-callback";
    config.max_line_bytes = 8192;

    const auto layout = protocol::make_layout(config);
    protocol::ensure_runtime_directories(layout);
    protocol::append_json_line(
        layout.commands_log(),
        protocol::make_file_jsonrpc_request(
            1,
            "cmd-1",
            "signal.submit",
            make_signal_submit_params("missing-callback", "missing-callback")),
        config.max_line_bytes);

    mtfile::MetaTraderFileBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<mtfile::MetaTraderFileBridgeConfig>(config)));
    bridge.on_signal_id() = []() {
        return optionx::SignalId{1};
    };

    bridge.process();

    const auto events = protocol::read_ndjson_since_file_seq(
        layout.events_log(),
        0,
        config.max_line_bytes);
    ASSERT_EQ(events.size(), 1u);
    const auto& result = events[0].document.at("result");
    EXPECT_EQ(result.at("status").get<std::string>(), "rejected");
    EXPECT_EQ(result.at("reason").at("code").get<std::string>(), "signal_handler_unavailable");
}

TEST(MetaTraderFileBridge, CorruptIdempotencyStateFailsClosed) {
    namespace mtfile = optionx::bridges::metatrader_file;
    namespace protocol = optionx::bridges::metatrader_file::detail;

    const auto root = make_temp_root();
    ScopedPathCleanup cleanup(root);

    mtfile::MetaTraderFileBridgeConfig config;
    config.common_files_root = root.u8string();
    config.bridge_id = 47;
    config.client_id = "terminal-corrupt-idempotency";
    config.max_line_bytes = 8192;

    const auto layout = protocol::make_layout(config);
    protocol::ensure_runtime_directories(layout);
    protocol::append_json_line(
        layout.commands_log(),
        protocol::make_file_jsonrpc_request(
            1,
            "cmd-1",
            "signal.submit",
            make_signal_submit_params("corrupt-idempotency", "corrupt-idempotency")),
        config.max_line_bytes);
    protocol::write_json_file_atomic(
        layout.idempotency_state(),
        nlohmann::json{
            {"processed_through_file_seq", "not-a-number"},
            {"records", nlohmann::json::object()},
            {"tombstones", nlohmann::json::object()},
            {"request_index", nlohmann::json::object()}
        });

    mtfile::MetaTraderFileBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<mtfile::MetaTraderFileBridgeConfig>(config)));
    bridge.on_signal_id() = []() {
        return optionx::SignalId{1};
    };

    int signal_count = 0;
    bridge.on_trade_signal() = [&](std::unique_ptr<optionx::TradeSignal>) {
        ++signal_count;
    };

    EXPECT_THROW(bridge.process(), std::runtime_error);
    EXPECT_EQ(signal_count, 0);
}

TEST(MetaTraderFileBridge, CorruptCommandCheckpointDoesNotReplayCommands) {
    namespace mtfile = optionx::bridges::metatrader_file;
    namespace protocol = optionx::bridges::metatrader_file::detail;

    const auto root = make_temp_root();
    ScopedPathCleanup cleanup(root);

    mtfile::MetaTraderFileBridgeConfig config;
    config.common_files_root = root.u8string();
    config.bridge_id = 38;
    config.client_id = "terminal-corrupt-checkpoint";
    config.max_line_bytes = 8192;

    const auto layout = protocol::make_layout(config);
    protocol::ensure_runtime_directories(layout);
    protocol::append_json_line(
        layout.commands_log(),
        protocol::make_file_jsonrpc_request(
            1,
            "cmd-1",
            "signal.submit",
            make_signal_submit_params("corrupt-checkpoint", "corrupt-checkpoint")),
        config.max_line_bytes);
    protocol::write_json_file_atomic(
        layout.commands_checkpoint(),
        nlohmann::json{{"offset", 100}});

    mtfile::MetaTraderFileBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<mtfile::MetaTraderFileBridgeConfig>(config)));
    bridge.on_signal_id() = []() {
        return optionx::SignalId{1};
    };

    int signal_count = 0;
    bridge.on_trade_signal() = [&](std::unique_ptr<optionx::TradeSignal>) {
        ++signal_count;
    };

    EXPECT_THROW(bridge.process(), std::runtime_error);
    EXPECT_EQ(signal_count, 0);
}

TEST(MetaTraderFileBridge, DurableIdempotencySuppressesReplayAfterCheckpointLoss) {
    namespace mtfile = optionx::bridges::metatrader_file;
    namespace protocol = optionx::bridges::metatrader_file::detail;

    const auto root = make_temp_root();
    ScopedPathCleanup cleanup(root);

    mtfile::MetaTraderFileBridgeConfig config;
    config.common_files_root = root.u8string();
    config.bridge_id = 39;
    config.client_id = "terminal-idempotency";
    config.max_line_bytes = 8192;

    const auto layout = protocol::make_layout(config);
    protocol::ensure_runtime_directories(layout);
    protocol::append_json_line(
        layout.commands_log(),
        protocol::make_file_jsonrpc_request(
            1,
            "cmd-1",
            "signal.submit",
            make_signal_submit_params("same-operation", "same-operation")),
        config.max_line_bytes);

    {
        mtfile::MetaTraderFileBridge bridge;
        ASSERT_TRUE(bridge.configure(std::make_unique<mtfile::MetaTraderFileBridgeConfig>(config)));
        bridge.on_signal_id() = []() {
            return optionx::SignalId{10};
        };

        int signal_count = 0;
        bridge.on_trade_signal() = [&](std::unique_ptr<optionx::TradeSignal>) {
            ++signal_count;
        };
        bridge.process();
        EXPECT_EQ(signal_count, 1);
    }

    std::filesystem::remove(layout.commands_checkpoint());

    mtfile::MetaTraderFileBridge replay_bridge;
    ASSERT_TRUE(replay_bridge.configure(std::make_unique<mtfile::MetaTraderFileBridgeConfig>(config)));
    replay_bridge.on_signal_id() = []() {
        return optionx::SignalId{11};
    };

    int replay_count = 0;
    replay_bridge.on_trade_signal() = [&](std::unique_ptr<optionx::TradeSignal>) {
        ++replay_count;
    };
    replay_bridge.process();
    EXPECT_EQ(replay_count, 0);

    const auto events = protocol::read_ndjson_since_file_seq(
        layout.events_log(),
        0,
        config.max_line_bytes);
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].document.at("result").at("signal_ref").at("signal_id").get<std::string>(), "10");
}

TEST(MetaTraderFileBridge, RejectsJsonRpcIdReuseForDifferentOperation) {
    namespace mtfile = optionx::bridges::metatrader_file;
    namespace protocol = optionx::bridges::metatrader_file::detail;

    const auto root = make_temp_root();
    ScopedPathCleanup cleanup(root);

    mtfile::MetaTraderFileBridgeConfig config;
    config.common_files_root = root.u8string();
    config.bridge_id = 40;
    config.client_id = "terminal-rpc-id";
    config.max_line_bytes = 8192;

    const auto layout = protocol::make_layout(config);
    protocol::ensure_runtime_directories(layout);
    protocol::append_json_line(
        layout.commands_log(),
        protocol::make_file_jsonrpc_request(
            1,
            "request-42",
            "signal.submit",
            make_signal_submit_params("key-A", "rpc-id-A")),
        config.max_line_bytes);
    protocol::append_json_line(
        layout.commands_log(),
        protocol::make_file_jsonrpc_request(
            2,
            "request-42",
            "signal.submit",
            make_signal_submit_params("key-B", "rpc-id-B", "GBPUSD")),
        config.max_line_bytes);

    mtfile::MetaTraderFileBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<mtfile::MetaTraderFileBridgeConfig>(config)));
    bridge.on_signal_id() = []() {
        return optionx::SignalId{50};
    };

    int signal_count = 0;
    bridge.on_trade_signal() = [&](std::unique_ptr<optionx::TradeSignal>) {
        ++signal_count;
    };
    bridge.process();
    EXPECT_EQ(signal_count, 1);

    const auto events = protocol::read_ndjson_since_file_seq(
        layout.events_log(),
        0,
        config.max_line_bytes);
    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0].document.at("result").at("signal_ref").at("signal_id").get<std::string>(), "50");
    EXPECT_EQ(events[1].document.at("result").at("status").get<std::string>(), "rejected");
    EXPECT_EQ(events[1].document.at("result").at("reason").at("code").get<std::string>(), "idempotency_conflict");
}

TEST(MetaTraderFileBridge, DeduplicatesRetryWhenOnlyDeadlineChanges) {
    namespace mtfile = optionx::bridges::metatrader_file;
    namespace protocol = optionx::bridges::metatrader_file::detail;

    const auto root = make_temp_root();
    ScopedPathCleanup cleanup(root);

    mtfile::MetaTraderFileBridgeConfig config;
    config.common_files_root = root.u8string();
    config.bridge_id = 40;
    config.client_id = "terminal-retry-deadline";
    config.max_line_bytes = 8192;

    const auto layout = protocol::make_layout(config);
    protocol::ensure_runtime_directories(layout);
    protocol::append_json_line(
        layout.commands_log(),
        protocol::make_file_jsonrpc_request(
            1,
            "request-1",
            "signal.submit",
            make_signal_submit_params("retry-key", "retry-hash", "EURUSD", "BUY",
                                      protocol::unix_time_ms() + 60000)),
        config.max_line_bytes);
    protocol::append_json_line(
        layout.commands_log(),
        protocol::make_file_jsonrpc_request(
            2,
            "request-2",
            "signal.submit",
            make_signal_submit_params("retry-key", "retry-hash", "EURUSD", "BUY",
                                      protocol::unix_time_ms() - 1)),
        config.max_line_bytes);

    mtfile::MetaTraderFileBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<mtfile::MetaTraderFileBridgeConfig>(config)));
    bridge.on_signal_id() = []() {
        return optionx::SignalId{51};
    };

    int signal_count = 0;
    bridge.on_trade_signal() = [&](std::unique_ptr<optionx::TradeSignal>) {
        ++signal_count;
    };
    bridge.process();
    EXPECT_EQ(signal_count, 1);

    const auto events = protocol::read_ndjson_since_file_seq(
        layout.events_log(),
        0,
        config.max_line_bytes);
    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0].document.at("result").at("signal_ref").at("signal_id").get<std::string>(), "51");
    EXPECT_EQ(events[1].document.at("result").at("signal_ref").at("signal_id").get<std::string>(), "51");
}

TEST(MetaTraderFileBridge, RejectsSameIdempotencyKeyWithDifferentBusinessPayload) {
    namespace mtfile = optionx::bridges::metatrader_file;
    namespace protocol = optionx::bridges::metatrader_file::detail;

    const auto root = make_temp_root();
    ScopedPathCleanup cleanup(root);

    mtfile::MetaTraderFileBridgeConfig config;
    config.common_files_root = root.u8string();
    config.bridge_id = 40;
    config.client_id = "terminal-retry-payload";
    config.max_line_bytes = 8192;

    const auto layout = protocol::make_layout(config);
    protocol::ensure_runtime_directories(layout);
    protocol::append_json_line(
        layout.commands_log(),
        protocol::make_file_jsonrpc_request(
            1,
            "request-1",
            "signal.submit",
            make_signal_submit_params("retry-key", "retry-hash", "EURUSD")),
        config.max_line_bytes);
    protocol::append_json_line(
        layout.commands_log(),
        protocol::make_file_jsonrpc_request(
            2,
            "request-2",
            "signal.submit",
            make_signal_submit_params("retry-key", "retry-hash", "GBPUSD")),
        config.max_line_bytes);

    mtfile::MetaTraderFileBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<mtfile::MetaTraderFileBridgeConfig>(config)));
    bridge.on_signal_id() = []() {
        return optionx::SignalId{52};
    };

    int signal_count = 0;
    bridge.on_trade_signal() = [&](std::unique_ptr<optionx::TradeSignal>) {
        ++signal_count;
    };
    std::vector<optionx::BridgeSignalReport> reports;
    bridge.on_signal_report() = [&](const optionx::BridgeSignalReport& report) {
        reports.push_back(report);
    };
    bridge.process();
    EXPECT_EQ(signal_count, 1);

    const auto events = protocol::read_ndjson_since_file_seq(
        layout.events_log(),
        0,
        config.max_line_bytes);
    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0].document.at("result").at("signal_ref").at("signal_id").get<std::string>(), "52");
    EXPECT_EQ(events[1].document.at("result").at("status").get<std::string>(), "rejected");
    EXPECT_EQ(events[1].document.at("result").at("reason").at("code").get<std::string>(), "idempotency_conflict");
    ASSERT_FALSE(reports.empty());
    const auto conflict_report = std::find_if(
        reports.begin(),
        reports.end(),
        [](const optionx::BridgeSignalReport& report) {
            return report.reason_code == "idempotency_conflict";
        });
    ASSERT_NE(conflict_report, reports.end());
    ASSERT_TRUE(conflict_report->candidate_signal);
    EXPECT_EQ(conflict_report->candidate_signal->symbol, "GBPUSD");
    EXPECT_EQ(conflict_report->candidate_signal->unique_hash, "retry-hash");
}

TEST(MetaTraderFileBridge, DeduplicatesRetryWithCanonicalBusinessPayload) {
    namespace mtfile = optionx::bridges::metatrader_file;
    namespace protocol = optionx::bridges::metatrader_file::detail;

    const auto root = make_temp_root();
    ScopedPathCleanup cleanup(root);

    mtfile::MetaTraderFileBridgeConfig config;
    config.common_files_root = root.u8string();
    config.bridge_id = 40;
    config.client_id = "terminal-canonical-payload";
    config.max_line_bytes = 8192;

    auto first_params = make_signal_submit_params(
        "canonical-key",
        "canonical-hash",
        "EURUSD",
        "PUT",
        protocol::unix_time_ms() + 60000);
    first_params["identity"]["unique_id"] = 42;
    first_params["routing"]["platform_type"] = "intrade.bar";
    first_params["routing"]["selector"]["kind"] = "DEFAULT";
    first_params["routing"]["selector"]["account_id"] = 7;
    first_params["signal"]["option_type"] = "sprint";
    first_params["signal"]["amount"]["value"] = "1.2500";
    first_params["signal"]["amount"]["currency"] = "usd";
    first_params["signal"]["expiry"]["duration_ms"] = "60000";

    auto retry_params = make_signal_submit_params(
        "canonical-key",
        "canonical-hash",
        "EURUSD",
        "BUY",
        protocol::unix_time_ms() + 120000);
    retry_params["identity"]["unique_id"] = "42";
    retry_params["routing"]["platform_type"] = "INTRADE_BAR";
    retry_params["routing"]["selector"]["kind"] = "default";
    retry_params["routing"]["selector"]["account_id"] = "7";
    retry_params["signal"]["option_type"] = "SPRINT";
    retry_params["signal"]["amount"]["value"] = 1.25;
    retry_params["signal"]["amount"]["currency"] = "USD";
    retry_params["signal"]["expiry"]["duration_ms"] = 60000;

    const auto layout = protocol::make_layout(config);
    protocol::ensure_runtime_directories(layout);
    protocol::append_json_line(
        layout.commands_log(),
        protocol::make_file_jsonrpc_request(
            1,
            "request-1",
            "signal.submit",
            first_params),
        config.max_line_bytes);
    protocol::append_json_line(
        layout.commands_log(),
        protocol::make_file_jsonrpc_request(
            2,
            "request-2",
            "signal.submit",
            retry_params),
        config.max_line_bytes);

    mtfile::MetaTraderFileBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<mtfile::MetaTraderFileBridgeConfig>(config)));
    bridge.on_signal_id() = []() {
        return optionx::SignalId{53};
    };

    int signal_count = 0;
    bridge.on_trade_signal() = [&](std::unique_ptr<optionx::TradeSignal>) {
        ++signal_count;
    };
    bridge.process();
    EXPECT_EQ(signal_count, 1);

    const auto events = protocol::read_ndjson_since_file_seq(
        layout.events_log(),
        0,
        config.max_line_bytes);
    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0].document.at("result").at("signal_ref").at("signal_id").get<std::string>(), "53");
    EXPECT_EQ(events[1].document.at("result").at("signal_ref").at("signal_id").get<std::string>(), "53");
}

TEST(MetaTraderFileBridge, RejectsJsonRpcIdReuseWhenBusinessPayloadChanges) {
    namespace mtfile = optionx::bridges::metatrader_file;
    namespace protocol = optionx::bridges::metatrader_file::detail;

    const auto root = make_temp_root();
    ScopedPathCleanup cleanup(root);

    mtfile::MetaTraderFileBridgeConfig config;
    config.common_files_root = root.u8string();
    config.bridge_id = 40;
    config.client_id = "terminal-rpc-id-payload-change";
    config.max_line_bytes = 8192;

    auto changed_params = make_signal_submit_params("same-key", "same-hash");
    changed_params["signal"]["amount"]["value"] = "2.00";

    const auto layout = protocol::make_layout(config);
    protocol::ensure_runtime_directories(layout);
    protocol::append_json_line(
        layout.commands_log(),
        protocol::make_file_jsonrpc_request(
            1,
            "request-1",
            "signal.submit",
            make_signal_submit_params("same-key", "same-hash")),
        config.max_line_bytes);
    protocol::append_json_line(
        layout.commands_log(),
        protocol::make_file_jsonrpc_request(
            2,
            "request-1",
            "signal.submit",
            changed_params),
        config.max_line_bytes);

    mtfile::MetaTraderFileBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<mtfile::MetaTraderFileBridgeConfig>(config)));
    bridge.on_signal_id() = []() {
        return optionx::SignalId{54};
    };

    int signal_count = 0;
    bridge.on_trade_signal() = [&](std::unique_ptr<optionx::TradeSignal>) {
        ++signal_count;
    };
    bridge.process();
    EXPECT_EQ(signal_count, 1);

    const auto events = protocol::read_ndjson_since_file_seq(
        layout.events_log(),
        0,
        config.max_line_bytes);
    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0].document.at("result").at("signal_ref").at("signal_id").get<std::string>(), "54");
    EXPECT_EQ(events[1].document.at("result").at("status").get<std::string>(), "rejected");
    EXPECT_EQ(events[1].document.at("result").at("reason").at("code").get<std::string>(), "idempotency_conflict");
}

TEST(MetaTraderFileBridge, KeepsDurationAndAbsoluteExpiryDistinctForIdempotency) {
    namespace mtfile = optionx::bridges::metatrader_file;
    namespace protocol = optionx::bridges::metatrader_file::detail;

    const auto root = make_temp_root();
    ScopedPathCleanup cleanup(root);

    mtfile::MetaTraderFileBridgeConfig config;
    config.common_files_root = root.u8string();
    config.bridge_id = 40;
    config.client_id = "terminal-expiry-kind";
    config.max_line_bytes = 8192;

    auto absolute_params = make_signal_submit_params("expiry-key", "expiry-hash");
    absolute_params["signal"]["expiry"] = {
        {"kind", "absolute"},
        {"expires_at_ms", ((protocol::unix_time_ms() / 1000) + 60) * 1000}
    };

    const auto layout = protocol::make_layout(config);
    protocol::ensure_runtime_directories(layout);
    protocol::append_json_line(
        layout.commands_log(),
        protocol::make_file_jsonrpc_request(
            1,
            "request-1",
            "signal.submit",
            make_signal_submit_params("expiry-key", "expiry-hash")),
        config.max_line_bytes);
    protocol::append_json_line(
        layout.commands_log(),
        protocol::make_file_jsonrpc_request(
            2,
            "request-2",
            "signal.submit",
            absolute_params),
        config.max_line_bytes);

    mtfile::MetaTraderFileBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<mtfile::MetaTraderFileBridgeConfig>(config)));
    bridge.on_signal_id() = []() {
        return optionx::SignalId{55};
    };

    int signal_count = 0;
    bridge.on_trade_signal() = [&](std::unique_ptr<optionx::TradeSignal>) {
        ++signal_count;
    };
    bridge.process();
    EXPECT_EQ(signal_count, 1);

    const auto events = protocol::read_ndjson_since_file_seq(
        layout.events_log(),
        0,
        config.max_line_bytes);
    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0].document.at("result").at("signal_ref").at("signal_id").get<std::string>(), "55");
    EXPECT_EQ(events[1].document.at("result").at("status").get<std::string>(), "rejected");
    EXPECT_EQ(events[1].document.at("result").at("reason").at("code").get<std::string>(), "idempotency_conflict");
}

TEST(MetaTraderFileBridge, InDoubtIdempotencySuppressesReplayAfterCallbackFailure) {
    namespace mtfile = optionx::bridges::metatrader_file;
    namespace protocol = optionx::bridges::metatrader_file::detail;

    const auto root = make_temp_root();
    ScopedPathCleanup cleanup(root);

    mtfile::MetaTraderFileBridgeConfig config;
    config.common_files_root = root.u8string();
    config.bridge_id = 41;
    config.client_id = "terminal-in-doubt";
    config.max_line_bytes = 8192;

    const auto layout = protocol::make_layout(config);
    protocol::ensure_runtime_directories(layout);
    protocol::append_json_line(
        layout.commands_log(),
        protocol::make_file_jsonrpc_request(
            1,
            "cmd-1",
            "signal.submit",
            make_signal_submit_params("in-doubt", "in-doubt")),
        config.max_line_bytes);

    {
        mtfile::MetaTraderFileBridge bridge;
        ASSERT_TRUE(bridge.configure(std::make_unique<mtfile::MetaTraderFileBridgeConfig>(config)));
        bridge.on_signal_id() = []() {
            return optionx::SignalId{20};
        };

        int signal_count = 0;
        bridge.on_trade_signal() = [&](std::unique_ptr<optionx::TradeSignal>) {
            ++signal_count;
            throw std::runtime_error("simulated downstream crash window");
        };
        bridge.process();
        EXPECT_EQ(signal_count, 1);
    }

    std::filesystem::remove(layout.commands_checkpoint());

    mtfile::MetaTraderFileBridge replay_bridge;
    ASSERT_TRUE(replay_bridge.configure(std::make_unique<mtfile::MetaTraderFileBridgeConfig>(config)));
    replay_bridge.on_signal_id() = []() {
        return optionx::SignalId{21};
    };

    int replay_count = 0;
    replay_bridge.on_trade_signal() = [&](std::unique_ptr<optionx::TradeSignal>) {
        ++replay_count;
    };
    replay_bridge.process();
    EXPECT_EQ(replay_count, 0);

    const auto events = protocol::read_ndjson_since_file_seq(
        layout.events_log(),
        0,
        config.max_line_bytes);
    ASSERT_EQ(events.size(), 1u);

    const auto state = protocol::read_json_file(
        layout.idempotency_state(),
        config.max_idempotency_state_bytes);
    ASSERT_TRUE(state.at("records").is_object());
    ASSERT_EQ(state.at("records").size(), 1u);
    const auto& stored_result =
        state.at("records").begin().value().at("result");
    EXPECT_EQ(stored_result.at("status").get<std::string>(), "in_doubt");
    EXPECT_EQ(stored_result.at("signal_ref").at("signal_id").get<std::string>(), "20");
}

TEST(MetaTraderFileBridge, PrunesIdempotencyRecords) {
    namespace mtfile = optionx::bridges::metatrader_file;
    namespace protocol = optionx::bridges::metatrader_file::detail;

    const auto root = make_temp_root();
    ScopedPathCleanup cleanup(root);

    mtfile::MetaTraderFileBridgeConfig config;
    config.common_files_root = root.u8string();
    config.bridge_id = 42;
    config.client_id = "terminal-idempotency-prune";
    config.max_line_bytes = 8192;
    config.max_idempotency_records = 2;
    config.max_idempotency_state_bytes = 8192;

    const auto layout = protocol::make_layout(config);
    protocol::ensure_runtime_directories(layout);
    const auto stable_deadline = protocol::unix_time_ms() + 60000;
    for (std::uint64_t seq = 1; seq <= 3; ++seq) {
        protocol::append_json_line(
            layout.commands_log(),
            protocol::make_file_jsonrpc_request(
                seq,
                "cmd-" + std::to_string(seq),
                "signal.submit",
                make_signal_submit_params(
                    "prune-" + std::to_string(seq),
                    "prune-" + std::to_string(seq),
                    "EURUSD",
                    "BUY",
                    stable_deadline)),
            config.max_line_bytes);
    }

    mtfile::MetaTraderFileBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<mtfile::MetaTraderFileBridgeConfig>(config)));
    optionx::SignalId next_signal_id = 30;
    bridge.on_signal_id() = [&]() {
        return next_signal_id++;
    };

    int signal_count = 0;
    bridge.on_trade_signal() = [&](std::unique_ptr<optionx::TradeSignal>) {
        ++signal_count;
    };
    bridge.process();
    EXPECT_EQ(signal_count, 3);

    const auto state = protocol::read_json_file(
        layout.idempotency_state(),
        config.max_idempotency_state_bytes);
    ASSERT_TRUE(state.at("records").is_object());
    EXPECT_LE(state.at("records").size(), 2u);
    EXPECT_EQ(state.at("processed_through_file_seq").get<std::uint64_t>(), 3u);

    std::filesystem::remove(layout.commands_checkpoint());

    mtfile::MetaTraderFileBridge replay_bridge;
    ASSERT_TRUE(replay_bridge.configure(std::make_unique<mtfile::MetaTraderFileBridgeConfig>(config)));
    replay_bridge.on_signal_id() = []() {
        return optionx::SignalId{99};
    };

    int replay_count = 0;
    replay_bridge.on_trade_signal() = [&](std::unique_ptr<optionx::TradeSignal>) {
        ++replay_count;
    };
    replay_bridge.process();
    EXPECT_EQ(replay_count, 0);

    protocol::append_json_line(
        layout.commands_log(),
        protocol::make_file_jsonrpc_request(
            4,
            "cmd-4",
            "signal.submit",
            make_signal_submit_params(
                "prune-1",
                "prune-1",
                "EURUSD",
                "BUY",
                stable_deadline)),
        config.max_line_bytes);

    replay_bridge.process();
    EXPECT_EQ(replay_count, 0);

    const auto after_reuse_events = protocol::read_ndjson_since_file_seq(
        layout.events_log(),
        0,
        config.max_line_bytes);
    ASSERT_GE(after_reuse_events.size(), 4u);
    EXPECT_EQ(
        after_reuse_events.back().document.at("result").at("status").get<std::string>(),
        "accepted");
    EXPECT_EQ(
        after_reuse_events.back().document.at("result").at("signal_ref").at("signal_id").get<std::string>(),
        "30");
}

TEST(MetaTraderFileBridge, ExpiresIdempotencyTombstonesAfterRetention) {
    namespace mtfile = optionx::bridges::metatrader_file;
    namespace protocol = optionx::bridges::metatrader_file::detail;

    const auto root = make_temp_root();
    ScopedPathCleanup cleanup(root);

    mtfile::MetaTraderFileBridgeConfig config;
    config.common_files_root = root.u8string();
    config.bridge_id = 45;
    config.client_id = "terminal-idempotency-expiry";
    config.max_line_bytes = 8192;
    config.max_idempotency_records = 2;
    config.max_idempotency_state_bytes = 8192;
    config.idempotency_retention_ms = 24ull * 60ull * 60ull * 1000ull;

    const auto layout = protocol::make_layout(config);
    protocol::ensure_runtime_directories(layout);
    for (std::uint64_t seq = 1; seq <= 3; ++seq) {
        protocol::append_json_line(
            layout.commands_log(),
            protocol::make_file_jsonrpc_request(
                seq,
                "cmd-" + std::to_string(seq),
                "signal.submit",
                make_signal_submit_params(
                    "expire-" + std::to_string(seq),
                    "expire-" + std::to_string(seq))),
            config.max_line_bytes);
    }

    mtfile::MetaTraderFileBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<mtfile::MetaTraderFileBridgeConfig>(config)));
    optionx::SignalId next_signal_id = 70;
    bridge.on_signal_id() = [&]() {
        return next_signal_id++;
    };

    int signal_count = 0;
    bridge.on_trade_signal() = [&](std::unique_ptr<optionx::TradeSignal>) {
        ++signal_count;
    };

    bridge.process();
    EXPECT_EQ(signal_count, 3);

    auto state = protocol::read_json_file(
        layout.idempotency_state(),
        config.max_idempotency_state_bytes);
    ASSERT_TRUE(state.at("tombstones").is_object());
    ASSERT_FALSE(state.at("tombstones").empty());
    for (auto it = state["tombstones"].begin(); it != state["tombstones"].end(); ++it) {
        it.value()["evicted_at_ms"] = 1;
    }
    protocol::write_json_file_atomic(layout.idempotency_state(), state);

    protocol::append_json_line(
        layout.commands_log(),
        protocol::make_file_jsonrpc_request(
            4,
            "cmd-4",
            "signal.submit",
            make_signal_submit_params("expire-1", "expire-1")),
        config.max_line_bytes);

    mtfile::MetaTraderFileBridge replay_bridge;
    ASSERT_TRUE(replay_bridge.configure(std::make_unique<mtfile::MetaTraderFileBridgeConfig>(config)));
    replay_bridge.on_signal_id() = [&]() {
        return next_signal_id++;
    };

    int replay_count = 0;
    replay_bridge.on_trade_signal() = [&](std::unique_ptr<optionx::TradeSignal>) {
        ++replay_count;
    };

    replay_bridge.process();
    EXPECT_EQ(replay_count, 1);

    const auto after = protocol::read_json_file(
        layout.idempotency_state(),
        config.max_idempotency_state_bytes);
    ASSERT_TRUE(after.at("tombstones").is_object());
    for (const auto& item : after.at("tombstones").items()) {
        EXPECT_NE(item.key(), "signal.submit\nexpire-1");
    }
}

TEST(MetaTraderFileBridge, DoesNotPruneInDoubtRecords) {
    namespace mtfile = optionx::bridges::metatrader_file;
    namespace protocol = optionx::bridges::metatrader_file::detail;

    const auto root = make_temp_root();
    ScopedPathCleanup cleanup(root);

    mtfile::MetaTraderFileBridgeConfig config;
    config.common_files_root = root.u8string();
    config.bridge_id = 43;
    config.client_id = "terminal-in-doubt-retention";
    config.max_line_bytes = 8192;
    config.max_idempotency_records = 1;
    config.max_idempotency_state_bytes = 8192;

    const auto layout = protocol::make_layout(config);
    protocol::ensure_runtime_directories(layout);
    for (std::uint64_t seq = 1; seq <= 2; ++seq) {
        protocol::append_json_line(
            layout.commands_log(),
            protocol::make_file_jsonrpc_request(
                seq,
                "cmd-" + std::to_string(seq),
                "signal.submit",
                make_signal_submit_params(
                    "in-doubt-retention-" + std::to_string(seq),
                    "in-doubt-retention-" + std::to_string(seq))),
            config.max_line_bytes);
    }

    mtfile::MetaTraderFileBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<mtfile::MetaTraderFileBridgeConfig>(config)));
    optionx::SignalId next_signal_id = 60;
    bridge.on_signal_id() = [&]() {
        return next_signal_id++;
    };

    int signal_count = 0;
    bridge.on_trade_signal() = [&](std::unique_ptr<optionx::TradeSignal>) {
        ++signal_count;
        throw std::runtime_error("keep operation in doubt");
    };

    EXPECT_THROW(bridge.process(), std::runtime_error);
    EXPECT_EQ(signal_count, 1);

    EXPECT_THROW(bridge.process(), std::runtime_error);
    EXPECT_EQ(signal_count, 1);

    const auto state = protocol::read_json_file(
        layout.idempotency_state(),
        config.max_idempotency_state_bytes);
    ASSERT_TRUE(state.at("records").is_object());
    ASSERT_EQ(state.at("records").size(), 1u);
    EXPECT_EQ(
        state.at("records").begin().value().at("result").at("status").get<std::string>(),
        "in_doubt");
}

TEST(MetaTraderFileBridge, AllowsReentrantTradeUpdatesFromSignalCallback) {
    namespace mtfile = optionx::bridges::metatrader_file;
    namespace protocol = optionx::bridges::metatrader_file::detail;

    const auto root = make_temp_root();
    ScopedPathCleanup cleanup(root);

    mtfile::MetaTraderFileBridgeConfig config;
    config.common_files_root = root.u8string();
    config.bridge_id = 44;
    config.client_id = "terminal-reentrant";
    config.max_line_bytes = 8192;

    const auto layout = protocol::make_layout(config);
    protocol::ensure_runtime_directories(layout);
    protocol::append_json_line(
        layout.commands_log(),
        protocol::make_file_jsonrpc_request(
            1,
            "cmd-1",
            "signal.submit",
            make_signal_submit_params("reentrant", "reentrant")),
        config.max_line_bytes);

    mtfile::MetaTraderFileBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<mtfile::MetaTraderFileBridgeConfig>(config)));
    bridge.on_signal_id() = []() {
        return optionx::SignalId{55};
    };

    bridge.on_trade_signal() = [&](std::unique_ptr<optionx::TradeSignal> signal) {
        ASSERT_TRUE(signal);
        optionx::TradeRequest request;
        request.trade_id = 55;
        request.signal_id = signal->signal_id;
        request.bridge_id = config.bridge_id;
        request.symbol = signal->symbol;
        request.order_type = signal->order_type;
        request.option_type = signal->option_type;
        request.amount = signal->amount;

        optionx::TradeResult result;
        result.trade_id = 55;
        result.trade_state = optionx::TradeState::OPEN_SUCCESS;
        result.amount = signal->amount;
        bridge.update_trade_result(request, result);
    };

    bridge.process();

    const auto events = protocol::read_ndjson_since_file_seq(
        layout.events_log(),
        0,
        config.max_line_bytes);
    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0].document.at("method").get<std::string>(), "trade.updated");
    EXPECT_EQ(events[1].document.at("result").at("status").get<std::string>(), "accepted");
}

TEST(MetaTraderFileBridge, PublishesAccountQueriesAndTradeUpdates) {
    namespace mtfile = optionx::bridges::metatrader_file;
    namespace protocol = optionx::bridges::metatrader_file::detail;

    const auto root = make_temp_root();
    ScopedPathCleanup cleanup(root);

    mtfile::MetaTraderFileBridgeConfig config;
    config.common_files_root = root.u8string();
    config.bridge_id = 32;
    config.client_id = "terminal-02";
    config.max_line_bytes = 8192;

    const auto layout = protocol::make_layout(config);
    protocol::ensure_runtime_directories(layout);

    mtfile::MetaTraderFileBridge bridge;
    ASSERT_TRUE(bridge.configure(std::make_unique<mtfile::MetaTraderFileBridgeConfig>(config)));

    auto account = std::make_shared<TestAccountInfo>();
    account->user_id = 77;
    account->balance = 2048.25;
    account->currency = optionx::CurrencyType::EUR;
    bridge.update_account_info(optionx::AccountInfoUpdate(
        account,
        optionx::AccountUpdateStatus::BALANCE_UPDATED,
        303));

    protocol::append_json_line(
        layout.commands_log(),
        protocol::make_file_jsonrpc_request(
            1,
            "balance-1",
            "account.balance.get"),
        config.max_line_bytes);
    bridge.process();

    optionx::TradeRequest request;
    request.trade_id = 7;
    request.signal_id = 12;
    request.bridge_id = 32;
    request.account_id = 303;
    request.symbol = "BTCUSD";
    request.order_type = optionx::OrderType::SELL;
    request.option_type = optionx::OptionType::SPRINT;
    request.currency = optionx::CurrencyType::EUR;
    request.amount = 3.0;
    request.duration = 120;

    optionx::TradeResult trade_result;
    trade_result.trade_id = 7;
    trade_result.trade_state = optionx::TradeState::OPEN_SUCCESS;
    trade_result.amount = 3.0;
    trade_result.currency = optionx::CurrencyType::EUR;
    trade_result.open_date = 12345;

    bridge.update_trade_result(request, trade_result);

    const auto state = protocol::read_json_file(
        layout.state_snapshot(),
        config.max_line_bytes);
    ASSERT_EQ(state.at("accounts").size(), 1u);
    EXPECT_EQ(state.at("accounts")[0].at("account_id").get<std::string>(), "303");
    EXPECT_EQ(state.at("accounts")[0].at("user_id").get<std::string>(), "77");
    EXPECT_EQ(state.at("accounts")[0].at("balance").at("value").get<std::string>(), "2048.25");
    EXPECT_EQ(state.at("accounts")[0].at("balance").at("currency").get<std::string>(), "EUR");

    const auto events = protocol::read_ndjson_since_file_seq(
        layout.events_log(),
        0,
        config.max_line_bytes);
    ASSERT_EQ(events.size(), 3u);
    EXPECT_EQ(events[0].document.at("method").get<std::string>(), "balance.updated");
    EXPECT_EQ(
        events[0].document.at("params").at("payload").at("account_id").get<std::string>(),
        "303");
    EXPECT_EQ(
        events[0].document.at("params").at("payload").at("user_id").get<std::string>(),
        "77");
    EXPECT_EQ(events[1].document.at("id").get<std::string>(), "balance-1");
    EXPECT_EQ(events[1].document.at("result").at("status").get<std::string>(), "completed");
    EXPECT_EQ(
        events[1].document.at("result").at("account").at("account_id").get<std::string>(),
        "303");
    EXPECT_EQ(
        events[1].document.at("result").at("account").at("user_id").get<std::string>(),
        "77");
    EXPECT_EQ(
        events[1].document.at("result").at("account").at("balance").at("value").get<std::string>(),
        "2048.25");
    EXPECT_EQ(events[2].document.at("method").get<std::string>(), "trade.updated");
    const auto& trade = events[2].document.at("params").at("payload").at("trade");
    EXPECT_EQ(trade.at("trade_id").get<std::string>(), "7");
    EXPECT_EQ(trade.at("account_id").get<std::string>(), "303");
    EXPECT_EQ(trade.at("state").get<std::string>(), "opened");
    EXPECT_FALSE(trade.at("final").get<bool>());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
