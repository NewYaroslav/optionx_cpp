#include <gtest/gtest.h>

#include <optionx_cpp/utils/metatrader_paths.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <utility>

namespace {

class ScopedPathCleanup {
public:
    explicit ScopedPathCleanup(std::filesystem::path cleanup_path)
        : path(std::move(cleanup_path)) {}

    ~ScopedPathCleanup() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }

    std::filesystem::path path;
};

std::filesystem::path make_temp_root() {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() /
           ("optionx_metatrader_paths_test_" + std::to_string(stamp));
}

} // namespace

TEST(MetaTraderPaths, BuildsMetaQuotesRootsFromRoamingRoot) {
    namespace mt = optionx::utils::metatrader;

    const auto roaming = std::filesystem::u8path("C:/Users/User/AppData/Roaming");
    const auto terminal_root = mt::metaquotes_terminal_root_from_roaming(roaming);
    EXPECT_EQ(
        terminal_root.generic_u8string(),
        "C:/Users/User/AppData/Roaming/MetaQuotes/Terminal");

    const auto common_files = mt::common_files_root_from_terminal_root(terminal_root);
    EXPECT_EQ(
        common_files.generic_u8string(),
        "C:/Users/User/AppData/Roaming/MetaQuotes/Terminal/Common/Files");

    EXPECT_TRUE(mt::metaquotes_terminal_root_from_roaming({}).empty());
    EXPECT_TRUE(mt::common_files_root_from_terminal_root({}).empty());
}

TEST(MetaTraderPaths, DiscoversTerminalDataDirectoriesByMqlFolders) {
    namespace mt = optionx::utils::metatrader;

    const auto root = make_temp_root();
    ScopedPathCleanup cleanup(root);
    const auto terminal_root = root / "MetaQuotes" / "Terminal";

    std::filesystem::create_directories(terminal_root / "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA" / "MQL4" / "Files");
    std::filesystem::create_directories(terminal_root / "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB" / "MQL5" / "Files");
    std::filesystem::create_directories(terminal_root / "CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC" / "MQL4" / "Files");
    std::filesystem::create_directories(terminal_root / "CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC" / "MQL5" / "Files");
    std::filesystem::create_directories(terminal_root / "Common" / "Files");
    std::filesystem::create_directories(terminal_root / "not-a-terminal");

    const auto discovered = mt::discover_terminal_data_directories(terminal_root);
    ASSERT_EQ(discovered.size(), 3u);

    EXPECT_EQ(discovered[0].kind(), mt::MetaTraderTerminalKind::MT4);
    EXPECT_TRUE(discovered[0].has_mql4);
    EXPECT_FALSE(discovered[0].has_mql5);
    const auto first_files = mt::existing_mql_files_directories(discovered[0]);
    ASSERT_EQ(first_files.size(), 1u);
    EXPECT_EQ(first_files[0].filename().u8string(), "Files");
    EXPECT_EQ(first_files[0].parent_path().filename().u8string(), "MQL4");

    EXPECT_EQ(discovered[1].kind(), mt::MetaTraderTerminalKind::MT5);
    EXPECT_FALSE(discovered[1].has_mql4);
    EXPECT_TRUE(discovered[1].has_mql5);

    EXPECT_EQ(discovered[2].kind(), mt::MetaTraderTerminalKind::MIXED);
    EXPECT_TRUE(discovered[2].has_mql4);
    EXPECT_TRUE(discovered[2].has_mql5);
    const auto mixed_files = mt::existing_mql_files_directories(discovered[2]);
    ASSERT_EQ(mixed_files.size(), 2u);
    EXPECT_EQ(mixed_files[0].parent_path().filename().u8string(), "MQL4");
    EXPECT_EQ(mixed_files[1].parent_path().filename().u8string(), "MQL5");

    std::error_code missing_ec;
    EXPECT_TRUE(mt::discover_terminal_data_directories(root / "missing", missing_ec).empty());
    EXPECT_FALSE(missing_ec);
}

TEST(MetaTraderPaths, InspectsSingleTerminalDirectory) {
    namespace mt = optionx::utils::metatrader;

    const auto root = make_temp_root();
    ScopedPathCleanup cleanup(root);
    const auto terminal = root / "terminal";

    EXPECT_FALSE(mt::looks_like_terminal_data_directory(terminal));

    std::filesystem::create_directories(terminal / "MQL5");
    const auto inspected = mt::inspect_terminal_data_directory(terminal);
    EXPECT_EQ(inspected.data_root, terminal);
    EXPECT_EQ(inspected.kind(), mt::MetaTraderTerminalKind::MT5);
    EXPECT_TRUE(mt::looks_like_terminal_data_directory(terminal));
    EXPECT_EQ(inspected.mql5_files_dir(), terminal / "MQL5" / "Files");
}

TEST(MetaTraderPaths, ReportsDiscoveryErrors) {
    namespace mt = optionx::utils::metatrader;

    const auto root = make_temp_root();
    ScopedPathCleanup cleanup(root);
    std::filesystem::create_directories(root);
    const auto not_directory = root / "not-a-directory";
    {
        std::ofstream out(not_directory, std::ios::binary);
        out << "not a directory";
    }

    const auto result = mt::discover_terminal_data_directories_result(not_directory);
    EXPECT_TRUE(result.terminals.empty());
    EXPECT_FALSE(result.complete);
    EXPECT_TRUE(static_cast<bool>(result.error));
    EXPECT_EQ(result.error_path, not_directory);

    std::error_code ec;
    EXPECT_TRUE(mt::discover_terminal_data_directories(not_directory, ec).empty());
    EXPECT_TRUE(static_cast<bool>(ec));
}

TEST(MetaTraderPaths, RejectsSymlinkTerminalAndContinuesDiscovery) {
    namespace mt = optionx::utils::metatrader;

    const auto root = make_temp_root();
    ScopedPathCleanup cleanup(root);
    const auto terminal_root = root / "MetaQuotes" / "Terminal";
    const auto outside = root / "outside-terminal";
    const auto symlink_terminal =
        terminal_root / "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
    const auto valid_terminal =
        terminal_root / "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB";

    std::filesystem::create_directories(outside / "MQL5" / "Files");
    std::filesystem::create_directories(valid_terminal / "MQL4" / "Files");

    std::error_code link_ec;
    std::filesystem::create_directory_symlink(outside, symlink_terminal, link_ec);
    if (link_ec) {
        GTEST_SKIP() << "Directory symlinks are not available: "
                     << link_ec.message();
    }

    const auto result = mt::discover_terminal_data_directories_result(terminal_root);
    EXPECT_FALSE(result.complete);
    EXPECT_TRUE(static_cast<bool>(result.error));
    EXPECT_EQ(result.error_path, symlink_terminal);
    ASSERT_EQ(result.terminals.size(), 1u);
    EXPECT_EQ(result.terminals[0].data_root, valid_terminal);
    EXPECT_EQ(result.terminals[0].kind(), mt::MetaTraderTerminalKind::MT4);
}

TEST(MetaTraderPaths, RejectsSymlinkTerminalRoot) {
    namespace mt = optionx::utils::metatrader;

    const auto root = make_temp_root();
    ScopedPathCleanup cleanup(root);
    const auto metaquotes = root / "MetaQuotes";
    const auto terminal_root = metaquotes / "Terminal";
    const auto outside_root = root / "outside-terminal-root";

    std::filesystem::create_directories(metaquotes);
    std::filesystem::create_directories(
        outside_root / "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA" / "MQL5" / "Files");

    std::error_code link_ec;
    std::filesystem::create_directory_symlink(
        outside_root,
        terminal_root,
        link_ec);
    if (link_ec) {
        GTEST_SKIP() << "Directory symlinks are not available: "
                     << link_ec.message();
    }

    const auto result = mt::discover_terminal_data_directories_result(terminal_root);
    EXPECT_TRUE(result.terminals.empty());
    EXPECT_FALSE(result.complete);
    EXPECT_TRUE(static_cast<bool>(result.error));
    EXPECT_EQ(result.error_path, terminal_root);
}

TEST(MetaTraderPaths, RejectsSymlinkMqlParentDirectory) {
    namespace mt = optionx::utils::metatrader;

    const auto root = make_temp_root();
    ScopedPathCleanup cleanup(root);
    const auto terminal = root / "terminal";
    const auto outside_mql = root / "outside-mql";

    std::filesystem::create_directories(terminal);
    std::filesystem::create_directories(outside_mql / "Files");

    std::error_code link_ec;
    std::filesystem::create_directory_symlink(
        outside_mql,
        terminal / "MQL4",
        link_ec);
    if (link_ec) {
        GTEST_SKIP() << "Directory symlinks are not available: "
                     << link_ec.message();
    }

    mt::MetaTraderTerminalDirectory inspected;
    inspected.data_root = terminal;
    inspected.has_mql4 = true;

    const auto result = mt::existing_mql_files_directories_result(inspected);
    EXPECT_TRUE(result.files_directories.empty());
    EXPECT_FALSE(result.complete);
    EXPECT_TRUE(static_cast<bool>(result.error));
    EXPECT_EQ(result.error_path, terminal / "MQL4");
}

TEST(MetaTraderPaths, ReportsMqlFilesDirectoryErrors) {
    namespace mt = optionx::utils::metatrader;

    const auto root = make_temp_root();
    ScopedPathCleanup cleanup(root);
    const auto terminal = root / "terminal";
    const auto outside_files = root / "outside-files";

    std::filesystem::create_directories(terminal / "MQL4");
    std::filesystem::create_directories(outside_files);

    std::error_code link_ec;
    std::filesystem::create_directory_symlink(
        outside_files,
        terminal / "MQL4" / "Files",
        link_ec);
    if (link_ec) {
        GTEST_SKIP() << "Directory symlinks are not available: "
                     << link_ec.message();
    }

    const auto inspected = mt::inspect_terminal_data_directory(terminal);
    ASSERT_TRUE(inspected.has_mql4);

    const auto result = mt::existing_mql_files_directories_result(inspected);
    EXPECT_TRUE(result.files_directories.empty());
    EXPECT_FALSE(result.complete);
    EXPECT_TRUE(static_cast<bool>(result.error));
    EXPECT_EQ(result.error_path, terminal / "MQL4" / "Files");

    std::error_code ec;
    EXPECT_TRUE(mt::existing_mql_files_directories(inspected, ec).empty());
    EXPECT_TRUE(static_cast<bool>(ec));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
