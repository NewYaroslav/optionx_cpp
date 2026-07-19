#include <optionx_cpp/bridges/metatrader_file.hpp>
#include <optionx_cpp/utils/json_comments.hpp>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

namespace {

using optionx::bridges::metatrader_file::MetaTraderFileBridge;
using optionx::bridges::metatrader_file::MetaTraderFileBridgeConfig;
using optionx::bridges::metatrader_file::MetaTraderFileCommandWriter;
using optionx::bridges::metatrader_file::MetaTraderFileTradeCommand;

class DemoAccountInfo final : public optionx::BaseAccountInfoData {
public:
    std::int64_t user_id = 77;
    double balance = 2048.25;
    optionx::CurrencyType currency = optionx::CurrencyType::USD;
    optionx::AccountType account_type = optionx::AccountType::DEMO;
    bool connected = true;

    std::unique_ptr<optionx::BaseAccountInfoData> clone_unique() const override {
        return std::make_unique<DemoAccountInfo>(*this);
    }

    std::shared_ptr<optionx::BaseAccountInfoData> clone_shared() const override {
        return std::make_shared<DemoAccountInfo>(*this);
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

bool has_arg(int argc, char** argv, const std::string& value) {
    for (int i = 1; i < argc; ++i) {
        if (argv[i] == value) {
            return true;
        }
    }
    return false;
}

std::string option_value(int argc, char** argv, const std::string& name) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (argv[i] == name) {
            return argv[i + 1];
        }
    }
    return {};
}

std::filesystem::path smoke_root() {
    return std::filesystem::temp_directory_path() /
           "optionx-metatrader-file-end-to-end-smoke";
}

MetaTraderFileBridgeConfig default_config(const bool self_test) {
    MetaTraderFileBridgeConfig config;
    config.bridge_id = 1;
    config.client_id = "e2e-smoke";
    config.poll_interval_ms = 100;
    config.max_line_bytes = 8192;
    config.max_scanned_records_per_poll = 128;
    config.max_returned_records_per_poll = 32;
    if (self_test || config.common_files_root.empty()) {
        config.common_files_root = smoke_root().u8string();
    }
    return config;
}

bool load_config(const std::string& path, MetaTraderFileBridgeConfig& config) {
    if (path.empty()) {
        return true;
    }

    std::ifstream input(path);
    if (!input) {
        std::cerr << "Could not open config: " << path << '\n';
        return false;
    }

    try {
        std::ostringstream buffer;
        buffer << input.rdbuf();
        config.from_json(optionx::utils::parse_json_with_comments(buffer.str()));
    } catch (const std::exception& ex) {
        std::cerr << "Could not parse config: " << ex.what() << '\n';
        return false;
    }
    return true;
}

std::string read_text_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return {};
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

MetaTraderFileTradeCommand make_trade_command(
        std::string id,
        std::string idempotency_key,
        std::string signal_name) {
    MetaTraderFileTradeCommand command;
    command.symbol = "EURUSD";
    command.order_type = "BUY";
    command.option_type = "SPRINT";
    command.amount_value = "1.00";
    command.currency = "USD";
    command.duration_ms = 60000;
    command.signal_name = std::move(signal_name);
    command.account_id = "77";
    command.id = std::move(id);
    command.idempotency_key = std::move(idempotency_key);
    command.unique_hash = command.idempotency_key;
    return command;
}

void print_usage() {
    std::cout
        << "Usage: metatrader_file_end_to_end_smoke [--self-test] [--config path]\n"
        << "Runs a local file-transport loopback: command writer -> bridge -> events.ndjson.\n";
}

} // namespace

int main(int argc, char** argv) {
    if (has_arg(argc, argv, "--help")) {
        print_usage();
        return 0;
    }

    const bool self_test = has_arg(argc, argv, "--self-test");
    auto config = default_config(self_test);
    if (!load_config(option_value(argc, argv, "--config"), config)) {
        return 2;
    }

    const auto validation = config.validate();
    if (!validation.first) {
        std::cerr << "Invalid config: " << validation.second << '\n';
        return 2;
    }

    if (self_test) {
        std::error_code ec;
        std::filesystem::remove_all(config.client_root(), ec);
    }

    try {
        MetaTraderFileBridge bridge;
        if (!bridge.configure(std::make_unique<MetaTraderFileBridgeConfig>(config))) {
            std::cerr << "Bridge configuration failed\n";
            return 2;
        }

        optionx::SignalId next_signal_id = 1;
        std::vector<std::string> received_symbols;
        std::vector<std::string> report_codes;

        bridge.on_signal_id() = [&next_signal_id]() {
            return next_signal_id++;
        };
        bridge.on_trade_signal() =
            [&received_symbols](std::unique_ptr<optionx::TradeSignal> signal) {
                nlohmann::json json = *signal;
                received_symbols.push_back(json.value("symbol", std::string()));
                std::cout << "received TradeSignal:\n" << json.dump(2) << '\n';
            };
        bridge.on_signal_report() =
            [&report_codes](const optionx::BridgeSignalReport& report) {
                nlohmann::json json = report;
                report_codes.push_back(json.value("reason_code", std::string()));
                std::cout << "signal report: " << json.dump(-1) << '\n';
            };
        bridge.on_status_update() = [](const optionx::BridgeStatusUpdate& update) {
            std::cout << "bridge status=" << optionx::to_str(update.status);
            if (!update.message.empty()) {
                std::cout << " message=" << update.message;
            }
            std::cout << '\n';
        };

        auto account = std::make_shared<DemoAccountInfo>();
        bridge.update_account_info(optionx::AccountInfoUpdate(
            account,
            optionx::AccountUpdateStatus::BALANCE_UPDATED));

        MetaTraderFileCommandWriter writer(config);
        const auto balance = writer.account_balance_get("77", "e2e-balance");
        const auto signal = writer.signal_submit(make_trade_command(
            "e2e-signal",
            "e2e-signal-idempotency",
            "e2e_signal"));
        const auto trade = writer.trade_open(make_trade_command(
            "e2e-trade",
            "e2e-trade-idempotency",
            "e2e_trade"));

        std::cout << "MetaTrader file root: "
                  << writer.layout().root.u8string() << '\n';
        std::cout << "commands.ndjson: "
                  << writer.layout().commands_log().u8string() << '\n';
        std::cout << "events.ndjson: "
                  << writer.layout().events_log().u8string() << '\n';
        std::cout << "written file_seq: "
                  << balance.file_seq << ", "
                  << signal.file_seq << ", "
                  << trade.file_seq << '\n';

        bridge.process();

        const auto events = read_text_file(writer.layout().events_log());
        std::cout << "events.ndjson:\n" << events;

        if (self_test) {
            const bool ok =
                received_symbols.size() == 2u &&
                events.find("\"method\":\"balance.updated\"") != std::string::npos &&
                events.find("\"id\":\"e2e-balance\"") != std::string::npos &&
                events.find("\"status\":\"completed\"") != std::string::npos &&
                events.find("\"id\":\"e2e-signal\"") != std::string::npos &&
                events.find("\"id\":\"e2e-trade\"") != std::string::npos &&
                events.find("\"status\":\"accepted\"") != std::string::npos;
            if (!ok) {
                std::cerr << "Self-test did not observe the expected end-to-end records.\n";
                return 3;
            }
        }
    } catch (const std::exception& ex) {
        std::cerr << "End-to-end smoke failed: " << ex.what() << '\n';
        return 1;
    }

    return 0;
}
