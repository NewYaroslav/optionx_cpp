#include <gtest/gtest.h>

#include <optionx_cpp/bridges.hpp>

#include <chrono>
#include <condition_variable>
#include <mutex>

#if defined(_WIN32)
#include <SimpleNamedPipe/NamedPipeClient.hpp>
#endif

namespace {

class TestAccountInfo final : public optionx::BaseAccountInfoData {
public:
    std::int64_t user_id = 42;
    double balance = 1234.5;
    optionx::CurrencyType currency = optionx::CurrencyType::RUB;
    optionx::AccountType account_type = optionx::AccountType::DEMO;
    bool connected = true;

    std::unique_ptr<optionx::BaseAccountInfoData> clone_unique() const override {
        return std::make_unique<TestAccountInfo>(*this);
    }

    std::shared_ptr<optionx::BaseAccountInfoData> clone_shared() const override {
        return std::make_shared<TestAccountInfo>(*this);
    }

private:
    bool get_info_bool(const optionx::AccountInfoRequest& request) const override {
        if (request.type == optionx::AccountInfoType::CONNECTION_STATUS) {
            return connected;
        }
        return false;
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
            break;
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
        if (request.type == optionx::AccountInfoType::USER_ID) {
            return std::to_string(user_id);
        }
        return {};
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

using optionx::bridges::IntradeBarLegacyBridge;

std::string make_test_pipe_name() {
    const auto stamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    return "OptionxIntradeLegacyBridgeTest_" + std::to_string(stamp);
}

} // namespace

TEST(IntradeBarLegacyBridge, ConfigRoundTripsJson) {
    optionx::bridges::intrade_bar_legacy::BridgeConfig config;
    config.named_pipe = "custom_pipe";
    config.min_payout = 0.72;
    config.buffer_size = 4096;
    config.pipe_timeout_ms = 100;
    config.ping_period_ms = 3000;

    nlohmann::json json;
    config.to_json(json);

    optionx::bridges::intrade_bar_legacy::BridgeConfig restored;
    restored.from_json(json);

    EXPECT_EQ(restored.named_pipe, "custom_pipe");
    EXPECT_DOUBLE_EQ(restored.min_payout, 0.72);
    EXPECT_EQ(restored.buffer_size, 4096u);
    EXPECT_EQ(restored.pipe_timeout_ms, 100u);
    EXPECT_EQ(restored.ping_period_ms, 3000);
    EXPECT_TRUE(restored.validate().first);
}

TEST(IntradeBarLegacyBridge, ParsesSprintContract) {
    const nlohmann::json contract = {
        {"s", "btc/usd"},
        {"note", "signal-a&payload"},
        {"a", 5.5},
        {"dir", "buy"},
        {"dur", 300}
    };

    const auto request = IntradeBarLegacyBridge::parse_contract(contract, 0.6);

    ASSERT_TRUE(request);
    EXPECT_EQ(request->symbol, "BTCUSDT");
    EXPECT_EQ(request->signal_name, "signal-a");
    EXPECT_EQ(request->user_data, "payload");
    EXPECT_DOUBLE_EQ(request->amount, 5.5);
    EXPECT_EQ(request->order_type, optionx::OrderType::BUY);
    EXPECT_EQ(request->option_type, optionx::OptionType::SPRINT);
    EXPECT_EQ(request->duration, 300u);
    EXPECT_EQ(request->expiry_time, 0);
    EXPECT_DOUBLE_EQ(request->min_payout, 0.6);
}

TEST(IntradeBarLegacyBridge, ParsesClassicExpiryAndDurationModes) {
    const nlohmann::json duration_contract = {
        {"s", "EURUSD"},
        {"note", "payload-only"},
        {"a", 10.0},
        {"dir", "SELL"},
        {"exp", 900}
    };

    const auto duration_request =
        IntradeBarLegacyBridge::parse_contract(duration_contract, 0.0);

    EXPECT_EQ(duration_request->symbol, "EURUSD");
    EXPECT_EQ(duration_request->user_data, "payload-only");
    EXPECT_EQ(duration_request->signal_name, "");
    EXPECT_EQ(duration_request->order_type, optionx::OrderType::SELL);
    EXPECT_EQ(duration_request->option_type, optionx::OptionType::CLASSIC);
    EXPECT_EQ(duration_request->duration, 900u);
    EXPECT_EQ(duration_request->expiry_time, 0);

    const nlohmann::json expiry_contract = {
        {"s", "EURUSD"},
        {"note", "sig&data"},
        {"a", 10.0},
        {"dir", "SELL"},
        {"exp", 1900000000}
    };

    const auto expiry_request =
        IntradeBarLegacyBridge::parse_contract(expiry_contract, 0.0);

    EXPECT_EQ(expiry_request->option_type, optionx::OptionType::CLASSIC);
    EXPECT_EQ(expiry_request->duration, 0u);
    EXPECT_EQ(expiry_request->expiry_time, 1900000000);
}

TEST(IntradeBarLegacyBridge, RejectsInvalidContractFields) {
    EXPECT_THROW(
        IntradeBarLegacyBridge::parse_contract(
            nlohmann::json{{"s", "INVALID"}, {"note", ""}, {"a", 1.0}, {"dir", "BUY"}, {"dur", 60}},
            0.0),
        std::invalid_argument);

    EXPECT_THROW(
        IntradeBarLegacyBridge::parse_contract(
            nlohmann::json{{"s", "EURUSD"}, {"note", ""}, {"a", 1.0}, {"dir", "SIDEWAYS"}, {"dur", 60}},
            0.0),
        std::invalid_argument);
}

TEST(IntradeBarLegacyBridge, FormatsTradeResultUpdate) {
    optionx::TradeRequest request;
    request.symbol = "EURUSD";
    request.signal_name = "signal";
    request.user_data = "payload";
    request.order_type = optionx::OrderType::SELL;
    request.duration = 300;

    optionx::TradeResult result;
    result.trade_id = 77;
    result.option_id = 12345;
    result.open_price = 1.2345;
    result.close_price = 1.23;
    result.amount = 10.0;
    result.profit = 8.2;
    result.payout = 0.82;
    result.send_date = 1000;
    result.open_date = 2000;
    result.close_date = 302000;
    result.trade_state = optionx::TradeState::WIN;

    const auto message = nlohmann::json::parse(
        IntradeBarLegacyBridge::format_trade_result(request, result));
    const auto& update = message.at("update_bet");

    EXPECT_EQ(update.at("s").get<std::string>(), "EURUSD");
    EXPECT_EQ(update.at("note").get<std::string>(), "signal&payload");
    EXPECT_EQ(update.at("aid").get<std::uint32_t>(), 77u);
    EXPECT_EQ(update.at("id").get<std::int64_t>(), 12345);
    EXPECT_EQ(update.at("dir").get<std::string>(), "sell");
    EXPECT_EQ(update.at("status").get<std::string>(), "win");
    EXPECT_EQ(update.at("dur").get<std::uint32_t>(), 300u);
    EXPECT_DOUBLE_EQ(update.at("st").get<double>(), 1.0);
    EXPECT_DOUBLE_EQ(update.at("ot").get<double>(), 2.0);
    EXPECT_DOUBLE_EQ(update.at("ct").get<double>(), 302.0);
}

TEST(IntradeBarLegacyBridge, FormatsOpenStatesAsWait) {
    optionx::TradeRequest request;
    optionx::TradeResult result;
    result.trade_state = optionx::TradeState::IN_PROGRESS;

    const auto message = nlohmann::json::parse(
        IntradeBarLegacyBridge::format_trade_result(request, result));

    EXPECT_EQ(
        message.at("update_bet").at("status").get<std::string>(),
        "wait");
}

TEST(IntradeBarLegacyBridge, FormatsAccountSnapshots) {
    TestAccountInfo account_info;

    const auto balance = nlohmann::json::parse(
        IntradeBarLegacyBridge::format_balance_update(account_info));
    EXPECT_DOUBLE_EQ(balance.at("b").get<double>(), 1234.5);
    EXPECT_EQ(balance.at("rub").get<int>(), 1);
    EXPECT_EQ(balance.at("demo").get<int>(), 1);

    const auto connection = nlohmann::json::parse(
        IntradeBarLegacyBridge::format_connection_update(account_info));
    EXPECT_EQ(connection.at("conn").get<int>(), 1);
    EXPECT_EQ(connection.at("aid").get<std::int64_t>(), 42);
}

TEST(IntradeBarLegacyBridge, SendsTradeResultThroughNamedPipe) {
#if defined(_WIN32)
    optionx::bridges::IntradeBarLegacyBridge bridge;
    const std::string pipe_name = make_test_pipe_name();

    auto config = std::make_unique<optionx::bridges::intrade_bar_legacy::BridgeConfig>();
    config->named_pipe = pipe_name;
    config->buffer_size = 4096;
    config->ping_period_ms = 60000;
    ASSERT_TRUE(bridge.configure(std::move(config)));

    struct BridgeGuard {
        optionx::bridges::IntradeBarLegacyBridge& bridge;
        ~BridgeGuard() { bridge.shutdown(); }
    } guard{bridge};

    std::mutex mutex;
    std::condition_variable cv;
    bool server_started = false;
    bool request_received = false;
    std::string status_error;

    bridge.on_status_update() = [&](const optionx::BridgeStatusUpdate& update) {
        std::lock_guard<std::mutex> lock(mutex);
        if (update.status == optionx::BridgeStatus::SERVER_STARTED) {
            server_started = true;
        }
        if (update.status == optionx::BridgeStatus::SERVER_START_FAILED ||
            update.status == optionx::BridgeStatus::CONNECTION_ERROR) {
            status_error = update.message;
        }
        cv.notify_all();
    };

    bridge.on_place_trade() = [&](std::unique_ptr<optionx::TradeRequest> request) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            request_received = true;
        }

        auto result = request->create_trade_result_unique();
        result->trade_id = 101;
        result->option_id = 202;
        result->amount = request->amount;
        result->profit = 8.2;
        result->payout = 0.82;
        result->trade_state = optionx::TradeState::WIN;
        request->dispatch_callbacks(request, result);
        cv.notify_all();
    };

    bridge.run();

    {
        std::unique_lock<std::mutex> lock(mutex);
        ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(3), [&] {
            return server_started || !status_error.empty();
        })) << "Bridge server did not start";
        ASSERT_TRUE(status_error.empty()) << status_error;
    }

    SimpleNamedPipe::ClientConfig client_config(pipe_name, 4096, 3000);
    SimpleNamedPipe::NamedPipeClient client(client_config);
    std::error_code client_error;

    ASSERT_TRUE(client.connect(&client_error)) << client_error.message();

    const std::string contract = R"json({
        "contract": {
            "s": "EURUSD",
            "note": "pipe-signal&payload",
            "a": 10.0,
            "dir": "BUY",
            "dur": 300
        }
    })json";

    ASSERT_TRUE(client.write(contract, &client_error)) << client_error.message();

    std::string response;
    ASSERT_TRUE(client.read(response, 3000, &client_error)) << client_error.message();

    {
        std::unique_lock<std::mutex> lock(mutex);
        ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(1), [&] {
            return request_received;
        }));
    }

    const auto message = nlohmann::json::parse(response);
    const auto& update = message.at("update_bet");
    EXPECT_EQ(update.at("s").get<std::string>(), "EURUSD");
    EXPECT_EQ(update.at("note").get<std::string>(), "pipe-signal&payload");
    EXPECT_EQ(update.at("aid").get<std::uint32_t>(), 101u);
    EXPECT_EQ(update.at("id").get<std::int64_t>(), 202);
    EXPECT_EQ(update.at("dir").get<std::string>(), "buy");
    EXPECT_EQ(update.at("status").get<std::string>(), "win");

    client.close();
#else
    GTEST_SKIP() << "Legacy named-pipe transport is Windows-only.";
#endif
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
