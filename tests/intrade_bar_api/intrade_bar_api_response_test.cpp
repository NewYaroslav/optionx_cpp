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
    EXPECT_TRUE(result.has_http_status());
    EXPECT_DOUBLE_EQ(result.value.balance, 42.5);
    EXPECT_EQ(result.value.currency, CurrencyType::USD);
    EXPECT_TRUE(result.error_message.empty());
}

TEST(IntradeBarApiResponses, ApiResultCarriesTypedFailure) {
    auto result = TradeOpenResult::fail("blocked", 451);

    EXPECT_FALSE(result);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.status_code, 451);
    EXPECT_TRUE(result.has_http_status());
    EXPECT_EQ(result.error_message, "blocked");
    EXPECT_EQ(result.value.option_id, 0);
}

TEST(IntradeBarApiResponses, ApiResultMarksMissingHttpStatusExplicitly) {
    auto success = HostAvailabilityResult::ok(HostAvailability{true});
    EXPECT_TRUE(success);
    EXPECT_EQ(success.status_code, HostAvailabilityResult::NO_HTTP_STATUS);
    EXPECT_FALSE(success.has_http_status());

    auto failure = ProfileInfoResult::fail("no response");
    EXPECT_FALSE(failure);
    EXPECT_EQ(failure.status_code, ProfileInfoResult::NO_RESPONSE_STATUS);
    EXPECT_FALSE(failure.has_http_status());
}

TEST(IntradeBarAuthData, KeepsDisconnectedDomainRetryPeriodInConfig) {
    AuthData auth_data;
    auth_data.set_email_password("user@example.test", "secret");
    auth_data.account_type = AccountType::DEMO;
    auth_data.currency = CurrencyType::USD;
    auth_data.disconnected_domain_retry_period_ms = 12345;
    auth_data.trade_history_source = TradeHistorySource::HTML_CSV;

    nlohmann::json json;
    auth_data.to_json(json);
    ASSERT_EQ(json.at("disconnected_domain_retry_period_ms").get<int64_t>(), 12345);
    ASSERT_EQ(json.at("trade_history_source").get<std::string>(), "HTML_CSV");

    AuthData restored;
    restored.from_json(json);
    EXPECT_EQ(restored.disconnected_domain_retry_period_ms, 12345);
    EXPECT_EQ(restored.trade_history_source, TradeHistorySource::HTML_CSV);

    auto [valid, message] = restored.validate();
    EXPECT_TRUE(valid) << message;

    restored.disconnected_domain_retry_period_ms = 0;
    auto [invalid, invalid_message] = restored.validate();
    EXPECT_FALSE(invalid);
    EXPECT_EQ(invalid_message, "Disconnected domain retry period must be positive");
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

TEST(IntradeBarApiResponses, AppliesTradeCheckInfoToTradeResultOutcome) {
    TradeResult standoff;
    standoff.amount = 10.0;
    ASSERT_TRUE(apply_trade_check_info_to_result(TradeCheckInfo{1.2345, 10.0}, standoff));
    EXPECT_EQ(standoff.trade_state, TradeState::STANDOFF);
    EXPECT_EQ(standoff.live_state, TradeState::STANDOFF);
    EXPECT_DOUBLE_EQ(standoff.close_price, 1.2345);
    EXPECT_DOUBLE_EQ(standoff.profit, 0.0);
    EXPECT_EQ(standoff.error_code, TradeErrorCode::SUCCESS);

    TradeResult loss;
    loss.amount = 10.0;
    ASSERT_TRUE(apply_trade_check_info_to_result(TradeCheckInfo{1.2340, 0.0}, loss));
    EXPECT_EQ(loss.trade_state, TradeState::LOSS);
    EXPECT_EQ(loss.live_state, TradeState::LOSS);
    EXPECT_DOUBLE_EQ(loss.profit, -10.0);

    TradeResult win;
    win.amount = 10.0;
    ASSERT_TRUE(apply_trade_check_info_to_result(TradeCheckInfo{1.2350, 18.0}, win));
    EXPECT_EQ(win.trade_state, TradeState::WIN);
    EXPECT_EQ(win.live_state, TradeState::WIN);
    EXPECT_DOUBLE_EQ(win.profit, 8.0);
    EXPECT_DOUBLE_EQ(win.payout, 0.8);
}

TEST(IntradeBarApiResponses, RequiresAmountToClassifyTradeCheckInfo) {
    TradeResult result;

    EXPECT_FALSE(apply_trade_check_info_to_result(TradeCheckInfo{1.2345, 1.0}, result));
    EXPECT_EQ(result.trade_state, TradeState::CHECK_ERROR);
    EXPECT_EQ(result.live_state, TradeState::CHECK_ERROR);
    EXPECT_EQ(result.error_code, TradeErrorCode::INVALID_REQUEST);
    EXPECT_EQ(
        result.error_desc,
        "Trade amount is required to classify Intrade Bar trade result.");
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

TEST(IntradeBarApiResponses, TrimsSettingsSwitchResponseBeforeClassification) {
    const auto ok_result = parse_settings_switch_response(" ok\n", 200, "currency");
    ASSERT_TRUE(ok_result);
    EXPECT_EQ(ok_result.value.failure_reason, SettingsSwitchFailureReason::NONE);

    const auto error_result = parse_settings_switch_response(" error\r\n", 200, "account type");
    EXPECT_FALSE(error_result);
    EXPECT_EQ(error_result.value.failure_reason, SettingsSwitchFailureReason::BROKER_REJECTED);
    EXPECT_TRUE(error_result.value.should_retry());
    EXPECT_EQ(error_result.value.response_body, " error\r\n");
}

TEST(IntradeBarApiResponses, ClassifiesUnexpectedSettingsSwitchResponseAsNonRetryable) {
    const auto result = parse_settings_switch_response("session expired", 200, "currency");

    EXPECT_FALSE(result);
    EXPECT_EQ(result.status_code, 200);
    EXPECT_EQ(result.value.failure_reason, SettingsSwitchFailureReason::UNEXPECTED_RESPONSE);
    EXPECT_FALSE(result.value.should_retry());
    EXPECT_EQ(result.value.response_body, "session expired");
}

TEST(IntradeBarApiResponses, ParsesTradeHistorySourceConfigValues) {
    EXPECT_EQ(trade_history_source_from_string("HTML"), TradeHistorySource::HTML);
    EXPECT_EQ(trade_history_source_from_string("CSV"), TradeHistorySource::CSV);
    EXPECT_EQ(trade_history_source_from_string("html+csv"), TradeHistorySource::HTML_CSV);
    EXPECT_EQ(trade_history_source_from_string("html_csv"), TradeHistorySource::HTML_CSV);
    EXPECT_EQ(
        trade_history_source_from_string("bad", TradeHistorySource::HTML),
        TradeHistorySource::HTML);
}

TEST(IntradeBarApiResponses, ParsesTradeHistoryCsvExport) {
    const std::string csv =
        "id;Type;Asset;Direction;Open;Close;Open quote;Close quote;Amount;Result\n"
        ";Sprint;EUR/GBP;Down;14:44:43, 14 Dec 20;14:47:43, 14 Dec 20;0.90492;0.90512;1 USD;1.82 USD\n"
        "123;Sprint;AUD/NZD;Up;20:52:19, 28 Jun 21;20:55:19, 28 Jun 21;1.07411;1.07417;500 USD;0 USD\n"
        ";Sprint;AUD/CAD;Up;16:34:33, 23 Jun 21;16:37:33, 23 Jun 21;0.93034;0.93008;50 RUB;50 RUB\n"
        ";Sprint;BTCUSD;Up;16:34:33, 23 Jun 21;16:39:33, 23 Jun 21;62830.01;62850.00;1 USD;1.79 USD\n";

    const auto trades = parse_trade_history_csv_export(csv, AccountType::DEMO);
    ASSERT_EQ(trades.size(), 4u);

    EXPECT_EQ(trades[0].symbol, "EURGBP");
    EXPECT_EQ(trades[0].option_type, OptionType::SPRINT);
    EXPECT_EQ(trades[0].order_type, OrderType::SELL);
    EXPECT_EQ(trades[0].trade_state, TradeState::WIN);
    EXPECT_EQ(trades[0].live_state, TradeState::WIN);
    EXPECT_DOUBLE_EQ(trades[0].amount, 1.0);
    EXPECT_DOUBLE_EQ(trades[0].profit, 0.82);
    EXPECT_DOUBLE_EQ(trades[0].payout, 0.82);
    EXPECT_EQ(trades[0].currency, CurrencyType::USD);
    EXPECT_EQ(trades[0].account_type, AccountType::DEMO);
    EXPECT_EQ(trades[0].platform_type, PlatformType::INTRADE_BAR);
    EXPECT_EQ(trades[0].duration, 180);

    EXPECT_EQ(trades[1].option_id, 123);
    EXPECT_EQ(trades[1].symbol, "AUDNZD");
    EXPECT_EQ(trades[1].order_type, OrderType::BUY);
    EXPECT_EQ(trades[1].trade_state, TradeState::LOSS);
    EXPECT_DOUBLE_EQ(trades[1].profit, -500.0);

    EXPECT_EQ(trades[2].symbol, "AUDCAD");
    EXPECT_EQ(trades[2].currency, CurrencyType::RUB);
    EXPECT_EQ(trades[2].trade_state, TradeState::STANDOFF);
    EXPECT_DOUBLE_EQ(trades[2].profit, 0.0);

    EXPECT_EQ(trades[3].symbol, "BTCUSDT");
    EXPECT_EQ(trades[3].trade_state, TradeState::WIN);
    EXPECT_EQ(trades[3].duration, 300);
}

TEST(IntradeBarApiResponses, ParsesTradeHistoryHtmlSnapshotAndMergesWithCsv) {
    const std::string html = R"HTML(
        <tbody class="table_tbody" id="trade_history">
            <tr id="trade_inv_224157357" data-id="224157357" data-option="BTCUSDT" data-rate="62830.01" data-timeopen="1719146073" data-timeclose="1719146373" data-status="1" data-contract="0">
            </tr>
            <tr id="trade_inv_224157358" data-id="224157358" data-option="EUR/USD" data-rate="1.07001" data-timeopen="1719146074" data-status="2" data-contract="1">
            </tr>
        </tbody>
    )HTML";

    const auto html_trades = parse_trade_history_html_snapshot(html, AccountType::DEMO);
    ASSERT_EQ(html_trades.size(), 2u);
    EXPECT_EQ(html_trades[0].option_id, 224157357);
    EXPECT_EQ(html_trades[0].symbol, "BTCUSDT");
    EXPECT_EQ(html_trades[0].order_type, OrderType::BUY);
    EXPECT_EQ(html_trades[0].option_type, OptionType::SPRINT);
    EXPECT_EQ(html_trades[0].open_date, time_shield::sec_to_ms(1719146073));
    EXPECT_EQ(html_trades[0].close_date, time_shield::sec_to_ms(1719146373));
    EXPECT_EQ(html_trades[0].duration, 300);

    std::vector<TradeResult> csv_trades;
    TradeResult csv_trade;
    csv_trade.symbol = "BTCUSDT";
    csv_trade.open_date = time_shield::sec_to_ms(1719146074);
    csv_trade.open_price = 62830.01;
    csv_trade.amount = 1.0;
    csv_trade.trade_state = TradeState::WIN;
    csv_trades.push_back(csv_trade);

    const auto merged = merge_trade_history_csv_with_html(
        std::move(csv_trades),
        html_trades);
    ASSERT_EQ(merged.size(), 2u);
    EXPECT_EQ(merged[0].option_id, 224157357);
    EXPECT_EQ(merged[0].symbol, "BTCUSDT");
    EXPECT_EQ(merged[0].duration, 300);
    EXPECT_EQ(merged[1].option_id, 224157358);
}

TEST(IntradeBarApiResponses, ParsesTradeCloseHtmlPageAndNextCursor) {
    const std::string html = R"HTML(
        <div id="trade_close_block" class="hide">
            <table class="">
                <tbody class="table_tbody" id="trade_close">
                    <tr class="trade_list_type trade_list_type_1" >
                        <th class="center"><div class="trading-table__up-td"></div></th>
                        <th>
                            224157357
                            <br>
                            19:06:42, 22.06.26
                            <br>
                            19:11:42, 22.06.26
                        </th>
                        <th>
                            BTC/USDT
                            <br>
                            64708.01
                            <br>
                            64735.64
                        </th>
                        <th>
                            <br>
                            1 $
                            <br>
                            1.79 $
                        </th>
                    </tr>
                    <tr class="trade_list_type trade_list_type_1" >
                        <th class="center"><div class="trading-table__down-td"></div></th>
                        <th>
                            224130715
                            <br>
                            09:57:43, 19.06.26
                            <br>
                            10:03:43, 19.06.26
                        </th>
                        <th>
                            BTC/USDT
                            <br>
                            62884.8
                            <br>
                            62854.73
                        </th>
                        <th>
                            <br>
                            1 $
                            <br>
                            1.79 $
                        </th>
                    </tr>
                    <tr class="trade_list_type trade_list_type_1" >
                        <th class="center"><div class="trading-table__up-td"></div></th>
                        <th>
                            224130651
                            <br>
                            09:29:34, 19.06.26
                            <br>
                            09:34:34, 19.06.26
                        </th>
                        <th>
                            BTC/USDT
                            <br>
                            62830.01
                            <br>
                            62825.99
                        </th>
                        <th>
                            <br>
                            1 $
                            <br>
                            0 $
                        </th>
                    </tr>
                </tbody>
            </table>
            <div class="text-center">
                <a class="trading-tables__btn btn btn--gray trade_btn_load_more" id="trade_btn_load_more" data-last="224130496">load more</a>
            </div>
        </div>
    )HTML";

    const auto page = parse_trade_history_html_page(html, AccountType::DEMO);
    ASSERT_EQ(page.trades.size(), 3u);
    EXPECT_EQ(page.next_last, "224130496");

    EXPECT_EQ(page.trades[0].option_id, 224157357);
    EXPECT_EQ(page.trades[0].symbol, "BTCUSDT");
    EXPECT_EQ(page.trades[0].order_type, OrderType::BUY);
    EXPECT_EQ(page.trades[0].trade_state, TradeState::WIN);
    EXPECT_EQ(page.trades[0].duration, 300);
    EXPECT_DOUBLE_EQ(page.trades[0].amount, 1.0);
    EXPECT_DOUBLE_EQ(page.trades[0].profit, 0.79);
    EXPECT_DOUBLE_EQ(page.trades[0].payout, 0.79);
    EXPECT_EQ(page.trades[0].currency, CurrencyType::USD);

    EXPECT_EQ(page.trades[1].option_id, 224130715);
    EXPECT_EQ(page.trades[1].order_type, OrderType::SELL);
    EXPECT_EQ(page.trades[1].duration, 360);
    EXPECT_EQ(page.trades[1].trade_state, TradeState::WIN);

    EXPECT_EQ(page.trades[2].option_id, 224130651);
    EXPECT_EQ(page.trades[2].order_type, OrderType::BUY);
    EXPECT_EQ(page.trades[2].trade_state, TradeState::LOSS);
    EXPECT_DOUBLE_EQ(page.trades[2].profit, -1.0);
}

TEST(IntradeBarApiResponses, ParsesTradeLoadMoreRowsWithoutTableWrapper) {
    const std::string html = R"HTML(
        <tr class="trade_list_type trade_list_type_1" >
            <th class="center"><div class="trading-table__up-td"></div></th>
            <th>
                224130496
                <br>
                08:02:55, 19.06.26
                <br>
                08:07:55, 19.06.26
            </th>
            <th>
                BTC/USDT
                <br>
                62577.98
                <br>
                62615.99
            </th>
            <th>
                <br>
                1 $
                <br>
                1.6 $
            </th>
        </tr>
        <script>
            $('#trade_btn_load_more').attr('data-last', '')
        </script>
    )HTML";

    const auto page = parse_trade_history_html_page(html, AccountType::DEMO);
    ASSERT_EQ(page.trades.size(), 1u);
    EXPECT_TRUE(page.next_last.empty());
    EXPECT_EQ(page.trades[0].option_id, 224130496);
    EXPECT_EQ(page.trades[0].trade_state, TradeState::WIN);
    EXPECT_DOUBLE_EQ(page.trades[0].profit, 0.6);
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

TEST(IntradeBarApiResponses, ParsesActiveTradeRowsWithFlexibleTrAttributes) {
    const std::string html = R"HTML(
        <tbody class="table_tbody" id="trade_active">
            <tr class="trade_graph_tick" data-id = "224130651" id="trade_inv_224130651" data-option = "BTCUSDT" data-rate = "62830.01" data-timeopen = "1781850574" data-status = "1" data-contract = "0">
            </tr>
            <tr  id='trade_inv_224130777' data-id='224130777' data-option='BTCUSDT' data-rate='62831.25' data-timeopen='1781850580' data-status='2' data-contract='0'>
            </tr>
            <tr class="ignored" id="not_trade_inv_224130999" data-id="224130999">
            </tr>
        </tbody>
    )HTML";

    const auto trades = parse_active_trades_snapshot(html);
    ASSERT_EQ(trades.size(), 2u);

    EXPECT_EQ(trades[0].id, 224130651);
    EXPECT_EQ(trades[0].symbol, "BTCUSDT");
    EXPECT_DOUBLE_EQ(trades[0].open_price, 62830.01);
    EXPECT_EQ(trades[0].status, 1);

    EXPECT_EQ(trades[1].id, 224130777);
    EXPECT_EQ(trades[1].symbol, "BTCUSDT");
    EXPECT_DOUBLE_EQ(trades[1].open_price, 62831.25);
    EXPECT_EQ(trades[1].status, 2);
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
