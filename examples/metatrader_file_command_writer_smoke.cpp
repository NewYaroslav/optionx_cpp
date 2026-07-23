#include "example_utils.hpp"

#include <optionx_cpp/bridges/metatrader_file.hpp>

#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>

namespace {

using optionx::bridges::metatrader_file::MetaTraderFileBridgeConfig;
using optionx::bridges::metatrader_file::MetaTraderFileCommandWriter;
using optionx::bridges::metatrader_file::MetaTraderFileTradeCommand;

std::filesystem::path smoke_root();
MetaTraderFileBridgeConfig default_config(bool self_test);
void print_usage();

} // namespace

int main(int argc, char** argv) {
    if (optionx::examples::has_arg(argc, argv, "--help")) {
        print_usage();
        return 0;
    }

    const bool self_test = optionx::examples::has_arg(argc, argv, "--self-test");
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

    if (self_test) {
        std::error_code ec;
        std::filesystem::remove_all(std::filesystem::u8path(config.common_files_root), ec);
    }

    try {
        MetaTraderFileCommandWriter writer(config);

        // This companion writes the same JSON-RPC command format that MQL
        // scripts append to Common\Files when a real terminal is connected.
        const auto balance = writer.account_balance_get("7", "smoke-balance");

        MetaTraderFileTradeCommand signal;
        signal.symbol = "EURUSD";
        signal.order_type = "BUY";
        signal.amount_value = "1.00";
        signal.currency = "USD";
        signal.duration_ms = 60000;
        signal.signal_name = "writer_smoke";
        signal.account_id = "7";
        signal.id = "smoke-signal";
        signal.idempotency_key = "smoke-signal-idem";
        signal.unique_hash = "smoke-signal-hash";
        const auto submitted = writer.signal_submit(signal);

        MetaTraderFileTradeCommand trade = signal;
        trade.id = "smoke-trade";
        trade.idempotency_key = "smoke-trade-idem";
        trade.unique_hash = "smoke-trade-hash";
        const auto opened = writer.trade_open(trade);

        const auto content = optionx::examples::read_text_file(writer.layout().commands_log());
        std::cout << "MetaTrader command writer root: "
                  << writer.layout().root.u8string() << '\n';
        std::cout << "Commands log: "
                  << writer.layout().commands_log().u8string() << '\n';
        std::cout << "Wrote file_seq values: "
                  << balance.file_seq << ", "
                  << submitted.file_seq << ", "
                  << opened.file_seq << '\n';
        std::cout << content;

        if (self_test) {
            const bool ok =
                content.find("\"account.balance.get\"") != std::string::npos &&
                content.find("\"signal.submit\"") != std::string::npos &&
                content.find("\"trade.open\"") != std::string::npos &&
                content.find("\"file_seq\"") != std::string::npos &&
                content.find("\"idempotency_key\"") != std::string::npos &&
                content.find("\"valid_until_ms\"") != std::string::npos;
            if (!ok) {
                std::cerr << "Self-test did not find the expected command fields.\n";
                return 3;
            }
        }
    } catch (const std::exception& ex) {
        std::cerr << "Smoke failed: " << ex.what() << '\n';
        return 1;
    }

    return 0;
}

namespace {

std::filesystem::path smoke_root() {
    return std::filesystem::temp_directory_path() /
           "optionx-metatrader-file-command-writer-smoke";
}

MetaTraderFileBridgeConfig default_config(const bool self_test) {
    MetaTraderFileBridgeConfig config;
    config.bridge_id = 1;
    config.client_id = "mql-smoke";
    if (self_test || config.common_files_root.empty()) {
        config.common_files_root = smoke_root().u8string();
    }
    return config;
}

void print_usage() {
    std::cout
        << "Usage: metatrader_file_command_writer_smoke [--self-test] [--config path]\n"
        << "Writes account.balance.get, signal.submit and trade.open commands to commands.ndjson.\n";
}

} // namespace
