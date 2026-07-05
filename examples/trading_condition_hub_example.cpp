#include <optionx_cpp/components.hpp>

#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

optionx::TradingConditionUpdate make_scope(std::string symbol) {
    optionx::TradingConditionUpdate scope;
    scope.symbol = std::move(symbol);
    scope.platform_type = optionx::PlatformType::INTRADE_BAR;
    scope.account_type = optionx::AccountType::DEMO;
    scope.currency = optionx::CurrencyType::USD;
    scope.option_type = optionx::OptionType::SPRINT;
    return scope;
}

optionx::TradingConditionUpdate make_initial_condition(
        std::string symbol,
        double payout,
        std::uint32_t min_duration,
        std::uint32_t max_duration) {
    auto update = make_scope(std::move(symbol));
    update.market_open = true;
    update.tradable = true;
    update.payout = payout;
    update.min_duration = min_duration;
    update.max_duration = max_duration;
    return update;
}

void print_optional_bool(const char* name, const std::optional<bool>& value) {
    if (value) {
        std::cout << ' ' << name << '=' << (*value ? "true" : "false");
    }
}

template<class T>
void print_optional_value(const char* name, const std::optional<T>& value) {
    if (value) {
        std::cout << ' ' << name << '=' << *value;
    }
}

void print_condition(const char* prefix, const optionx::TradingConditionUpdate& update) {
    std::cout << prefix
              << " symbol=" << update.symbol
              << " option=" << optionx::to_str(update.option_type);
    print_optional_bool("market_open", update.market_open);
    print_optional_bool("tradable", update.tradable);
    print_optional_value("payout", update.payout);
    print_optional_value("min_duration", update.min_duration);
    print_optional_value("max_duration", update.max_duration);
    print_optional_value("max_open_trades", update.max_open_trades);
    if (!update.message.empty()) {
        std::cout << " message=\"" << update.message << '"';
    }
    std::cout << '\n';
}

class ConditionRecorder final
        : public optionx::components::ITradingConditionSubscriber {
public:
    explicit ConditionRecorder(std::string name)
        : m_name(std::move(name)) {}

    void on_trading_condition(
            const optionx::TradingConditionUpdate& update) override {
        updates.push_back(update);
        const std::string prefix = "[" + m_name + "] update";
        print_condition(prefix.c_str(), update);
    }

    std::vector<optionx::TradingConditionUpdate> updates;

private:
    std::string m_name;
};

} // namespace

int main() {
    optionx::components::TradingConditionHub hub;

    // The hub stores weak references. Keep the subscriber alive while it should
    // receive condition changes such as payout, session and expiration updates.
    auto recorder = std::make_shared<ConditionRecorder>("bot");
    hub.add_subscriber(recorder);

    hub.publish(make_initial_condition("EUR/USD", 0.82, 60, 180));
    hub.publish(make_initial_condition("BTCUSDT", 0.70, 60, 300));

    // Broker updates can be partial. This event says that EUR/USD is closed and
    // its max expiration changed; it intentionally does not repeat the payout.
    auto eur_session_update = make_scope("EUR/USD");
    eur_session_update.market_open = false;
    eur_session_update.tradable = false;
    eur_session_update.max_duration = 300u;
    eur_session_update.message = "evening limits";
    hub.publish(eur_session_update);

    // Direct current-state query: the hub merged independent partial updates
    // into one snapshot for this exact broker/symbol/account/option scope.
    if (const auto state = hub.current_condition(make_scope("EUR/USD"))) {
        print_condition("[query] current", *state);
    }

    // Late subscribers receive merged current snapshots, not the whole old log.
    auto late_recorder = std::make_shared<ConditionRecorder>("late");
    hub.add_subscriber(late_recorder);

    return 0;
}
