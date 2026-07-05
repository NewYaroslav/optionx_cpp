#include <gtest/gtest.h>

#include <optionx_cpp/components.hpp>

#include <memory>
#include <string>
#include <vector>

namespace {

class TestAccountInfo final : public optionx::BaseAccountInfoData {
public:
    double balance = 0.0;

    std::unique_ptr<optionx::BaseAccountInfoData> clone_unique() const override {
        return std::make_unique<TestAccountInfo>(*this);
    }

    std::shared_ptr<optionx::BaseAccountInfoData> clone_shared() const override {
        return std::make_shared<TestAccountInfo>(*this);
    }

protected:
    bool get_info_bool(const optionx::AccountInfoRequest& request) const override {
        (void)request;
        return false;
    }

    std::int64_t get_info_int64(const optionx::AccountInfoRequest& request) const override {
        if (request.type == optionx::AccountInfoType::BALANCE) {
            return static_cast<std::int64_t>(balance);
        }
        return 0;
    }

    double get_info_f64(const optionx::AccountInfoRequest& request) const override {
        if (request.type == optionx::AccountInfoType::BALANCE) {
            return balance;
        }
        return 0.0;
    }

    std::string get_info_str(const optionx::AccountInfoRequest& request) const override {
        (void)request;
        return {};
    }

    optionx::AccountType get_info_account_type(
            const optionx::AccountInfoRequest& request) const override {
        (void)request;
        return optionx::AccountType::UNKNOWN;
    }

    optionx::CurrencyType get_info_currency(
            const optionx::AccountInfoRequest& request) const override {
        (void)request;
        return optionx::CurrencyType::UNKNOWN;
    }
};

class RecordingAccountSubscriber final
        : public optionx::components::IAccountInfoSubscriber {
public:
    void on_account_info(const optionx::AccountInfoUpdate& update) override {
        statuses.push_back(update.status);
        messages.push_back(update.message);
        if (update.account_info) {
            balances.push_back(update.account_info->get_info<double>(
                optionx::AccountInfoType::BALANCE));
        }
    }

    std::vector<optionx::AccountUpdateStatus> statuses;
    std::vector<std::string> messages;
    std::vector<double> balances;
};

optionx::AccountInfoUpdate make_update(
        optionx::AccountUpdateStatus status,
        std::string message = {}) {
    return optionx::AccountInfoUpdate(nullptr, status, std::move(message));
}

} // namespace

TEST(AccountInfoHub, RoutesUpdatesAndReplaysLastUpdate) {
    optionx::components::AccountInfoHub hub;

    const auto first = std::make_shared<RecordingAccountSubscriber>();
    hub.add_subscriber(first);

    hub.publish(make_update(optionx::AccountUpdateStatus::CONNECTING, "start"));

    ASSERT_EQ(first->statuses.size(), 1u);
    EXPECT_EQ(first->statuses[0], optionx::AccountUpdateStatus::CONNECTING);
    EXPECT_EQ(first->messages[0], "start");

    const auto late = std::make_shared<RecordingAccountSubscriber>();
    hub.add_subscriber(late);

    ASSERT_EQ(late->statuses.size(), 1u);
    EXPECT_EQ(late->statuses[0], optionx::AccountUpdateStatus::CONNECTING);
    EXPECT_EQ(late->messages[0], "start");

    hub.publish(make_update(optionx::AccountUpdateStatus::CONNECTED, "ready"));

    ASSERT_EQ(first->statuses.size(), 2u);
    EXPECT_EQ(first->statuses[1], optionx::AccountUpdateStatus::CONNECTED);

    ASSERT_EQ(late->statuses.size(), 2u);
    EXPECT_EQ(late->statuses[1], optionx::AccountUpdateStatus::CONNECTED);
}

TEST(AccountInfoHub, RoutesStatusAndCurrentAccountSnapshot) {
    optionx::components::AccountInfoHub hub;

    auto account = std::make_shared<TestAccountInfo>();
    account->balance = 1250.75;

    const auto subscriber = std::make_shared<RecordingAccountSubscriber>();
    hub.add_subscriber(subscriber);

    hub.publish(optionx::AccountInfoUpdate(
        account,
        optionx::AccountUpdateStatus::BALANCE_UPDATED));

    ASSERT_EQ(subscriber->statuses.size(), 1u);
    EXPECT_EQ(subscriber->statuses[0], optionx::AccountUpdateStatus::BALANCE_UPDATED);
    ASSERT_EQ(subscriber->balances.size(), 1u);
    EXPECT_DOUBLE_EQ(subscriber->balances[0], 1250.75);
}

TEST(AccountInfoHub, RemovesSubscribersAndIgnoresDuplicateAdds) {
    optionx::components::AccountInfoHub hub(false);

    const auto first = std::make_shared<RecordingAccountSubscriber>();
    const auto second = std::make_shared<RecordingAccountSubscriber>();

    hub.add_subscriber(first);
    hub.add_subscriber(first);
    hub.add_subscriber(second);

    EXPECT_EQ(hub.subscriber_count(), 2u);

    hub.remove_subscriber(first.get());
    hub.publish(make_update(optionx::AccountUpdateStatus::BALANCE_UPDATED));

    EXPECT_TRUE(first->statuses.empty());

    ASSERT_EQ(second->statuses.size(), 1u);
    EXPECT_EQ(second->statuses[0], optionx::AccountUpdateStatus::BALANCE_UPDATED);
}

TEST(AccountInfoHub, BindsToAccountInfoCallback) {
    optionx::components::AccountInfoHub hub;
    optionx::account_info_callback_t callback;

    const auto subscriber = std::make_shared<RecordingAccountSubscriber>();
    hub.add_subscriber(subscriber);

    hub.bind_to(callback);
    ASSERT_TRUE(static_cast<bool>(callback));

    callback(make_update(optionx::AccountUpdateStatus::DISCONNECTED, "stop"));

    ASSERT_EQ(subscriber->statuses.size(), 1u);
    EXPECT_EQ(subscriber->statuses[0], optionx::AccountUpdateStatus::DISCONNECTED);
    EXPECT_EQ(subscriber->messages[0], "stop");

    const auto last_update = hub.last_update();
    ASSERT_TRUE(last_update.has_value());
    EXPECT_EQ(last_update->status, optionx::AccountUpdateStatus::DISCONNECTED);

    hub.unbind_from(callback);
    EXPECT_FALSE(static_cast<bool>(callback));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
