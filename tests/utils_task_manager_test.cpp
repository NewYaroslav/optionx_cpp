#include <gtest/gtest.h>

#include <optionx_cpp/utils.hpp>

TEST(LogRedactionTest, RedactsNonEmptySecrets) {
    EXPECT_TRUE(optionx::utils::redact_secret_value("").empty());
    EXPECT_EQ(optionx::utils::redact_secret_value("session=abc"), "***");
    EXPECT_EQ(optionx::utils::redact_secret("legacy-token"), "***");
}

TEST(LogRedactionTest, RedactsSecretFieldsInsideText) {
    using optionx::utils::redact_secrets_in_text;

    EXPECT_EQ(
        redact_secrets_in_text("status=ok token=abc123 user_hash=deadbeef"),
        "status=ok token=*** user_hash=***");
    EXPECT_EQ(
        redact_secrets_in_text("password=secret&symbol=EURUSD"),
        "password=***&symbol=EURUSD");
    EXPECT_EQ(
        redact_secrets_in_text("Cookie: user_id=42; user_hash=deadbeef"),
        "Cookie: ***");
    EXPECT_EQ(
        redact_secrets_in_text("plain diagnostic text"),
        "plain diagnostic text");
}

TEST(TaskManagerTest, RejectsInvalidPeriodicPeriods) {
    optionx::utils::TaskManager manager;
    auto callback = [](std::shared_ptr<optionx::utils::Task>) {};

    EXPECT_FALSE(manager.add_periodic_task(0, callback));
    EXPECT_FALSE(manager.add_periodic_task(-1, callback));
    EXPECT_FALSE(manager.add_delayed_periodic_task(1, 0, callback));
    EXPECT_FALSE(manager.add_periodic_on_date_task(1, 0, callback));
    EXPECT_FALSE(manager.add_periodic_task("bad-periodic", 0, callback));
    EXPECT_FALSE(manager.add_delayed_periodic_task("bad-delayed-periodic", 1, 0, callback));
    EXPECT_FALSE(manager.add_periodic_on_date_task("bad-periodic-on-date", 1, 0, callback));
}

TEST(TaskTest, KeepsPreviousPeriodWhenSetPeriodIsInvalid) {
    int calls = 0;
    auto task = std::make_shared<optionx::utils::Task>(
        optionx::utils::TaskType::PERIODIC,
        [&calls](std::shared_ptr<optionx::utils::Task>) {
            ++calls;
        },
        0,
        10);

    EXPECT_FALSE(task->set_period(0));
    EXPECT_TRUE(task->set_period(1));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
