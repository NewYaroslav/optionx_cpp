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

std::filesystem::path make_unique_smoke_root() {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() /
           ("optionx-metatrader-file-end-to-end-smoke-" + std::to_string(stamp));
}

MetaTraderFileBridgeConfig default_config() {
    MetaTraderFileBridgeConfig config;
    config.bridge_id = 1;
    config.client_id = "e2e-smoke";
    config.poll_interval_ms = 100;
    config.max_line_bytes = 8192;
    config.max_scanned_records_per_poll = 128;
    config.max_returned_records_per_poll = 32;
    if (config.common_files_root.empty()) {
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

std::vector<nlohmann::json> read_json_lines(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return {};
    }

    std::vector<nlohmann::json> result;
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty()) {
            result.push_back(nlohmann::json::parse(line));
        }
    }
    return result;
}

bool has_request(
        const std::vector<nlohmann::json>& records,
        const std::string& id,
        const std::string& method) {
    for (const auto& record : records) {
        if (record.value("id", std::string()) == id &&
            record.value("method", std::string()) == method) {
            return true;
        }
    }
    return false;
}

bool has_result_status(
        const std::vector<nlohmann::json>& records,
        const std::string& id,
        const std::string& status) {
    for (const auto& record : records) {
        if (record.value("id", std::string()) != id ||
            !record.contains("result") ||
            !record.at("result").is_object()) {
            continue;
        }
        if (record.at("result").value("status", std::string()) == status) {
            return true;
        }
    }
    return false;
}

bool has_method(
        const std::vector<nlohmann::json>& records,
        const std::string& method) {
    for (const auto& record : records) {
        if (record.value("method", std::string()) == method) {
            return true;
        }
    }
    return false;
}

bool has_signal_name(
        const std::vector<nlohmann::json>& signals,
        const std::string& signal_name) {
    for (const auto& signal : signals) {
        if (signal.value("signal_name", std::string()) == signal_name) {
            return true;
        }
    }
    return false;
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
        << "Runs a local file-transport loopback: command writer -> bridge -> events.ndjson.\n"
        << "--self-test always overrides filesystem root with a unique temp directory.\n";
}

} // namespace

int main(int argc, char** argv) {
    if (has_arg(argc, argv, "--help")) {
        print_usage();
        return 0;
    }

    const bool self_test = has_arg(argc, argv, "--self-test");
    auto config = default_config();
    if (!load_config(option_value(argc, argv, "--config"), config)) {
        return 2;
    }

    if (self_test) {
        config.common_files_root = make_unique_smoke_root().u8string();
        config.client_id = "e2e-smoke";
    }

    const auto validation = config.validate();
    if (!validation.first) {
        std::cerr << "Invalid config: " << validation.second << '\n';
        return 2;
    }

    if (self_test) {
        std::error_code ec;
        std::filesystem::remove_all(config.client_root(), ec);
        if (ec) {
            std::cerr << "Could not clean self-test root: "
                      << config.client_root().u8string()
                      << ": "
                      << ec.message()
                      << '\n';
            return 2;
        }
    }

    try {
        MetaTraderFileBridge bridge;
        if (!bridge.configure(std::make_unique<MetaTraderFileBridgeConfig>(config))) {
            std::cerr << "Bridge configuration failed\n";
            return 2;
        }

        optionx::SignalId next_signal_id = 1;
        std::vector<nlohmann::json> received_signals;
        std::vector<std::string> report_codes;

        bridge.on_signal_id() = [&next_signal_id]() {
            return next_signal_id++;
        };
        bridge.on_trade_signal() =
            [&received_signals](std::unique_ptr<optionx::TradeSignal> signal) {
                nlohmann::json json = *signal;
                received_signals.push_back(json);
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

        const auto commands = read_json_lines(writer.layout().commands_log());
        const auto event_records = read_json_lines(writer.layout().events_log());
        const auto events = read_text_file(writer.layout().events_log());
        std::cout << "events.ndjson:\n" << events;

        if (self_test) {
            const bool ok =
                received_signals.size() == 2u &&
                has_signal_name(received_signals, "e2e_signal") &&
                has_signal_name(received_signals, "e2e_trade") &&
                has_request(commands, "e2e-balance", "account.balance.get") &&
                has_request(commands, "e2e-signal", "signal.submit") &&
                has_request(commands, "e2e-trade", "trade.open") &&
                has_method(event_records, "balance.updated") &&
                has_result_status(event_records, "e2e-balance", "completed") &&
                has_result_status(event_records, "e2e-signal", "accepted") &&
                has_result_status(event_records, "e2e-trade", "accepted");
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
