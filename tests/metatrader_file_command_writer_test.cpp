#include <gtest/gtest.h>

#include <optionx_cpp/bridges.hpp>

#include <chrono>
#include <filesystem>
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

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
