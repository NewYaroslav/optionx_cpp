#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <optionx_cpp/utils.hpp>

namespace {

std::string unique_path_component(const std::string& prefix) {
    static std::atomic<std::uint64_t> counter{0};
    const auto stamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    return prefix + "_" + std::to_string(stamp) + "_" +
           std::to_string(counter.fetch_add(1));
}

struct ScopedPathCleanup {
    explicit ScopedPathCleanup(std::filesystem::path cleanup_path)
        : path(std::move(cleanup_path)) {}

    ~ScopedPathCleanup() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }

    std::filesystem::path path;
};

} // namespace

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

TEST(UnicodeCaseUtilsTest, FoldsUnicodeTextForCaselessMatching) {
    EXPECT_TRUE(optionx::utils::has_unicode_case_folding());
    EXPECT_EQ(optionx::utils::unicode_case_fold(u8"Stra\u00DFe"), "strasse");
    EXPECT_TRUE(optionx::utils::unicode_iequals(u8"\uFB03", "FFI"));
    EXPECT_TRUE(optionx::utils::unicode_case_contains(
        u8"BTCUSD Crossing \u0411\u0410\u0419 64,143.35",
        u8"\u0431\u0430\u0439"));
}

TEST(PathUtilsTest, CreatesUtf8DirectoriesFromUtf8Strings) {
    const std::string unicode_name =
        "\xD1\x82\xD0\xB5\xD1\x81\xD1\x82_\xE8\xB7\xAF\xE5\xBE\x84";
    const auto root = std::filesystem::temp_directory_path() /
        unique_path_component("optionx_path_utils");
    const ScopedPathCleanup root_cleanup(root);
    const auto target = root / std::filesystem::u8path(unicode_name) / "nested";
    const auto target_utf8 = target.u8string();

    ASSERT_NO_THROW(optionx::utils::create_directories(target_utf8));
    EXPECT_TRUE(std::filesystem::is_directory(target));

    const auto relative = optionx::utils::make_relative(
        target_utf8,
        root.u8string());
    EXPECT_EQ(
        std::filesystem::u8path(relative).filename().u8string(),
        "nested");

    const auto exec_relative_root = unique_path_component("optionx_exec_path");
    const auto exec_relative_path = exec_relative_root + "/" + unicode_name + "/nested";
    const auto resolved = optionx::utils::resolve_exec_path(exec_relative_path);
    const auto resolved_path = std::filesystem::u8path(resolved);
    const ScopedPathCleanup exec_cleanup(resolved_path.parent_path().parent_path());

    EXPECT_TRUE(resolved_path.is_absolute());
    EXPECT_EQ(resolved_path.parent_path().filename().u8string(), unicode_name);
    ASSERT_NO_THROW(optionx::utils::create_directories(resolved));
    EXPECT_TRUE(std::filesystem::is_directory(resolved_path));
}

TEST(FixedPointUtilsTest, PrecisionToleranceUsesHalfDecimalStep) {
    EXPECT_DOUBLE_EQ(optionx::utils::precision_tolerance(0), 0.5);
    EXPECT_DOUBLE_EQ(optionx::utils::precision_tolerance(2), 0.005);
    EXPECT_TRUE(optionx::utils::compare_with_precision(1.00, 1.004, 2));
    EXPECT_FALSE(optionx::utils::compare_with_precision(1.00, 1.01, 2));
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
