#include <gtest/gtest.h>

#include <optionx_cpp/bridges.hpp>

#include <stdexcept>
#include <string>

TEST(BotBinaryProtocol, FormatsDurationCommandForHttpAndFileSignals) {
    namespace bot = optionx::bridges::bot_binary;

    bot::BotBinaryAdapterConfig config;
    config.http_base_url = "http://127.0.0.2/";

    auto command = bot::bot_binary_duration_command(
        "frxEURAUD",
        optionx::OrderType::BUY,
        "1.00",
        5 * 60,
        "idem-001");
    command.transport_suffix = "2018.09.29=1538190215";

    const auto prepared = bot::prepare_bot_binary_command(command, config);

    EXPECT_EQ(
        prepared.request_query_value,
        "frxEURAUD=CALL=1.00=duration=5=m=");
    EXPECT_EQ(
        prepared.http_url,
        "http://127.0.0.2/?request=frxEURAUD%3DCALL%3D1.00%3Dduration%3D5%3Dm%3D");
    EXPECT_EQ(
        prepared.file_name,
        "frxEURAUD=CALL=1.00=duration=5=m=2018.09.29=1538190215.txt");
    EXPECT_EQ(prepared.transport_suffix, "2018.09.29=1538190215");
}

TEST(BotBinaryProtocol, FormatsAbsoluteEndTimeCommand) {
    namespace bot = optionx::bridges::bot_binary;

    auto command = bot::bot_binary_end_time_command(
        "R_50",
        optionx::OrderType::SELL,
        "1",
        1538264736,
        "idem-002");
    command.transport_suffix = "2018.09.29=1538264736";

    const auto prepared = bot::prepare_bot_binary_command(command);

    EXPECT_EQ(
        prepared.request_query_value,
        "R_50=PUT=1=endtime=1538264736=s=");
    EXPECT_EQ(
        prepared.file_name,
        "R_50=PUT=1=endtime=1538264736=s=2018.09.29=1538264736.txt");
}

TEST(BotBinaryProtocol, DerivesStableDefaultSuffixFromIdempotencyKey) {
    namespace bot = optionx::bridges::bot_binary;

    const auto first = bot::prepare_bot_binary_command(
        bot::bot_binary_duration_command(
            "R_25",
            optionx::OrderType::BUY,
            "2.50",
            60,
            "same-logical-operation"));
    const auto second = bot::prepare_bot_binary_command(
        bot::bot_binary_duration_command(
            "R_25",
            optionx::OrderType::BUY,
            "2.50",
            60,
            "same-logical-operation"));

    EXPECT_EQ(first.transport_suffix, second.transport_suffix);
    EXPECT_EQ(first.file_name, second.file_name);
    EXPECT_EQ(first.request_query_value, second.request_query_value);
    EXPECT_EQ(first.file_name.rfind("R_25=CALL=2.50=duration=1=m=ox_", 0), 0u);
}

TEST(BotBinaryProtocol, AcceptsOptionXIdempotencyKeySyntax) {
    namespace bot = optionx::bridges::bot_binary;

    const auto prepared = bot::prepare_bot_binary_command(
        bot::bot_binary_duration_command(
            "R_25",
            optionx::OrderType::BUY,
            "1.00",
            60,
            "manual:client-trade-1"));

    EXPECT_FALSE(prepared.transport_suffix.empty());
    EXPECT_EQ(prepared.file_name.rfind("R_25=CALL=1.00=duration=1=m=ox_", 0), 0u);
}

TEST(BotBinaryProtocol, CanIncludeSuffixInHttpRequestWhenConfigured) {
    namespace bot = optionx::bridges::bot_binary;

    bot::BotBinaryAdapterConfig config;
    config.include_suffix_in_http_request = true;

    auto command = bot::bot_binary_duration_command(
        "R_25",
        optionx::OrderType::SELL,
        "1",
        30,
        "idem-003");
    command.transport_suffix = "custom_suffix";

    const auto prepared = bot::prepare_bot_binary_command(command, config);

    EXPECT_EQ(
        prepared.request_query_value,
        "R_25=PUT=1=duration=30=s=custom_suffix");
    EXPECT_EQ(
        prepared.http_url,
        "http://127.0.0.2/?request=R_25%3DPUT%3D1%3Dduration%3D30%3Ds%3Dcustom_suffix");
    EXPECT_EQ(
        prepared.file_name,
        "R_25=PUT=1=duration=30=s=custom_suffix.txt");
}

TEST(BotBinaryProtocol, EncodesPercentSuffixForHttpWithoutChangingRawValues) {
    namespace bot = optionx::bridges::bot_binary;

    bot::BotBinaryAdapterConfig config;
    config.include_suffix_in_http_request = true;

    auto command = bot::bot_binary_duration_command(
        "R_25",
        optionx::OrderType::BUY,
        "1",
        60,
        "idem-percent");
    command.transport_suffix = "legacy%2Bsignal";

    const auto prepared = bot::prepare_bot_binary_command(command, config);

    EXPECT_EQ(
        prepared.request_query_value,
        "R_25=CALL=1=duration=1=m=legacy%2Bsignal");
    EXPECT_EQ(
        prepared.http_url,
        "http://127.0.0.2/?request=R_25%3DCALL%3D1%3Dduration%3D1%3Dm%3Dlegacy%252Bsignal");
    EXPECT_EQ(
        prepared.file_name,
        "R_25=CALL=1=duration=1=m=legacy%2Bsignal.txt");

    const auto http = bot::parse_bot_binary_http_request(prepared.http_url);
    const auto file = bot::parse_bot_binary_file_signal_name(prepared.file_name);

    EXPECT_EQ(http.transport_suffix, "legacy%2Bsignal");
    EXPECT_EQ(file.transport_suffix, "legacy%2Bsignal");
}

TEST(BotBinaryProtocol, EncodesPlusSuffixForHttpWithoutChangingRawValues) {
    namespace bot = optionx::bridges::bot_binary;

    bot::BotBinaryAdapterConfig config;
    config.include_suffix_in_http_request = true;

    auto command = bot::bot_binary_duration_command(
        "R_25",
        optionx::OrderType::BUY,
        "1",
        60,
        "idem-plus");
    command.transport_suffix = "legacy+signal";

    const auto prepared = bot::prepare_bot_binary_command(command, config);

    EXPECT_EQ(
        prepared.request_query_value,
        "R_25=CALL=1=duration=1=m=legacy+signal");
    EXPECT_EQ(
        prepared.http_url,
        "http://127.0.0.2/?request=R_25%3DCALL%3D1%3Dduration%3D1%3Dm%3Dlegacy%2Bsignal");
    EXPECT_EQ(
        prepared.file_name,
        "R_25=CALL=1=duration=1=m=legacy+signal.txt");

    const auto http = bot::parse_bot_binary_http_request(prepared.http_url);
    const auto file = bot::parse_bot_binary_file_signal_name(prepared.file_name);

    EXPECT_EQ(http.transport_suffix, "legacy+signal");
    EXPECT_EQ(file.transport_suffix, "legacy+signal");
}

TEST(BotBinaryProtocol, ConvertsTradeRequestSnapshot) {
    namespace bot = optionx::bridges::bot_binary;

    optionx::TradeRequest request;
    request.symbol = "frxEURUSD";
    request.order_type = optionx::OrderType::BUY;
    request.duration = 120;

    const auto prepared = bot::prepare_bot_binary_command(
        request,
        "3.00",
        "trade-request-idem");

    EXPECT_EQ(
        prepared.request_query_value,
        "frxEURUSD=CALL=3.00=duration=2=m=");
}

TEST(BotBinaryProtocol, ParsesRawHttpAndFileSignals) {
    namespace bot = optionx::bridges::bot_binary;

    const auto raw = bot::parse_bot_binary_request_value(
        "frxEURAUD=CALL=1.00=duration=5=m=");
    EXPECT_EQ(raw.symbol, "frxEURAUD");
    EXPECT_EQ(raw.order_type, optionx::OrderType::BUY);
    EXPECT_EQ(raw.amount_value, "1.00");
    EXPECT_EQ(raw.expiry_kind, bot::BotBinaryExpiryKind::DURATION);
    EXPECT_EQ(raw.expiry_value, 5u);
    EXPECT_EQ(raw.expiry_unit, bot::BotBinaryTimeUnit::MINUTES);
    EXPECT_TRUE(raw.transport_suffix.empty());

    const auto http = bot::parse_bot_binary_http_request(
        "http://127.0.0.2/?request=R_50%3DPUT%3D1%3Dendtime%3D1538264736%3Ds%3D&ignored=1");
    EXPECT_EQ(http.symbol, "R_50");
    EXPECT_EQ(http.order_type, optionx::OrderType::SELL);
    EXPECT_EQ(http.expiry_kind, bot::BotBinaryExpiryKind::END_TIME);
    EXPECT_EQ(http.expiry_value, 1538264736u);
    EXPECT_EQ(http.expiry_unit, bot::BotBinaryTimeUnit::SECONDS);

    const auto file = bot::parse_bot_binary_file_signal_name(
        R"(C:\Common\Files\Signal\R_25=PUT=1=duration=5=m=2018.09.29=1538190215.txt)");
    EXPECT_EQ(file.symbol, "R_25");
    EXPECT_EQ(file.order_type, optionx::OrderType::SELL);
    EXPECT_EQ(file.amount_value, "1");
    EXPECT_EQ(file.expiry_kind, bot::BotBinaryExpiryKind::DURATION);
    EXPECT_EQ(file.expiry_value, 5u);
    EXPECT_EQ(file.expiry_unit, bot::BotBinaryTimeUnit::MINUTES);
    EXPECT_EQ(file.transport_suffix, "2018.09.29=1538190215");

    const auto plus_suffix = bot::parse_bot_binary_http_request(
        "http://127.0.0.2/?request=R_25=CALL=1=duration=1=m=legacy+signal");
    EXPECT_EQ(plus_suffix.transport_suffix, "legacy+signal");
}

TEST(BotBinaryProtocol, IgnoresNonRequestQueryParameterNames) {
    namespace bot = optionx::bridges::bot_binary;

    EXPECT_THROW(
        bot::parse_bot_binary_http_request(
            "http://127.0.0.2/?xrequest=R_25%3DCALL%3D1%3Dduration%3D1%3Dm%3D"),
        std::invalid_argument);
}

TEST(BotBinaryProtocol, ConvertsParsedCommandToTradeSignal) {
    namespace bot = optionx::bridges::bot_binary;

    const auto parsed = bot::parse_bot_binary_file_signal_name(
        "R_50=CALL=0.50=duration=30=s=legacy-signal.txt");
    const auto signal = bot::bot_binary_to_trade_signal(parsed, "legacy_binarybot");

    EXPECT_EQ(signal.symbol, "R_50");
    EXPECT_EQ(signal.signal_name, "legacy_binarybot");
    EXPECT_TRUE(signal.unique_hash.empty());
    EXPECT_EQ(parsed.transport_suffix, "legacy-signal");
    EXPECT_EQ(signal.option_type, optionx::OptionType::SPRINT);
    EXPECT_EQ(signal.order_type, optionx::OrderType::BUY);
    EXPECT_DOUBLE_EQ(signal.amount, 0.50);
    EXPECT_EQ(signal.duration, 30u);
    EXPECT_EQ(signal.expiry_time, 0);
}

TEST(BotBinaryProtocol, ConvertsEndTimeCommandToClassicTradeRequest) {
    namespace bot = optionx::bridges::bot_binary;

    const auto parsed = bot::parse_bot_binary_file_signal_name(
        "R_50=PUT=1=endtime=1538264736=s=legacy-endtime.txt");
    const auto request = bot::bot_binary_to_trade_request(parsed, "legacy_binarybot");

    EXPECT_EQ(request.symbol, "R_50");
    EXPECT_EQ(request.signal_name, "legacy_binarybot");
    EXPECT_TRUE(request.unique_hash.empty());
    EXPECT_EQ(request.option_type, optionx::OptionType::CLASSIC);
    EXPECT_EQ(request.order_type, optionx::OrderType::SELL);
    EXPECT_DOUBLE_EQ(request.amount, 1.0);
    EXPECT_EQ(request.duration, 0u);
    EXPECT_EQ(request.expiry_time, 1538264736);
}

TEST(BotBinaryProtocol, RejectsInvalidCommands) {
    namespace bot = optionx::bridges::bot_binary;

    EXPECT_THROW(
        bot::prepare_bot_binary_command(
            bot::bot_binary_duration_command(
                "EUR/USD",
                optionx::OrderType::BUY,
                "1.00",
                60,
                "idem")),
        std::invalid_argument);

    EXPECT_THROW(
        bot::prepare_bot_binary_command(
            bot::bot_binary_duration_command(
                "R_25",
                optionx::OrderType::UNKNOWN,
                "1.00",
                60,
                "idem")),
        std::invalid_argument);

    EXPECT_THROW(
        bot::bot_binary_duration_command(
            "R_25",
            optionx::OrderType::BUY,
            "1.00",
            0,
            "idem"),
        std::invalid_argument);

    EXPECT_THROW(
        bot::prepare_bot_binary_command(
            bot::bot_binary_duration_command(
                "R_25",
                optionx::OrderType::BUY,
                "NaN",
                60,
                "idem")),
        std::invalid_argument);

    EXPECT_THROW(
        bot::prepare_bot_binary_command(
            bot::bot_binary_duration_command(
                "R_25",
                optionx::OrderType::BUY,
                "-1",
                60,
                "idem")),
        std::invalid_argument);

    EXPECT_THROW(
        bot::prepare_bot_binary_command(
            bot::bot_binary_duration_command(
                "R_25",
                optionx::OrderType::BUY,
                "0",
                60,
                "idem")),
        std::invalid_argument);

    EXPECT_THROW(
        bot::prepare_bot_binary_command(
            bot::bot_binary_duration_command(
                "R_25",
                optionx::OrderType::BUY,
                "1,50",
                60,
                "idem")),
        std::invalid_argument);

    EXPECT_THROW(
        bot::parse_bot_binary_request_value(
            "R_25=BUY=1=duration=1=m="),
        std::invalid_argument);

    EXPECT_THROW(
        bot::parse_bot_binary_file_signal_name(
            "R_25=CALL=1=duration=1=m=.txt"),
        std::invalid_argument);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
