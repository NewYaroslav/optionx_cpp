#include <optionx_cpp/components.hpp>

#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

namespace {

class DemoAccountInfo final : public optionx::BaseAccountInfoData {
public:
    double balance = 0.0;
    optionx::CurrencyType currency = optionx::CurrencyType::USD;
    optionx::AccountType account_type = optionx::AccountType::DEMO;
    bool connected = false;
    std::int64_t open_trades = 0;

    std::unique_ptr<optionx::BaseAccountInfoData> clone_unique() const override {
        return std::make_unique<DemoAccountInfo>(*this);
    }

    std::shared_ptr<optionx::BaseAccountInfoData> clone_shared() const override {
        return std::make_shared<DemoAccountInfo>(*this);
    }

protected:
    bool get_info_bool(const optionx::AccountInfoRequest& request) const override {
        if (request.type == optionx::AccountInfoType::CONNECTION_STATUS) {
            return connected;
        }
        return false;
    }

    std::int64_t get_info_int64(const optionx::AccountInfoRequest& request) const override {
        switch (request.type) {
        case optionx::AccountInfoType::CONNECTION_STATUS:
            return connected ? 1 : 0;
        case optionx::AccountInfoType::OPEN_TRADES:
            return open_trades;
        case optionx::AccountInfoType::ACCOUNT_TYPE:
            return static_cast<std::int64_t>(account_type);
        case optionx::AccountInfoType::CURRENCY:
            return static_cast<std::int64_t>(currency);
        case optionx::AccountInfoType::BALANCE:
            return static_cast<std::int64_t>(balance);
        default:
            return 0;
        }
    }

    double get_info_f64(const optionx::AccountInfoRequest& request) const override {
        if (request.type == optionx::AccountInfoType::BALANCE) {
            return balance;
        }
        return 0.0;
    }

    std::string get_info_str(const optionx::AccountInfoRequest& request) const override {
        switch (request.type) {
        case optionx::AccountInfoType::BALANCE:
            return std::to_string(balance);
        case optionx::AccountInfoType::ACCOUNT_TYPE:
            return optionx::to_str(account_type);
        case optionx::AccountInfoType::CURRENCY:
            return optionx::to_str(currency);
        default:
            return {};
        }
    }

    optionx::AccountType get_info_account_type(
            const optionx::AccountInfoRequest& request) const override {
        (void)request;
        return account_type;
    }

    optionx::CurrencyType get_info_currency(
            const optionx::AccountInfoRequest& request) const override {
        (void)request;
        return currency;
    }
};

class ConsoleAccountSubscriber final : public optionx::components::IAccountInfoSubscriber {
public:
    void on_account_info(const optionx::AccountInfoUpdate& update) override {
        std::cout << "[account] " << optionx::to_str(update.status);
        if (!update.message.empty()) {
            std::cout << " message=\"" << update.message << "\"";
        }

        // AccountUpdateStatus identifies what changed; account_info carries the
        // current snapshot where the new value can be queried.
        if (update.account_info) {
            switch (update.status) {
            case optionx::AccountUpdateStatus::BALANCE_UPDATED:
                std::cout << " balance="
                          << update.account_info->get_info<double>(
                                 optionx::AccountInfoType::BALANCE);
                break;
            case optionx::AccountUpdateStatus::CURRENCY_CHANGED:
                std::cout << " currency="
                          << optionx::to_str(update.account_info->get_info<optionx::CurrencyType>(
                                 optionx::AccountInfoType::CURRENCY));
                break;
            case optionx::AccountUpdateStatus::ACCOUNT_TYPE_CHANGED:
                std::cout << " account_type="
                          << optionx::to_str(update.account_info->get_info<optionx::AccountType>(
                                 optionx::AccountInfoType::ACCOUNT_TYPE));
                break;
            case optionx::AccountUpdateStatus::OPEN_TRADES_CHANGED:
                std::cout << " open_trades="
                          << update.account_info->get_info<std::int64_t>(
                                 optionx::AccountInfoType::OPEN_TRADES);
                break;
            case optionx::AccountUpdateStatus::CONNECTED:
            case optionx::AccountUpdateStatus::DISCONNECTED:
                std::cout << " connected="
                          << update.account_info->get_info<bool>(
                                 optionx::AccountInfoType::CONNECTION_STATUS);
                break;
            default:
                break;
            }
        }

        std::cout << '\n';
    }
};

optionx::AccountInfoUpdate make_update(
        std::shared_ptr<DemoAccountInfo> account,
        optionx::AccountUpdateStatus status,
        std::string message = {}) {
    return optionx::AccountInfoUpdate(std::move(account), status, std::move(message));
}

} // namespace

int main() {
    optionx::components::AccountInfoHub hub;

    // The hub stores weak references. Keep this shared_ptr alive while the
    // subscriber should receive account events.
    auto subscriber = std::make_shared<ConsoleAccountSubscriber>();
    hub.add_subscriber(subscriber);

    auto account = std::make_shared<DemoAccountInfo>();
    account->connected = true;
    hub.publish(make_update(account, optionx::AccountUpdateStatus::CONNECTED));

    account->balance = 1250.75;
    hub.publish(make_update(account, optionx::AccountUpdateStatus::BALANCE_UPDATED));

    account->open_trades = 2;
    hub.publish(make_update(account, optionx::AccountUpdateStatus::OPEN_TRADES_CHANGED));

    // Late subscribers receive the latest cached update immediately.
    auto late_subscriber = std::make_shared<ConsoleAccountSubscriber>();
    hub.add_subscriber(late_subscriber);

    return 0;
}
