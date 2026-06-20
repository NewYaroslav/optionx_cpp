#define OPTIONX_LOG_UNIQUE_FILE_INDEX 2

#include "IntradeBarSmokeSupport.hpp"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <map>

namespace smoke = optionx::tests::intrade_bar_smoke;

namespace {

struct CliOptions {
    std::string command = "help";
    std::map<std::string, std::string> values;
    bool confirm = false;
    bool allow_real = false;
};

void print_usage(std::ostream& out) {
    out
        << "Usage:\n"
        << "  intrade_bar_smoke_cli auth\n"
        << "  intrade_bar_smoke_cli auth-cache\n"
        << "  intrade_bar_smoke_cli show-account\n"
        << "  intrade_bar_smoke_cli domain-check [--domain-min=0] [--domain-max=1000]\n"
        << "  intrade_bar_smoke_cli quotes [--symbol=EURUSD]\n"
        << "  intrade_bar_smoke_cli switch-check --confirm [--account-type=DEMO] [--currency=USD]\n"
        << "  intrade_bar_smoke_cli open-trade --confirm [--symbol=EURUSD] [--amount=1] [--duration=60] [--buy|--sell]\n"
        << "\n"
        << "Configuration comes from environment variables or OPTIONX_INTRADE_BAR_CONFIG_FILE.\n"
        << "Credentials always require proxy settings before any broker request is sent.\n";
}

CliOptions parse_args(int argc, char** argv) {
    CliOptions options;
    if (argc >= 2) options.command = argv[1];
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--confirm") {
            options.confirm = true;
        } else if (arg == "--allow-real") {
            options.allow_real = true;
        } else if (arg == "--buy") {
            options.values["order"] = "BUY";
        } else if (arg == "--sell") {
            options.values["order"] = "SELL";
        } else if (arg.rfind("--", 0) == 0) {
            const auto eq = arg.find('=');
            if (eq != std::string::npos) {
                options.values[arg.substr(2, eq - 2)] = arg.substr(eq + 1);
            }
        }
    }
    return options;
}

bool restore_settings_direct(
        const smoke::IntradeBarSmokeConfig& config,
        std::ostream& out,
        std::ostream& err) {
    out << "restore_direct=1\n";
    smoke::IntradeBarSmokeRuntime runtime(config);
    if (!smoke::connect_or_report(runtime, out, err)) return false;
    const bool restored = runtime.has_account_settings(
        config.account_type,
        config.currency);
    out << "restore_ok=" << restored << ' ';
    smoke::print_account(out, runtime.platform());
    runtime.disconnect();
    return restored;
}

int run_auth(smoke::IntradeBarSmokeConfig config) {
    if (!smoke::require_live_config(config, std::cerr)) return 2;
    smoke::IntradeBarSmokeRuntime runtime(std::move(config));
    if (!smoke::connect_or_report(runtime, std::cout, std::cerr)) return 1;
    smoke::print_account(std::cout, runtime.platform());
    runtime.disconnect();
    return 0;
}

int run_auth_cache(smoke::IntradeBarSmokeConfig config) {
    if (!smoke::require_live_config(config, std::cerr)) return 2;
    smoke::remove_saved_session(config);

    smoke::ConnectAttempt fresh;
    {
        smoke::IntradeBarSmokeRuntime runtime(config);
        fresh = runtime.connect();
        runtime.disconnect();
    }
    if (!fresh.success) {
        std::cerr << "fresh auth failed: " << fresh.reason << '\n';
        return 1;
    }

    const bool session_saved = smoke::saved_session(config).has_value();
    smoke::ConnectAttempt cached;
    {
        smoke::IntradeBarSmokeRuntime runtime(config);
        cached = runtime.connect();
        runtime.disconnect();
    }
    if (!cached.success) {
        std::cerr << "cached auth failed: " << cached.reason << '\n';
        return 1;
    }

    std::cout << "fresh_ms=" << fresh.elapsed_ms
              << " cached_ms=" << cached.elapsed_ms
              << " session_saved=" << session_saved
              << " faster=" << (cached.elapsed_ms < fresh.elapsed_ms)
              << '\n';
    return cached.elapsed_ms < fresh.elapsed_ms ? 0 : 3;
}

int run_domain_check(smoke::IntradeBarSmokeConfig config, const CliOptions& options) {
    if (!smoke::require_live_config(config, std::cerr)) return 2;

    config.auto_find_domain = true;
    config.domain_index_min = static_cast<int>(smoke::option_i64_or(
        options.values,
        "domain-min",
        config.domain_index_min));
    config.domain_index_max = static_cast<int>(smoke::option_i64_or(
        options.values,
        "domain-max",
        config.domain_index_max));
    const int64_t timeout_ms = smoke::option_i64_or(
        options.values,
        "timeout-ms",
        config.auth_timeout_ms);

    std::cout << "domain_check auto_find_domain=" << config.auto_find_domain
              << " domain_min=" << config.domain_index_min
              << " domain_max=" << config.domain_index_max
              << " timeout_ms=" << timeout_ms << '\n';

    smoke::IntradeBarSmokeRuntime runtime(config);
    const auto connect = runtime.connect(timeout_ms);
    std::cout << "auth callback=" << connect.callback_received
              << " success=" << connect.success
              << " elapsed_ms=" << connect.elapsed_ms << '\n';

    const auto selection = runtime.latest_domain_selection();
    std::cout << "domain selected=" << selection.received
              << " success=" << selection.success
              << " host=" << selection.selected_host << '\n';

    if (!connect.success) {
        std::cerr << "auth failed: " << connect.reason << '\n';
        runtime.disconnect();
        return 1;
    }

    smoke::print_account(std::cout, runtime.platform());
    runtime.disconnect();
    return selection.received && selection.success ? 0 : 1;
}

int run_quotes(smoke::IntradeBarSmokeConfig config, const CliOptions& options) {
    if (!smoke::require_live_config(config, std::cerr)) return 2;
    const std::string requested_symbol = smoke::option_value_or(
        options.values,
        "symbol",
        config.quote_symbol);
    const std::string symbol = smoke::normalize_quote_symbol(requested_symbol);

    smoke::IntradeBarSmokeRuntime runtime(std::move(config));
    if (!smoke::connect_or_report(runtime, std::cout, std::cerr)) return 1;
    if (!runtime.wait_for_price_update(symbol)) {
        std::cerr << "timed out waiting for price update: " << symbol << '\n';
        runtime.disconnect();
        return 1;
    }

    const auto ticks = runtime.latest_ticks();
    for (const auto& tick : ticks) {
        if (tick.symbol != symbol) continue;
        std::cout << tick.symbol;
        if (requested_symbol != symbol) {
            std::cout << " requested=" << requested_symbol;
        }
        std::cout
                  << " bid=" << std::setprecision(12) << tick.tick.bid
                  << " ask=" << std::setprecision(12) << tick.tick.ask
                  << " mid=" << std::setprecision(12) << tick.mid_price()
                  << '\n';
    }
    runtime.disconnect();
    return 0;
}

int run_switch_check(smoke::IntradeBarSmokeConfig config, const CliOptions& options) {
    if (!smoke::require_live_config(config, std::cerr)) return 2;
    if (!options.confirm) {
        std::cerr << "switch-check temporarily changes broker account settings; pass --confirm.\n";
        return 2;
    }

    const auto target_account_type = smoke::parse_enum_or<optionx::AccountType>(
        smoke::option_value_or(
            options.values,
            "account-type",
            optionx::to_str(config.account_type)),
        config.account_type);
    const auto target_currency = smoke::parse_enum_or<optionx::CurrencyType>(
        smoke::option_value_or(
            options.values,
            "currency",
            optionx::to_str(config.currency)),
        config.currency);
    const auto default_source_account_type =
        smoke::opposite_account_type(target_account_type);
    const auto default_source_currency =
        smoke::opposite_currency(target_currency);
    const auto source_account_type = smoke::parse_enum_or<optionx::AccountType>(
        smoke::option_value_or(
            options.values,
            "from-account-type",
            optionx::to_str(default_source_account_type)),
        default_source_account_type);
    const auto source_currency = smoke::parse_enum_or<optionx::CurrencyType>(
        smoke::option_value_or(
            options.values,
            "from-currency",
            optionx::to_str(default_source_currency)),
        default_source_currency);
    const int64_t timeout_ms = smoke::option_i64_or(
        options.values,
        "timeout-ms",
        config.settings_switch_timeout_ms);

    if (source_account_type == target_account_type && source_currency == target_currency) {
        std::cerr << "switch-check source settings must differ from target settings.\n";
        return 2;
    }
    if (source_currency == target_currency) {
        std::cerr << "switch-check source currency must differ from target currency "
                  << "so the connected balance poll can detect the mismatch.\n";
        return 2;
    }

    smoke::IntradeBarSmokeConfig target_config = config;
    target_config.account_type = target_account_type;
    target_config.currency = target_currency;

    smoke::IntradeBarSmokeConfig source_config = config;
    source_config.account_type = source_account_type;
    source_config.currency = source_currency;

    std::cout << "target account_type=" << optionx::to_str(target_account_type)
              << " currency=" << optionx::to_str(target_currency)
              << " balance_check_period_ms=" << target_config.balance_check_period_ms
              << " timeout_ms=" << timeout_ms << '\n';

    smoke::IntradeBarSmokeRuntime target_runtime(target_config);
    if (!smoke::connect_or_report(target_runtime, std::cout, std::cerr)) return 1;
    const bool initial_target_ok = target_runtime.has_account_settings(
        target_account_type,
        target_currency);
    std::cout << "initial_target_ok=" << initial_target_ok << ' ';
    smoke::print_account(std::cout, target_runtime.platform());
    if (!initial_target_ok) {
        target_runtime.disconnect();
        return 1;
    }

    {
        std::cout << "source account_type=" << optionx::to_str(source_account_type)
                  << " currency=" << optionx::to_str(source_currency) << '\n';
        smoke::IntradeBarSmokeRuntime source_runtime(source_config);
        if (!smoke::connect_or_report(source_runtime, std::cout, std::cerr)) {
            target_runtime.disconnect();
            restore_settings_direct(target_config, std::cout, std::cerr);
            return 1;
        }
        const bool source_ok = source_runtime.has_account_settings(
            source_account_type,
            source_currency);
        std::cout << "source_ok=" << source_ok << ' ';
        smoke::print_account(std::cout, source_runtime.platform());
        source_runtime.disconnect();
        if (!source_ok) {
            target_runtime.disconnect();
            restore_settings_direct(target_config, std::cout, std::cerr);
            return 1;
        }
    }

    const std::size_t update_count = target_runtime.account_update_count();
    const auto started_at = std::chrono::steady_clock::now();
    const bool recovered = target_runtime.wait_for_account_settings_after(
        target_account_type,
        target_currency,
        update_count,
        timeout_ms);
    const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started_at).count();

    std::cout << "recovered=" << recovered
              << " elapsed_ms=" << elapsed_ms
              << " account_updates=" << target_runtime.account_update_count()
              << ' ';
    smoke::print_account(std::cout, target_runtime.platform());

    target_runtime.disconnect();
    if (!recovered) {
        restore_settings_direct(target_config, std::cout, std::cerr);
    }
    return recovered ? 0 : 1;
}

int run_open_trade(smoke::IntradeBarSmokeConfig config, const CliOptions& options) {
    if (!smoke::require_live_config(config, std::cerr)) return 2;
    const bool confirmed = options.confirm || config.allow_trade;
    if (!confirmed) {
        std::cerr << "open-trade requires --confirm or OPTIONX_INTRADE_BAR_ALLOW_TRADE=1.\n";
        return 2;
    }
    if (config.account_type != optionx::AccountType::DEMO &&
        !(options.allow_real && config.allow_real_trade)) {
        std::cerr << "Refusing real-account trade. Use DEMO or set both --allow-real and OPTIONX_INTRADE_BAR_ALLOW_REAL_TRADE=1.\n";
        return 2;
    }

    const std::string symbol = smoke::option_value_or(
        options.values,
        "symbol",
        config.trade_symbol);
    const double amount = smoke::option_double_or(
        options.values,
        "amount",
        config.trade_amount);
    const int64_t duration = smoke::option_i64_or(
        options.values,
        "duration",
        config.trade_duration_sec);
    const auto order_type = smoke::parse_enum_or<optionx::OrderType>(
        smoke::option_value_or(
            options.values,
            "order",
            optionx::to_str(config.trade_order_type)),
        config.trade_order_type);

    smoke::IntradeBarSmokeRuntime runtime(config);
    if (!smoke::connect_or_report(runtime, std::cout, std::cerr)) return 1;
    smoke::print_account(std::cout, runtime.platform());

    const auto open = runtime.open_trade_and_wait(
        symbol,
        amount,
        order_type,
        duration,
        config.trade_open_timeout_ms);
    std::cout << "trade accepted=" << open.accepted
              << " callback=" << open.callback_received
              << " state=" << optionx::to_str(open.state)
              << " option_id=" << open.option_id
              << " open_price=" << open.open_price
              << " elapsed_ms=" << open.elapsed_ms
              << " error=" << open.error_desc
              << '\n';

    runtime.disconnect();
    return open.accepted && open.callback_received &&
        open.state == optionx::TradeState::OPEN_SUCCESS ? 0 : 1;
}

} // namespace

int main(int argc, char** argv) {
    LOGIT_ADD_MEMORY_LOGGER_DEFAULT_SINGLE_MODE();
    if (smoke::parse_bool(smoke::getenv_or_empty("OPTIONX_INTRADE_BAR_CLI_CONSOLE_LOG"), false)) {
        LOGIT_ADD_CONSOLE_DEFAULT();
    }
    LOGIT_ADD_FILE_LOGGER_DEFAULT();
    LOGIT_ADD_UNIQUE_FILE_LOGGER_DEFAULT_SINGLE_MODE();
    kurlyk::init();

    const CliOptions options = parse_args(argc, argv);
    auto config = smoke::load_config();
    int result = 0;

    if (options.command == "help" || options.command == "--help" || options.command == "-h") {
        print_usage(std::cout);
    } else if (options.command == "auth") {
        result = run_auth(std::move(config));
    } else if (options.command == "auth-cache") {
        result = run_auth_cache(std::move(config));
    } else if (options.command == "show-account") {
        result = run_auth(std::move(config));
    } else if (options.command == "domain-check") {
        result = run_domain_check(std::move(config), options);
    } else if (options.command == "quotes") {
        result = run_quotes(std::move(config), options);
    } else if (options.command == "switch-check") {
        result = run_switch_check(std::move(config), options);
    } else if (options.command == "open-trade") {
        result = run_open_trade(std::move(config), options);
    } else {
        std::cerr << "Unknown command: " << options.command << "\n\n";
        print_usage(std::cerr);
        result = 2;
    }

    kurlyk::deinit();
    LOGIT_WAIT();
    return result;
}
