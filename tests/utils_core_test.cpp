#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <vector>

#include <optionx_cpp/utils.hpp>

TEST(StringUtilsTest, ParseListIgnoresEmptyInput) {
    std::vector<std::string> items{"existing"};

    optionx::utils::parse_list("", items);

    ASSERT_EQ(items.size(), 1u);
    EXPECT_EQ(items[0], "existing");
}

TEST(StringUtilsTest, ParseListSkipsEmptyItems) {
    std::vector<std::string> items;

    optionx::utils::parse_list("one,,two,", items);

    ASSERT_EQ(items.size(), 2u);
    EXPECT_EQ(items[0], "one");
    EXPECT_EQ(items[1], "two");
}

TEST(HttpUtilsTest, RemoveHttpPrefixOnlyRemovesLeadingScheme) {
    EXPECT_EQ(
        optionx::utils::remove_http_prefix("https://intrade.bar"),
        "intrade.bar");
    EXPECT_EQ(
        optionx::utils::remove_http_prefix("http://intrade.bar"),
        "intrade.bar");
    EXPECT_EQ(
        optionx::utils::remove_http_prefix("wss://intrade.bar"),
        "wss://intrade.bar");
    EXPECT_EQ(
        optionx::utils::remove_http_prefix("prefixhttps://intrade.bar"),
        "prefixhttps://intrade.bar");
}

TEST(FixedPointUtilsTest, PrecisionToleranceUsesFullDecimalStep) {
    EXPECT_DOUBLE_EQ(optionx::utils::precision_tolerance(0), 1.0);
    EXPECT_DOUBLE_EQ(optionx::utils::precision_tolerance(2), 0.01);
    EXPECT_TRUE(optionx::utils::compare_with_precision(1.00, 1.01, 2));
    EXPECT_FALSE(optionx::utils::compare_with_precision(1.00, 1.02, 2));
    EXPECT_FALSE(optionx::utils::compare_with_precision(1e14, 1e14 + 0.25, 2));
    EXPECT_THROW(
        optionx::utils::precision_tolerance(19),
        std::invalid_argument);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
