#define OPTIONX_LOG_UNIQUE_FILE_INDEX 2

#include "IntradeBarSmokeSupport.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <map>
#include <vector>

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
        << "  intrade_bar_smoke_cli domain-check [--domain-min=-1] [--domain-max=1000]\n"
        << "  intrade_bar_smoke_cli quotes [--symbol=EURUSD]\n"
        << "  intrade_bar_smoke_cli history [--source=CSV|HTML|HTML_CSV] [--days=14|--all] [--time-field=CLOSE_DATE] [--comment=...]\n"
        << "  intrade_bar_smoke_cli switch-check --confirm [--account-type=DEMO] [--currency=USD]\n"
        << "  intrade_bar_smoke_cli open-trade --confirm [--symbol=EURUSD] [--amount=1] [--duration=60] [--buy|--sell]\n"
        << "  intrade_bar_smoke_cli open-trades-sync-check --confirm [--symbol=BTCUSDT|EURUSD] [--amount=1] [--duration=300] [--count=1] [--interval-ms=15000] [--buy|--sell]\n"
        << "  intrade_bar_smoke_cli open-check-result --confirm [--symbol=BTCUSDT] [--amount=1] [--duration=300] [--buy|--sell]\n"
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
        } else if (arg == "--all") {
            options.values["all"] = "1";
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

std::string upper_ascii(std::string value) {
    value = optionx::utils::trim_copy(std::move(value));
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    return value;
}

const char* trade_history_time_field_to_string(optionx::TradeRecordTimeField value) noexcept {
    switch (value) {
    case optionx::TradeRecordTimeField::PLACE_DATE:
        return "PLACE_DATE";
    case optionx::TradeRecordTimeField::SEND_DATE:
        return "SEND_DATE";
    case optionx::TradeRecordTimeField::OPEN_DATE:
        return "OPEN_DATE";
    case optionx::TradeRecordTimeField::CLOSE_DATE:
        return "CLOSE_DATE";
    case optionx::TradeRecordTimeField::AUTO:
    default:
        return "AUTO";
    }
}

optionx::TradeRecordTimeField parse_trade_history_time_field(
        const std::string& value,
        optionx::TradeRecordTimeField fallback) {
    const std::string normalized = upper_ascii(value);
    if (normalized.empty()) return fallback;
    if (normalized == "AUTO") return optionx::TradeRecordTimeField::AUTO;
    if (normalized == "PLACE_DATE" || normalized == "PLACE") return optionx::TradeRecordTimeField::PLACE_DATE;
    if (normalized == "SEND_DATE" || normalized == "SEND") return optionx::TradeRecordTimeField::SEND_DATE;
    if (normalized == "OPEN_DATE" || normalized == "OPEN") return optionx::TradeRecordTimeField::OPEN_DATE;
    if (normalized == "CLOSE_DATE" || normalized == "CLOSE") return optionx::TradeRecordTimeField::CLOSE_DATE;
    return fallback;
}

const char* time_range_mode_to_string(optionx::TimeRangeMode value) noexcept {
    switch (value) {
    case optionx::TimeRangeMode::NONE:
        return "NONE";
    case optionx::TimeRangeMode::HALF_OPEN:
        return "HALF_OPEN";
    case optionx::TimeRangeMode::CLOSED:
    default:
        return "CLOSED";
    }
}

optionx::TimeRangeMode parse_time_range_mode(
        const std::string& value,
        optionx::TimeRangeMode fallback) {
    const std::string normalized = upper_ascii(value);
    if (normalized.empty()) return fallback;
    if (normalized == "NONE" || normalized == "ALL") return optionx::TimeRangeMode::NONE;
    if (normalized == "HALF_OPEN" || normalized == "HALF-OPEN") return optionx::TimeRangeMode::HALF_OPEN;
    if (normalized == "CLOSED") return optionx::TimeRangeMode::CLOSED;
    return fallback;
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
    config.domain_index_min = smoke::option_signed_int_or(
        options.values,
        "domain-min",
        config.domain_index_min);
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

bool is_btc_symbol(const std::string& symbol) {
    const std::string normalized = upper_ascii(symbol);
    return normalized == "BTCUSD" || normalized == "BTCUSDT";
}

int64_t default_open_trades_sync_duration_sec(const std::string& symbol) {
    return is_btc_symbol(symbol) ? 5 * time_shield::SEC_PER_MIN :
        3 * time_shield::SEC_PER_MIN;
}

int run_open_trades_sync_check(
        smoke::IntradeBarSmokeConfig config,
        const CliOptions& options) {
    if (!smoke::require_live_config(config, std::cerr)) return 2;
    const bool confirmed = options.confirm || config.allow_trade;
    if (!confirmed) {
        std::cerr << "open-trades-sync-check opens a broker trade; pass --confirm or OPTIONX_INTRADE_BAR_ALLOW_TRADE=1.\n";
        return 2;
    }
    if (config.account_type != optionx::AccountType::DEMO &&
        !(options.allow_real && config.allow_real_trade)) {
        std::cerr << "Refusing real-account trade. Use DEMO or set both --allow-real and OPTIONX_INTRADE_BAR_ALLOW_REAL_TRADE=1.\n";
        return 2;
    }

    config.active_trades_close_buffer_ms = smoke::option_i64_or(
        options.values,
        "close-buffer-ms",
        config.active_trades_close_buffer_ms);
    config.active_trades_sync_period_ms = smoke::option_i64_or(
        options.values,
        "sync-period-ms",
        config.active_trades_sync_period_ms);

    const std::string requested_symbol = smoke::option_value_or(
        options.values,
        "symbol",
        "BTCUSDT");
    const std::string symbol = smoke::normalize_quote_symbol(requested_symbol);
    const int64_t duration = smoke::option_i64_or(
        options.values,
        "duration",
        default_open_trades_sync_duration_sec(symbol));
    const double amount = smoke::option_double_or(
        options.values,
        "amount",
        config.trade_amount);
    const auto order_type = smoke::parse_enum_or<optionx::OrderType>(
        smoke::option_value_or(
            options.values,
            "order",
            optionx::to_str(config.trade_order_type)),
        config.trade_order_type);
    const int64_t snapshot_timeout_ms = smoke::option_i64_or(
        options.values,
        "snapshot-timeout-ms",
        std::max<int64_t>(30000, config.active_trades_sync_period_ms + 15000));
    const int64_t local_counter_timeout_ms = smoke::option_i64_or(
        options.values,
        "local-counter-timeout-ms",
        10000);
    const int64_t trade_count = std::max<int64_t>(
        1,
        smoke::option_i64_or(options.values, "count", 1));
    const int64_t open_interval_ms = std::max<int64_t>(
        0,
        smoke::option_i64_or(options.values, "interval-ms", 15000));
    const int64_t min_accepted = std::max<int64_t>(
        1,
        smoke::option_i64_or(options.values, "min-accepted", trade_count));

    std::cout << "open_trades_sync_check symbol=" << symbol;
    if (requested_symbol != symbol) {
        std::cout << " requested=" << requested_symbol;
    }
    std::cout << " amount=" << amount
              << " duration=" << duration
              << " order=" << optionx::to_str(order_type)
              << " count=" << trade_count
              << " interval_ms=" << open_interval_ms
              << " min_accepted=" << min_accepted
              << " close_buffer_ms=" << config.active_trades_close_buffer_ms
              << " sync_period_ms=" << config.active_trades_sync_period_ms
              << " snapshot_timeout_ms=" << snapshot_timeout_ms
              << '\n';

    smoke::IntradeBarSmokeRuntime open_runtime(config);
    const std::size_t first_initial_updates = open_runtime.open_trades_update_count();
    if (!smoke::connect_or_report(open_runtime, std::cout, std::cerr)) return 1;
    const bool first_snapshot = open_runtime.wait_for_open_trades_update_after(
        first_initial_updates,
        snapshot_timeout_ms);
    const int64_t initial_open_trades = open_runtime.current_open_trades();
    std::cout << "initial_snapshot=" << first_snapshot
              << " initial_open_trades=" << initial_open_trades
              << " open_trades_updates=" << open_runtime.open_trades_update_count()
              << ' ';
    smoke::print_account(std::cout, open_runtime.platform());
    if (!first_snapshot) {
        std::cerr << "Initial broker open-trades snapshot was not received.\n";
        open_runtime.disconnect();
        return 1;
    }

    std::vector<smoke::TradeOpenAttempt> opened_trades;
    opened_trades.reserve(static_cast<std::size_t>(trade_count));
    bool local_counter_ok = true;
    for (int64_t index = 0; index < trade_count; ++index) {
        const std::size_t before_open_updates = open_runtime.open_trades_update_count();
        const auto open = open_runtime.open_trade_and_wait(
            symbol,
            amount,
            order_type,
            duration,
            config.trade_open_timeout_ms);

        bool local_increment = false;
        if (open.accepted &&
            open.callback_received &&
            open.state == optionx::TradeState::OPEN_SUCCESS &&
            open.option_id > 0) {
            opened_trades.push_back(open);
            local_increment = open_runtime.wait_for_open_trades_at_least_after(
                initial_open_trades + static_cast<int64_t>(opened_trades.size()),
                before_open_updates,
                local_counter_timeout_ms);
            local_counter_ok = local_counter_ok && local_increment;
        }
        const int64_t local_open_trades = open_runtime.current_open_trades();

        std::cout << "open[" << (index + 1) << "/" << trade_count << "]"
                  << " accepted=" << open.accepted
                  << " callback=" << open.callback_received
                  << " state=" << optionx::to_str(open.state)
                  << " option_id=" << open.option_id
                  << " open_price=" << std::setprecision(12) << open.open_price
                  << " open_date=" << open.open_date
                  << " close_date=" << open.close_date
                  << " elapsed_ms=" << open.elapsed_ms
                  << " local_increment=" << local_increment
                  << " local_open_trades=" << local_open_trades
                  << " error=" << open.error_desc
                  << '\n';

        if (index + 1 < trade_count && open_interval_ms > 0) {
            open_runtime.pump_for(open_interval_ms);
        }
    }

    std::cout << "open_summary requested=" << trade_count
              << " accepted=" << opened_trades.size()
              << " local_counter_ok=" << local_counter_ok
              << " local_open_trades=" << open_runtime.current_open_trades()
              << '\n';

    open_runtime.disconnect();

    if (static_cast<int64_t>(opened_trades.size()) < min_accepted ||
        !local_counter_ok) {
        return 1;
    }

    smoke::IntradeBarSmokeRuntime snapshot_runtime(config);
    const std::size_t reconnect_updates = snapshot_runtime.open_trades_update_count();
    const auto reconnect = snapshot_runtime.connect(config.auth_timeout_ms);
    std::cout << "reconnect callback=" << reconnect.callback_received
              << " success=" << reconnect.success
              << " elapsed_ms=" << reconnect.elapsed_ms << '\n';
    if (!reconnect.success) {
        std::cerr << "reconnect auth failed: " << reconnect.reason << '\n';
        snapshot_runtime.disconnect();
        return 1;
    }

    const bool broker_snapshot = snapshot_runtime.wait_for_open_trades_at_least_after(
        static_cast<int64_t>(opened_trades.size()),
        reconnect_updates,
        snapshot_timeout_ms);
    const int64_t broker_snapshot_open_trades = snapshot_runtime.current_open_trades();
    std::cout << "broker_snapshot=" << broker_snapshot
              << " broker_open_trades=" << broker_snapshot_open_trades
              << " open_trades_updates=" << snapshot_runtime.open_trades_update_count()
              << ' ';
    smoke::print_account(std::cout, snapshot_runtime.platform());
    if (!broker_snapshot || broker_snapshot_open_trades <= 0) {
        snapshot_runtime.disconnect();
        return 1;
    }

    int64_t latest_close_date = 0;
    for (const auto& trade : opened_trades) {
        latest_close_date = std::max(latest_close_date, trade.close_date);
    }
    const int64_t now_ms = OPTIONX_TIMESTAMP_MS;
    const int64_t default_close_timeout_ms = std::max<int64_t>(
        30000,
        std::max<int64_t>(0, latest_close_date - now_ms) +
            config.active_trades_close_buffer_ms +
            2 * config.active_trades_sync_period_ms +
            60000);
    const int64_t close_timeout_ms = smoke::option_i64_or(
        options.values,
        "close-timeout-ms",
        default_close_timeout_ms);
    const int64_t target_open_trades = std::max<int64_t>(
        0,
        broker_snapshot_open_trades - static_cast<int64_t>(opened_trades.size()));
    std::size_t last_update_count = snapshot_runtime.open_trades_update_count();
    std::cout << "waiting_for_decrement close_timeout_ms=" << close_timeout_ms
              << " expected_close_date=" << latest_close_date
              << " target_open_trades=" << target_open_trades
              << '\n';

    const int64_t close_wait_started_ms = OPTIONX_TIMESTAMP_MS;
    const int64_t close_deadline_ms = close_wait_started_ms + close_timeout_ms;
    while (snapshot_runtime.current_open_trades() > target_open_trades &&
           OPTIONX_TIMESTAMP_MS < close_deadline_ms) {
        const int64_t remaining_ms = close_deadline_ms - OPTIONX_TIMESTAMP_MS;
        const bool update = snapshot_runtime.wait_for_open_trades_update_after(
            last_update_count,
            std::max<int64_t>(1, remaining_ms));
        const auto updates = snapshot_runtime.open_trades_updates();
        if (updates.size() > last_update_count) {
            for (std::size_t i = last_update_count; i < updates.size(); ++i) {
                std::cout << "broker_counter_update index=" << (i + 1)
                          << " open_trades=" << updates[i]
                          << " elapsed_ms=" << (OPTIONX_TIMESTAMP_MS - close_wait_started_ms)
                          << '\n';
            }
            last_update_count = updates.size();
        }
        if (!update) break;
    }

    const int64_t final_open_trades = snapshot_runtime.current_open_trades();
    const bool broker_decrement = final_open_trades <= target_open_trades;
    std::cout << "broker_decrement=" << broker_decrement
              << " final_open_trades=" << final_open_trades
              << " open_trades_updates=" << snapshot_runtime.open_trades_update_count()
              << ' ';
    smoke::print_account(std::cout, snapshot_runtime.platform());

    snapshot_runtime.disconnect();
    return broker_decrement ? 0 : 1;
}

void print_trade_result_line(
        std::ostream& out,
        const std::string& prefix,
        const optionx::TradeResult& result) {
    out << prefix
        << " state=" << optionx::to_str(result.trade_state)
        << " option_id=" << result.option_id
        << " amount=" << std::setprecision(12) << result.amount
        << " currency=" << optionx::to_str(result.currency)
        << " open_price=" << std::setprecision(12) << result.open_price
        << " close_price=" << std::setprecision(12) << result.close_price
        << " profit=" << std::setprecision(12) << result.profit
        << " payout=" << std::setprecision(12) << result.payout
        << " balance=" << std::setprecision(12) << result.balance
        << " open_date=" << result.open_date
        << " close_date=" << result.close_date
        << " error=" << result.error_desc
        << '\n';
}

void print_trade_record_line(
        std::ostream& out,
        const std::string& prefix,
        const optionx::TradeRecord& record) {
    out << prefix
        << " state=" << optionx::to_str(record.trade_state)
        << " option_id=" << record.option_id
        << " symbol=" << record.symbol
        << " option_type=" << optionx::to_str(record.option_type)
        << " order=" << optionx::to_str(record.order_type)
        << " amount=" << std::setprecision(12) << record.amount
        << " currency=" << optionx::to_str(record.currency)
        << " open_price=" << std::setprecision(12) << record.open_price
        << " close_price=" << std::setprecision(12) << record.close_price
        << " profit=" << std::setprecision(12) << record.profit
        << " payout=" << std::setprecision(12) << record.payout
        << " balance=" << std::setprecision(12) << record.balance
        << " open_date=" << record.open_date
        << " close_date=" << record.close_date
        << " comment=" << record.comment
        << " error=" << record.error_desc
        << '\n';
}

int run_history(smoke::IntradeBarSmokeConfig config, const CliOptions& options) {
    if (!smoke::require_live_config(config, std::cerr)) return 2;

    config.trade_history_source =
        optionx::platforms::intrade_bar::trade_history_source_from_string(
            smoke::option_value_or(
                options.values,
                "source",
                optionx::platforms::intrade_bar::trade_history_source_to_string(
                    config.trade_history_source)),
            config.trade_history_source);
    config.account_type = smoke::parse_enum_or<optionx::AccountType>(
        smoke::option_value_or(
            options.values,
            "account-type",
            optionx::to_str(config.account_type)),
        config.account_type);

    const int64_t now_ms = OPTIONX_TIMESTAMP_MS;
    const int64_t days = smoke::option_i64_or(options.values, "days", 14);
    const bool fetch_all = options.values.find("all") != options.values.end();
    const int64_t from_ms = smoke::option_i64_or(
        options.values,
        "from-ms",
        now_ms - days * time_shield::MS_PER_DAY);
    const int64_t to_ms = smoke::option_i64_or(options.values, "to-ms", now_ms);
    const int64_t timeout_ms = smoke::option_i64_or(
        options.values,
        "timeout-ms",
        config.auth_timeout_ms);

    optionx::TradeHistoryRequest request = fetch_all ?
        optionx::TradeHistoryRequest::all() :
        optionx::TradeHistoryRequest{};
    request.time_field = parse_trade_history_time_field(
        smoke::option_value_or(
            options.values,
            "time-field",
            trade_history_time_field_to_string(request.time_field)),
        request.time_field);
    request.comment = smoke::option_value_or(
        options.values,
        "comment",
        request.comment);
    if (!fetch_all) {
        request.start_ms = from_ms;
        request.stop_ms = to_ms;
        request.range_mode = parse_time_range_mode(
            smoke::option_value_or(
                options.values,
                "range-mode",
                time_range_mode_to_string(request.range_mode)),
            request.range_mode);
    }

    std::cout << "history source="
              << optionx::platforms::intrade_bar::trade_history_source_to_string(
                     config.trade_history_source)
              << " account_type=" << optionx::to_str(config.account_type)
              << " range_mode=" << time_range_mode_to_string(request.range_mode)
              << " time_field=" << trade_history_time_field_to_string(request.time_field)
              << " from_ms=" << request.start_ms
              << " to_ms=" << request.stop_ms
              << " comment=" << request.comment
              << " timeout_ms=" << timeout_ms
              << '\n';

    smoke::IntradeBarSmokeRuntime runtime(config);
    if (!smoke::connect_or_report(runtime, std::cout, std::cerr)) return 1;
    smoke::print_account(std::cout, runtime.platform());

    const auto history = runtime.fetch_trade_history_and_wait(request, timeout_ms);
    std::cout << "history accepted=" << history.accepted
              << " callback=" << history.callback_received
              << " success=" << history.success
              << " status_code=" << history.status_code
              << " records=" << history.records.size()
              << " elapsed_ms=" << history.elapsed_ms
              << " error=" << history.error_desc
              << '\n';
    for (const auto& record : history.records) {
        print_trade_record_line(std::cout, "history_record", record);
    }

    runtime.disconnect();
    return history.accepted && history.callback_received && history.success ? 0 : 1;
}

int run_open_check_result(smoke::IntradeBarSmokeConfig config, const CliOptions& options) {
    if (!smoke::require_live_config(config, std::cerr)) return 2;
    const bool confirmed = options.confirm || config.allow_trade;
    if (!confirmed) {
        std::cerr << "open-check-result opens a broker trade; pass --confirm or OPTIONX_INTRADE_BAR_ALLOW_TRADE=1.\n";
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
        "BTCUSDT");
    const double amount = smoke::option_double_or(
        options.values,
        "amount",
        1.0);
    const int64_t duration = smoke::option_i64_or(
        options.values,
        "duration",
        5 * time_shield::SEC_PER_MIN);
    const int64_t result_timeout_ms = smoke::option_i64_or(
        options.values,
        "result-timeout-ms",
        config.trade_result_timeout_ms);
    const int retry_attempts = static_cast<int>(smoke::option_i64_or(
        options.values,
        "retry-attempts",
        15));
    const auto order_type = smoke::parse_enum_or<optionx::OrderType>(
        smoke::option_value_or(
            options.values,
            "order",
            optionx::to_str(config.trade_order_type)),
        config.trade_order_type);

    smoke::IntradeBarSmokeRuntime runtime(config);
    if (!smoke::connect_or_report(runtime, std::cout, std::cerr)) return 1;
    smoke::print_account(std::cout, runtime.platform());

    std::cout << "open_check_result symbol=" << symbol
              << " amount=" << amount
              << " duration=" << duration
              << " order=" << optionx::to_str(order_type)
              << " result_timeout_ms=" << result_timeout_ms
              << " retry_attempts=" << retry_attempts
              << '\n';

    const auto lifecycle = runtime.open_trade_and_wait_for_result(
        symbol,
        amount,
        order_type,
        duration,
        result_timeout_ms);

    std::cout << "lifecycle accepted=" << lifecycle.accepted
              << " callback=" << lifecycle.callback_received
              << " open_received=" << lifecycle.open_received
              << " terminal_received=" << lifecycle.terminal_received
              << " elapsed_ms=" << lifecycle.elapsed_ms
              << '\n';
    print_trade_result_line(std::cout, "lifecycle_result", lifecycle.result);

    if (!lifecycle.accepted ||
        !lifecycle.callback_received ||
        !lifecycle.open_received ||
        !lifecycle.terminal_received ||
        lifecycle.result.option_id <= 0) {
        runtime.disconnect();
        return 1;
    }

    optionx::TradeResultQuery query;
    query.trade_id = lifecycle.result.trade_id;
    query.option_id = lifecycle.result.option_id;
    query.retry_attempts = retry_attempts;

    auto restored_result = std::make_unique<optionx::TradeResult>();
    restored_result->trade_id = lifecycle.result.trade_id;
    restored_result->option_id = lifecycle.result.option_id;
    restored_result->amount = lifecycle.result.amount;
    restored_result->payout = lifecycle.result.payout;
    restored_result->account_type = lifecycle.result.account_type;
    restored_result->currency = lifecycle.result.currency;
    restored_result->platform_type = lifecycle.result.platform_type;
    restored_result->open_price = lifecycle.result.open_price;
    restored_result->open_date = lifecycle.result.open_date;
    restored_result->close_date = lifecycle.result.close_date;

    const auto fetch = runtime.fetch_trade_result_and_wait(
        std::move(query),
        std::move(restored_result),
        config.trade_open_timeout_ms);

    std::cout << "fetch accepted=" << fetch.accepted
              << " callback=" << fetch.callback_received
              << " elapsed_ms=" << fetch.elapsed_ms
              << '\n';
    print_trade_result_line(std::cout, "fetched_result", fetch.result);

    const bool state_match = fetch.result.trade_state == lifecycle.result.trade_state;
    const bool profit_match = std::abs(fetch.result.profit - lifecycle.result.profit) < 0.01;
    std::cout << "compare state_match=" << state_match
              << " profit_match=" << profit_match
              << '\n';

    runtime.disconnect();
    return fetch.accepted &&
        fetch.callback_received &&
        optionx::is_terminal_trade_state(fetch.result.trade_state) &&
        fetch.result.trade_state != optionx::TradeState::CHECK_ERROR &&
        state_match &&
        profit_match ? 0 : 1;
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
    } else if (options.command == "history") {
        result = run_history(std::move(config), options);
    } else if (options.command == "switch-check") {
        result = run_switch_check(std::move(config), options);
    } else if (options.command == "open-trade") {
        result = run_open_trade(std::move(config), options);
    } else if (options.command == "open-trades-sync-check") {
        result = run_open_trades_sync_check(std::move(config), options);
    } else if (options.command == "open-check-result") {
        result = run_open_check_result(std::move(config), options);
    } else {
        std::cerr << "Unknown command: " << options.command << "\n\n";
        print_usage(std::cerr);
        result = 2;
    }

    kurlyk::deinit();
    LOGIT_WAIT();
    return result;
}
