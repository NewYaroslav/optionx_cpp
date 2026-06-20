#include <gtest/gtest.h>

#include <optionx_cpp/platforms/IntradeBarPlatform.hpp>

using namespace optionx;
using namespace optionx::platforms;
using namespace optionx::platforms::intrade_bar;

TEST(IntradeBarApiResponses, ApiResultCarriesTypedSuccessPayload) {
    auto result = BalanceInfoResult::ok(BalanceInfo{42.5, CurrencyType::USD}, 200);

    ASSERT_TRUE(result);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.status_code, 200);
    EXPECT_DOUBLE_EQ(result.value.balance, 42.5);
    EXPECT_EQ(result.value.currency, CurrencyType::USD);
    EXPECT_TRUE(result.error_message.empty());
}

TEST(IntradeBarApiResponses, ApiResultCarriesTypedFailure) {
    auto result = TradeOpenResult::fail("blocked", 451);

    EXPECT_FALSE(result);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.status_code, 451);
    EXPECT_EQ(result.error_message, "blocked");
    EXPECT_EQ(result.value.option_id, 0);
}

TEST(IntradeBarApiResponses, TradeWorkflowPayloadsKeepBrokerSpecificFieldsTyped) {
    TradeOpenInfo opened;
    opened.option_id = 123;
    opened.open_date = 456;
    opened.open_price = 1.2345;

    auto open_result = TradeOpenResult::ok(opened, 200);
    ASSERT_TRUE(open_result);
    EXPECT_EQ(open_result.value.option_id, 123);
    EXPECT_EQ(open_result.value.open_date, 456);
    EXPECT_DOUBLE_EQ(open_result.value.open_price, 1.2345);

    auto check_result = TradeCheckResult::ok(TradeCheckInfo{1.2350, 18.0}, 200);
    ASSERT_TRUE(check_result);
    EXPECT_DOUBLE_EQ(check_result.value.price, 1.2350);
    EXPECT_DOUBLE_EQ(check_result.value.profit, 18.0);
}

TEST(IntradeBarApiResponses, ParsesSuccessfulSettingsSwitchResponse) {
    const auto result = parse_settings_switch_response("ok", 200, "currency");

    ASSERT_TRUE(result);
    EXPECT_EQ(result.status_code, 200);
    EXPECT_EQ(result.value.failure_reason, SettingsSwitchFailureReason::NONE);
    EXPECT_FALSE(result.value.should_retry());
    EXPECT_TRUE(result.value.response_body.empty());
}

TEST(IntradeBarApiResponses, ClassifiesBrokerRejectedSettingsSwitchAsRetryable) {
    const auto result = parse_settings_switch_response("error", 200, "account type");

    EXPECT_FALSE(result);
    EXPECT_EQ(result.status_code, 200);
    EXPECT_EQ(result.value.failure_reason, SettingsSwitchFailureReason::BROKER_REJECTED);
    EXPECT_TRUE(result.value.should_retry());
    EXPECT_EQ(result.value.response_body, "error");
    EXPECT_NE(result.error_message.find("active trades"), std::string::npos);
}

TEST(IntradeBarApiResponses, ClassifiesUnexpectedSettingsSwitchResponseAsNonRetryable) {
    const auto result = parse_settings_switch_response("session expired", 200, "currency");

    EXPECT_FALSE(result);
    EXPECT_EQ(result.status_code, 200);
    EXPECT_EQ(result.value.failure_reason, SettingsSwitchFailureReason::UNEXPECTED_RESPONSE);
    EXPECT_FALSE(result.value.should_retry());
    EXPECT_EQ(result.value.response_body, "session expired");
}

TEST(IntradeBarApiResponses, ParsesActiveTradesAndLatestCloseTime) {
    const std::string html = R"HTML(
        <tbody class="table_tbody" id="trade_active">
            <tr id="trade_inv_224130651" data-id="224130651" data-option="BTCUSDT" data-rate="62830.01" data-timeopen="1781850574" data-status="1" data-contract="0">
                <script async>
                    time_time_224130651 = 283;
                    timer224130651 = setInterval(showRemaining, 1000, "timer_224130651", window.time_time_224130651, 224130651,'1', '1781850874');
                </script>
            </tr>
            <tr id="trade_inv_224130777" data-id="224130777" data-option="BTCUSDT" data-rate="62831.25" data-timeopen="1781850580" data-status="2" data-contract="0">
                <script async>
                    time_time_224130777 = 343;
                    timer224130777 = setInterval(showRemaining, 1000, "timer_224130777", window.time_time_224130777, 224130777,'2', '1781850940');
                </script>
            </tr>
        </tbody>
    )HTML";

    const auto trades = parse_active_trades_snapshot(html);
    ASSERT_EQ(trades.size(), 2u);

    EXPECT_EQ(trades[0].id, 224130651);
    EXPECT_EQ(trades[0].symbol, "BTCUSDT");
    EXPECT_DOUBLE_EQ(trades[0].open_price, 62830.01);
    EXPECT_EQ(trades[0].open_time_ms, time_shield::sec_to_ms(1781850574));
    EXPECT_EQ(trades[0].close_time_ms, time_shield::sec_to_ms(1781850874));
    EXPECT_EQ(trades[0].status, 1);
    EXPECT_EQ(trades[0].contract, 0);

    int64_t latest_close_ms = 0;
    for (const auto& trade : trades) {
        if (trade.close_time_ms > latest_close_ms) {
            latest_close_ms = trade.close_time_ms;
        }
    }
    EXPECT_EQ(latest_close_ms, time_shield::sec_to_ms(1781850940));
}

TEST(IntradeBarApiResponses, RejectsActiveTradesPageWithoutActiveBlock) {
    const std::string html = R"HTML(
        <tbody class="table_tbody" id="trade_history">
            <tr id="trade_inv_224130999" data-id="224130999" data-option="BTCUSDT" data-rate="62830.01" data-timeopen="1781850574">
                <script async>
                    timer224130999 = setInterval(showRemaining, 1000, "timer_224130999", 1, 224130999,'1', '1781850874');
                </script>
            </tr>
        </tbody>
    )HTML";

    EXPECT_THROW(
        static_cast<void>(parse_active_trades_snapshot(html)),
        std::runtime_error);
}

TEST(IntradeBarApiResponses, IgnoresTradeRowsOutsideActiveBlock) {
    const std::string html = R"HTML(
        <tbody class="table_tbody" id="trade_history">
            <tr id="trade_inv_224130999" data-id="224130999" data-option="BTCUSDT" data-rate="62830.01" data-timeopen="1781850574">
                <script async>
                    timer224130999 = setInterval(showRemaining, 1000, "timer_224130999", 1, 224130999,'1', '1781850874');
                </script>
            </tr>
        </tbody>
        <tbody class="table_tbody" id="trade_active">
        </tbody>
    )HTML";

    const auto trades = parse_active_trades_snapshot(html);
    EXPECT_TRUE(trades.empty());
}

TEST(IntradeBarApiResponses, SkipsActiveTradeRowsWithMalformedId) {
    const std::string html = R"HTML(
        <tbody class="table_tbody" id="trade_active">
            <tr id="trade_inv_224130651" data-id="224130651junk" data-option="BTCUSDT" data-rate="62830.01" data-timeopen="1781850574" data-status="1" data-contract="0">
            </tr>
        </tbody>
    )HTML";

    const auto trades = parse_active_trades_snapshot(html);
    EXPECT_TRUE(trades.empty());
}

TEST(IntradeBarApiResponses, RejectsPartialNumericActiveTradeFields) {
    const std::string html = R"HTML(
        <tbody class="table_tbody" id="trade_active">
            <tr id="trade_inv_224130651" data-id="224130651" data-option="BTCUSDT" data-rate="62830.01junk" data-timeopen="1781850574x" data-status="1x" data-contract="0x">
                <script async>
                    time_time_224130651 = 283x;
                </script>
            </tr>
        </tbody>
    )HTML";

    const auto trades = parse_active_trades_snapshot(html);
    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].id, 224130651);
    EXPECT_EQ(trades[0].symbol, "BTCUSDT");
    EXPECT_DOUBLE_EQ(trades[0].open_price, 0.0);
    EXPECT_EQ(trades[0].open_time_ms, 0);
    EXPECT_EQ(trades[0].status, 0);
    EXPECT_EQ(trades[0].contract, 0);
    EXPECT_EQ(trades[0].close_time_ms, 0);
}

TEST(IntradeBarApiResponses, EmptyTradeOpenResponseHasSpecificError) {
    bool called = false;
    bool success = true;
    long status_code = 0;
    std::string error_desc;

    parse_execute_trade(
        "",
        200,
        [&](bool parsed,
            long status,
            int64_t,
            int64_t,
            double,
            const std::string& error) {
            called = true;
            success = parsed;
            status_code = status;
            error_desc = error;
        });

    EXPECT_TRUE(called);
    EXPECT_FALSE(success);
    EXPECT_EQ(status_code, 200);
    EXPECT_EQ(
        error_desc,
        "Trade open failed. Server returned an empty response; instrument may be closed or unavailable.");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
