#define OPTIONX_LOG_UNIQUE_FILE_INDEX 2

#include <gtest/gtest.h>

#include "IntradeBarSmokeSupport.hpp"

#include <algorithm>

namespace smoke = optionx::tests::intrade_bar_smoke;

namespace {

void assert_successful_connect(const smoke::ConnectAttempt& attempt) {
    ASSERT_TRUE(attempt.callback_received)
        << "Timed out waiting for Intrade Bar auth callback.";
    ASSERT_TRUE(attempt.success) << attempt.reason;
}

} // namespace

#define OPTIONX_SKIP_WITHOUT_CREDENTIALS(config)                                      \
    do {                                                                             \
        if (!(config).has_credentials()) {                                            \
            GTEST_SKIP() << "Set OPTIONX_INTRADE_BAR_EMAIL and "                     \
                         << "OPTIONX_INTRADE_BAR_PASSWORD to run live smoke tests."; \
        }                                                                            \
    } while (0)

#define OPTIONX_ASSERT_PROXY_PRESENT(config)                                      \
    ASSERT_TRUE((config).has_proxy())                                             \
        << "Refusing to run Intrade Bar live smoke without proxy settings."

TEST(IntradeBarApiSmoke, RefusesCredentialsWithoutProxy) {
    const auto config = smoke::load_config();
    if (!config.has_credentials()) {
        GTEST_SKIP() << "Set OPTIONX_INTRADE_BAR_EMAIL and OPTIONX_INTRADE_BAR_PASSWORD to run live smoke tests.";
    }
    OPTIONX_ASSERT_PROXY_PRESENT(config);
}

TEST(IntradeBarApiSmoke, SuccessfulAuthorizationWithProxy) {
    const auto config = smoke::load_config();
    OPTIONX_SKIP_WITHOUT_CREDENTIALS(config);
    OPTIONX_ASSERT_PROXY_PRESENT(config);

    smoke::IntradeBarSmokeRuntime runtime(config);
    const auto connect = runtime.connect();
    assert_successful_connect(connect);
    EXPECT_TRUE(runtime.platform().is_connected());

    const auto disconnect = runtime.disconnect();
    EXPECT_TRUE(disconnect.callback_received);
    EXPECT_TRUE(disconnect.success) << disconnect.reason;
}

TEST(IntradeBarApiSmoke, SessionCacheMakesSecondAuthorizationFaster) {
    const auto config = smoke::load_config();
    OPTIONX_SKIP_WITHOUT_CREDENTIALS(config);
    OPTIONX_ASSERT_PROXY_PRESENT(config);

    ASSERT_TRUE(smoke::remove_saved_session(config));
    EXPECT_FALSE(smoke::saved_session(config).has_value());

    smoke::ConnectAttempt first;
    {
        smoke::IntradeBarSmokeRuntime runtime(config);
        first = runtime.connect();
        assert_successful_connect(first);
        runtime.disconnect();
    }

    ASSERT_TRUE(smoke::saved_session(config).has_value())
        << "Fresh auth should store a reusable broker session.";

    smoke::ConnectAttempt second;
    {
        smoke::IntradeBarSmokeRuntime runtime(config);
        second = runtime.connect();
        assert_successful_connect(second);
        runtime.disconnect();
    }

    LOGIT_INFO(
        "Intrade Bar auth timings: fresh_ms=",
        first.elapsed_ms,
        ", cached_ms=",
        second.elapsed_ms);

    EXPECT_LT(second.elapsed_ms, first.elapsed_ms)
        << "Expected cached-session auth to be faster than fresh login.";
}

TEST(IntradeBarApiSmoke, AccountInfoAndQuotesAvailableAfterAuth) {
    const auto config = smoke::load_config();
    OPTIONX_SKIP_WITHOUT_CREDENTIALS(config);
    OPTIONX_ASSERT_PROXY_PRESENT(config);

    smoke::IntradeBarSmokeRuntime runtime(config);
    const auto connect = runtime.connect();
    assert_successful_connect(connect);

    EXPECT_TRUE(runtime.platform().is_connected());
    EXPECT_EQ(
        runtime.platform().get_info<optionx::AccountType>(optionx::AccountInfoType::ACCOUNT_TYPE),
        config.account_type);
    EXPECT_EQ(
        runtime.platform().get_info<optionx::CurrencyType>(optionx::AccountInfoType::CURRENCY),
        config.currency);
    EXPECT_GE(runtime.platform().get_info<double>(optionx::AccountInfoType::BALANCE), 0.0);

    runtime.request_balance_refresh();
    const std::string quote_symbol = smoke::normalize_quote_symbol(config.quote_symbol);
    EXPECT_TRUE(runtime.wait_for_price_update(quote_symbol, config.price_timeout_ms));

    const auto ticks = runtime.latest_ticks();
    EXPECT_FALSE(ticks.empty());
    EXPECT_TRUE(std::any_of(ticks.begin(), ticks.end(), [&](const optionx::SingleTick& tick) {
        return tick.symbol == quote_symbol;
    })) << "Expected quote symbol in live price snapshot: " << quote_symbol;

    runtime.disconnect();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    LOGIT_ADD_MEMORY_LOGGER_DEFAULT_SINGLE_MODE();
    LOGIT_ADD_CONSOLE_DEFAULT();
    LOGIT_ADD_FILE_LOGGER_DEFAULT();
    LOGIT_ADD_UNIQUE_FILE_LOGGER_DEFAULT_SINGLE_MODE();
    kurlyk::init();
    const int result = RUN_ALL_TESTS();
    kurlyk::deinit();
    LOGIT_WAIT();
    return result;
}

#undef OPTIONX_SKIP_WITHOUT_CREDENTIALS
#undef OPTIONX_ASSERT_PROXY_PRESENT
