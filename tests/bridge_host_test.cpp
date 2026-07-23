#include <gtest/gtest.h>

#include <optionx_cpp/bridges.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

class TestAccountInfo final : public optionx::BaseAccountInfoData {
public:
    double balance = 100.0;

    std::unique_ptr<optionx::BaseAccountInfoData> clone_unique() const override {
        return std::make_unique<TestAccountInfo>(*this);
    }

    std::shared_ptr<optionx::BaseAccountInfoData> clone_shared() const override {
        return std::make_shared<TestAccountInfo>(*this);
    }

protected:
    bool get_info_bool(const optionx::AccountInfoRequest&) const override {
        return true;
    }

    int64_t get_info_int64(const optionx::AccountInfoRequest&) const override {
        return 0;
    }

    double get_info_f64(const optionx::AccountInfoRequest& request) const override {
        if (request.type == optionx::AccountInfoType::BALANCE) {
            return balance;
        }
        return 0.0;
    }

    std::string get_info_str(const optionx::AccountInfoRequest&) const override {
        return {};
    }

    optionx::AccountType get_info_account_type(const optionx::AccountInfoRequest&) const override {
        return optionx::AccountType::UNKNOWN;
    }

    optionx::CurrencyType get_info_currency(const optionx::AccountInfoRequest&) const override {
        return optionx::CurrencyType::USD;
    }
};

class RecordingBridge final : public optionx::bridges::BaseBridge {
public:
    explicit RecordingBridge(std::vector<std::string>& calls)
        : calls_(calls) {}

    optionx::bridge_status_callback_t& on_status_update() override {
        return status_callback_;
    }

    trade_signal_callback_t& on_trade_signal() override {
        return trade_signal_callback_;
    }

    signal_report_callback_t& on_signal_report() override {
        return signal_report_callback_;
    }

    signal_id_allocator_t& on_signal_id() override {
        return signal_id_allocator_;
    }

    void update_account_info(const optionx::AccountInfoUpdate& info) override {
        calls_.push_back("bridge.update_account_info");
        last_account_update = info;
        ++account_update_count;
    }

    void run() override {
        calls_.push_back("bridge.run");
        ++run_count;
    }

    void shutdown() override {
        calls_.push_back("bridge.shutdown");
        ++shutdown_count;
    }

    std::optional<optionx::AccountInfoUpdate> last_account_update;
    int account_update_count = 0;
    int run_count = 0;
    int shutdown_count = 0;

private:
    std::vector<std::string>& calls_;
    optionx::bridge_status_callback_t status_callback_;
    trade_signal_callback_t trade_signal_callback_;
    signal_report_callback_t signal_report_callback_;
    signal_id_allocator_t signal_id_allocator_;
};

} // namespace

TEST(BridgeHost, RefreshesAccountInfoFromBeforeRunHook) {
    std::vector<std::string> calls;
    RecordingBridge bridge(calls);
    optionx::bridges::BridgeHost host(bridge);

    auto account = std::make_shared<TestAccountInfo>();
    account->balance = 42.5;
    host.set_account_info_provider([&calls, account]() -> std::optional<optionx::AccountInfoUpdate> {
        calls.push_back("provider.account");
        return optionx::AccountInfoUpdate(
            account,
            optionx::AccountUpdateStatus::CONNECTED,
            1007);
    });
    host.hooks().before_run = [&calls](optionx::bridges::BridgeHost& current_host) {
        calls.push_back("host.before_run");
        EXPECT_TRUE(current_host.refresh_account_info());
    };
    host.hooks().after_run = [&calls](optionx::bridges::BridgeHost&) {
        calls.push_back("host.after_run");
    };

    host.run();

    const std::vector<std::string> expected = {
        "host.before_run",
        "provider.account",
        "bridge.update_account_info",
        "bridge.run",
        "host.after_run"
    };
    EXPECT_EQ(calls, expected);
    EXPECT_TRUE(host.run_requested());
    EXPECT_EQ(bridge.run_count, 1);
    ASSERT_TRUE(bridge.last_account_update.has_value());
    EXPECT_EQ(bridge.last_account_update->status, optionx::AccountUpdateStatus::CONNECTED);
    EXPECT_EQ(bridge.last_account_update->account_id, 1007);
    ASSERT_TRUE(bridge.last_account_update->account_info);
    EXPECT_EQ(
        bridge.last_account_update->account_info->get_info<double>(optionx::AccountInfoType::BALANCE),
        42.5);
}

TEST(BridgeHost, RefreshAccountInfoNoopsWithoutUpdate) {
    std::vector<std::string> calls;
    RecordingBridge bridge(calls);
    optionx::bridges::BridgeHost host(bridge);

    EXPECT_FALSE(host.refresh_account_info());

    host.set_account_info_provider([]() -> std::optional<optionx::AccountInfoUpdate> {
        return std::nullopt;
    });
    EXPECT_FALSE(host.refresh_account_info());
    EXPECT_EQ(bridge.account_update_count, 0);
    EXPECT_TRUE(calls.empty());
}

TEST(BridgeHost, ResetUsesShutdownPathAndHooks) {
    std::vector<std::string> calls;
    RecordingBridge bridge(calls);
    optionx::bridges::BridgeHost host(bridge);

    host.hooks().before_reset = [&calls](optionx::bridges::BridgeHost&) {
        calls.push_back("host.before_reset");
    };
    host.hooks().before_shutdown = [&calls](optionx::bridges::BridgeHost&) {
        calls.push_back("host.before_shutdown");
    };
    host.hooks().after_shutdown = [&calls](optionx::bridges::BridgeHost&) {
        calls.push_back("host.after_shutdown");
    };
    host.hooks().after_reset = [&calls](optionx::bridges::BridgeHost&) {
        calls.push_back("host.after_reset");
    };

    host.run();
    calls.clear();

    host.reset();

    const std::vector<std::string> expected = {
        "host.before_reset",
        "host.before_shutdown",
        "bridge.shutdown",
        "host.after_shutdown",
        "host.after_reset"
    };
    EXPECT_EQ(calls, expected);
    EXPECT_FALSE(host.run_requested());
    EXPECT_EQ(bridge.shutdown_count, 1);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
