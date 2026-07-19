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
        "http://127.0.0.2/?request=frxEURAUD=CALL=1.00=duration=5=m=");
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
        prepared.file_name,
        "R_25=PUT=1=duration=30=s=custom_suffix.txt");
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
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
