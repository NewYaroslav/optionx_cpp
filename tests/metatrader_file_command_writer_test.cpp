#include <gtest/gtest.h>

#include <optionx_cpp/bridges.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <set>
#include <system_error>
#include <thread>
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
           ("optionx_metatrader_file_writer_test_" + std::to_string(stamp));
}

optionx::bridges::metatrader_file::MetaTraderFileBridgeConfig make_config(
        const std::filesystem::path& root) {
    optionx::bridges::metatrader_file::MetaTraderFileBridgeConfig config;
    config.common_files_root = root.u8string();
    config.bridge_id = 42;
    config.client_id = "terminal-01";
    config.max_line_bytes = 8192;
    return config;
}

std::string make_filler_line(
        const std::uint64_t file_seq,
        const std::size_t target_line_bytes) {
    const std::string prefix =
        "{\"file_seq\":" + std::to_string(file_seq) +
        ",\"jsonrpc\":\"2.0\",\"id\":\"fill\",\"method\":\"account.balance.get\",\"params\":{\"padding\":\"";
    const std::string suffix = "\"}}";
    if (target_line_bytes <= prefix.size() + suffix.size()) {
        throw std::runtime_error("filler line target is too small");
    }
    return prefix + std::string(target_line_bytes - prefix.size() - suffix.size(), 'x') + suffix;
}

void write_text_append(
        const std::filesystem::path& file,
        const std::string& text) {
    std::ofstream out(file, std::ios::binary | std::ios::app);
    ASSERT_TRUE(out);
    out << text;
}

} // namespace

TEST(MetaTraderFileCommandWriter, AppendsCanonicalCommands) {
    namespace mtfile = optionx::bridges::metatrader_file;
    namespace protocol = optionx::bridges::metatrader_file::detail;

    const auto root = make_temp_root();
    ScopedPathCleanup cleanup(root);

    auto writer = mtfile::MetaTraderFileCommandWriter(make_config(root));

    mtfile::MetaTraderFileTradeCommand signal;
    signal.symbol = "EURUSD";
    signal.order_type = "BUY";
    signal.amount_value = "1.25";
    signal.currency = "USD";
    signal.duration_ms = 300000;
    signal.signal_name = "writer_test";
    signal.account_id = "7";
    signal.id = "cmd-signal";
    signal.idempotency_key = "idem-signal";
    signal.unique_hash = "signal-hash";
    signal.valid_until_ms = protocol::unix_time_ms() + 60000;

    const auto signal_written = writer.signal_submit(signal);
    EXPECT_EQ(signal_written.file_seq, 1u);
    EXPECT_EQ(signal_written.id, "cmd-signal");
    EXPECT_EQ(signal_written.idempotency_key, "idem-signal");

    mtfile::MetaTraderFileTradeCommand trade = signal;
    trade.id = "cmd-trade";
    trade.idempotency_key = "idem-trade";
    trade.unique_hash = "trade-hash";
    const auto trade_written = writer.trade_open(trade);
    EXPECT_EQ(trade_written.file_seq, 2u);

    const auto balance_written = writer.account_balance_get("7", "cmd-balance");
    EXPECT_EQ(balance_written.file_seq, 3u);
    EXPECT_TRUE(balance_written.idempotency_key.empty());

    const auto records = protocol::read_ndjson_since_file_seq(
        writer.layout().commands_log(),
        0,
        8192);
    ASSERT_EQ(records.size(), 3u);

    const auto& signal_doc = records[0].document;
    EXPECT_EQ(signal_doc.at("file_seq").get<std::uint64_t>(), 1u);
    EXPECT_EQ(signal_doc.at("jsonrpc").get<std::string>(), "2.0");
    EXPECT_EQ(signal_doc.at("id").get<std::string>(), "cmd-signal");
    EXPECT_EQ(signal_doc.at("method").get<std::string>(), "signal.submit");
    const auto& signal_params = signal_doc.at("params");
    EXPECT_EQ(signal_params.at("context").at("idempotency_key").get<std::string>(), "idem-signal");
    EXPECT_GE(signal_params.at("context").at("valid_until_ms").get<std::int64_t>(), signal.valid_until_ms);
    EXPECT_EQ(signal_params.at("identity").at("unique_hash").get<std::string>(), "signal-hash");
    EXPECT_EQ(signal_params.at("identity").at("signal_name").get<std::string>(), "writer_test");
    EXPECT_EQ(signal_params.at("routing").at("selector").at("account_id").get<std::string>(), "7");
    EXPECT_EQ(signal_params.at("signal").at("amount").at("value").get<std::string>(), "1.25");
    EXPECT_EQ(signal_params.at("signal").at("expiry").at("duration_ms").get<std::uint64_t>(), 300000u);

    EXPECT_EQ(records[1].document.at("method").get<std::string>(), "trade.open");
    EXPECT_EQ(records[1].document.at("params").at("trade").at("symbol").get<std::string>(), "EURUSD");
    EXPECT_EQ(records[1].document.at("params").at("context").at("idempotency_key").get<std::string>(), "idem-trade");

    EXPECT_EQ(records[2].document.at("method").get<std::string>(), "account.balance.get");
    EXPECT_EQ(records[2].document.at("params").at("account_id").get<std::string>(), "7");
}

TEST(MetaTraderFileCommandWriter, RecoversSequenceAndCleansAfterCheckpoint) {
    namespace mtfile = optionx::bridges::metatrader_file;
    namespace protocol = optionx::bridges::metatrader_file::detail;

    const auto root = make_temp_root();
    ScopedPathCleanup cleanup(root);

    const auto config = make_config(root);
    mtfile::MetaTraderFileCommandWriter writer(config);
    ASSERT_EQ(writer.account_balance_get({}, "cmd-1").file_seq, 1u);
    ASSERT_EQ(writer.account_balance_get({}, "cmd-2").file_seq, 2u);

    mtfile::MetaTraderFileCommandWriter recovered(config);
    EXPECT_EQ(recovered.next_file_seq(), 3u);
    EXPECT_FALSE(recovered.clear_commands_if_checkpoint_caught_up());

    protocol::write_json_file_atomic(
        recovered.layout().commands_checkpoint(),
        protocol::make_log_checkpoint(2));

    EXPECT_TRUE(recovered.clear_commands_if_checkpoint_caught_up());
    EXPECT_EQ(protocol::max_file_seq_in_ndjson(recovered.layout().commands_log(), 8192), 0u);
    EXPECT_EQ(recovered.next_file_seq(), 3u);

    ASSERT_EQ(recovered.account_balance_get({}, "cmd-3").file_seq, 3u);
}

TEST(MetaTraderFileCommandWriter, FailsClosedOnMalformedCheckpoint) {
    namespace mtfile = optionx::bridges::metatrader_file;
    namespace protocol = optionx::bridges::metatrader_file::detail;

    const auto root = make_temp_root();
    ScopedPathCleanup cleanup(root);

    const auto config = make_config(root);
    const auto layout = protocol::make_layout(config);
    protocol::ensure_runtime_directories(layout);
    protocol::write_text_file_atomic(layout.commands_checkpoint(), "{\"not_last_file_seq\":1}");

    mtfile::MetaTraderFileCommandWriter writer(config);
    EXPECT_THROW(
        static_cast<void>(writer.account_balance_get({}, "cmd-1")),
        std::runtime_error);
    EXPECT_EQ(protocol::max_file_seq_in_ndjson(layout.commands_log(), 8192), 0u);
}

TEST(MetaTraderFileCommandWriter, RepairsIncompleteTailBeforeAppend) {
    namespace mtfile = optionx::bridges::metatrader_file;
    namespace protocol = optionx::bridges::metatrader_file::detail;

    const auto root = make_temp_root();
    ScopedPathCleanup cleanup(root);

    const auto config = make_config(root);
    mtfile::MetaTraderFileCommandWriter writer(config);
    ASSERT_EQ(writer.account_balance_get({}, "cmd-1").file_seq, 1u);

    {
        std::ofstream out(writer.layout().commands_log(), std::ios::binary | std::ios::app);
        ASSERT_TRUE(out);
        out << "{\"file_seq\":2,\"jsonrpc\":\"2.0\",\"method\":\"trade.op";
    }

    ASSERT_EQ(writer.account_balance_get({}, "cmd-2").file_seq, 2u);
    const auto records = protocol::read_ndjson_since_file_seq(
        writer.layout().commands_log(),
        0,
        8192);

    ASSERT_EQ(records.size(), 2u);
    EXPECT_EQ(records[0].file_seq, 1u);
    EXPECT_EQ(records[1].file_seq, 2u);
    EXPECT_EQ(records[1].document.at("id").get<std::string>(), "cmd-2");
}

TEST(MetaTraderFileCommandWriter, SerializesConcurrentAppends) {
    namespace mtfile = optionx::bridges::metatrader_file;
    namespace protocol = optionx::bridges::metatrader_file::detail;

    const auto root = make_temp_root();
    ScopedPathCleanup cleanup(root);

    mtfile::MetaTraderFileCommandWriter writer(make_config(root));
    std::mutex errors_mutex;
    std::vector<std::string> errors;
    std::vector<std::thread> threads;

    constexpr int command_count = 24;
    threads.reserve(command_count);
    for (int i = 0; i < command_count; ++i) {
        threads.emplace_back([&writer, &errors, &errors_mutex, i]() {
            try {
                writer.account_balance_get({}, "cmd-" + std::to_string(i));
            } catch (const std::exception& ex) {
                std::lock_guard<std::mutex> lock(errors_mutex);
                errors.push_back(ex.what());
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }

    if (!errors.empty()) {
        FAIL() << errors.front();
    }
    const auto records = protocol::read_ndjson_since_file_seq(
        writer.layout().commands_log(),
        0,
        8192);
    ASSERT_EQ(records.size(), static_cast<std::size_t>(command_count));

    std::set<std::uint64_t> sequences;
    for (const auto& record : records) {
        sequences.insert(record.file_seq);
    }
    EXPECT_EQ(sequences.size(), static_cast<std::size_t>(command_count));
    EXPECT_EQ(*sequences.begin(), 1u);
    EXPECT_EQ(*sequences.rbegin(), static_cast<std::uint64_t>(command_count));
}

TEST(MetaTraderFileCommandWriter, RejectsAppendThatWouldExceedCommandLogLimit) {
    namespace mtfile = optionx::bridges::metatrader_file;
    namespace protocol = optionx::bridges::metatrader_file::detail;

    const auto root = make_temp_root();
    ScopedPathCleanup cleanup(root);

    auto config = make_config(root);
    config.max_line_bytes = 512;
    config.max_command_log_bytes = config.max_line_bytes;
    const auto layout = protocol::make_layout(config);
    protocol::ensure_runtime_directories(layout);
    write_text_append(layout.commands_log(), make_filler_line(1, config.max_line_bytes - 1) + "\n");
    ASSERT_EQ(std::filesystem::file_size(layout.commands_log()), config.max_command_log_bytes);

    mtfile::MetaTraderFileCommandWriter writer(config);
    EXPECT_THROW(
        static_cast<void>(writer.account_balance_get({}, "cmd-over-limit")),
        std::runtime_error);

    const auto records = protocol::read_ndjson_since_file_seq(
        layout.commands_log(),
        0,
        config.max_line_bytes);
    ASSERT_EQ(records.size(), 1u);
    EXPECT_EQ(records[0].file_seq, 1u);
}

TEST(MetaTraderFileCommandWriter, ClearsCaughtUpFullLogBeforeAppend) {
    namespace mtfile = optionx::bridges::metatrader_file;
    namespace protocol = optionx::bridges::metatrader_file::detail;

    const auto root = make_temp_root();
    ScopedPathCleanup cleanup(root);

    auto config = make_config(root);
    config.max_line_bytes = 512;
    config.max_command_log_bytes = config.max_line_bytes;
    const auto layout = protocol::make_layout(config);
    protocol::ensure_runtime_directories(layout);
    write_text_append(layout.commands_log(), make_filler_line(1, config.max_line_bytes - 1) + "\n");
    protocol::write_json_file_atomic(layout.commands_checkpoint(), protocol::make_log_checkpoint(1));

    mtfile::MetaTraderFileCommandWriter writer(config);
    const auto written = writer.account_balance_get({}, "cmd-after-cleanup");
    EXPECT_EQ(written.file_seq, 2u);

    const auto records = protocol::read_ndjson_since_file_seq(
        layout.commands_log(),
        0,
        config.max_line_bytes);
    ASSERT_EQ(records.size(), 1u);
    EXPECT_EQ(records[0].file_seq, 2u);
    EXPECT_EQ(records[0].document.at("id").get<std::string>(), "cmd-after-cleanup");
}

TEST(MetaTraderFileCommandWriter, SerializesIndependentWriterInstances) {
    namespace mtfile = optionx::bridges::metatrader_file;
    namespace protocol = optionx::bridges::metatrader_file::detail;

    const auto root = make_temp_root();
    ScopedPathCleanup cleanup(root);

    const auto config = make_config(root);
    std::mutex errors_mutex;
    std::vector<std::string> errors;
    std::vector<std::thread> threads;

    constexpr int command_count = 24;
    threads.reserve(command_count);
    for (int i = 0; i < command_count; ++i) {
        threads.emplace_back([config, &errors, &errors_mutex, i]() {
            try {
                mtfile::MetaTraderFileCommandWriter writer(config);
                writer.account_balance_get({}, "cmd-instance-" + std::to_string(i));
            } catch (const std::exception& ex) {
                std::lock_guard<std::mutex> lock(errors_mutex);
                errors.push_back(ex.what());
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }

    if (!errors.empty()) {
        FAIL() << errors.front();
    }

    const auto records = protocol::read_ndjson_since_file_seq(
        protocol::make_layout(config).commands_log(),
        0,
        config.max_line_bytes);
    ASSERT_EQ(records.size(), static_cast<std::size_t>(command_count));

    std::set<std::uint64_t> sequences;
    for (const auto& record : records) {
        sequences.insert(record.file_seq);
    }
    EXPECT_EQ(sequences.size(), static_cast<std::size_t>(command_count));
    EXPECT_EQ(*sequences.begin(), 1u);
    EXPECT_EQ(*sequences.rbegin(), static_cast<std::uint64_t>(command_count));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
