#include <gtest/gtest.h>

#include <optionx_cpp/bridges/metatrader_file.hpp>

TEST(MetaTraderFileConfigIncludeTest, UmbrellaHeaderExposesConfig) {
    optionx::bridges::metatrader_file::MetaTraderFileBridgeConfig config;

    config.common_files_root = ".";
    config.bridge_id = 1;

    EXPECT_EQ(config.bridge_type(), optionx::BridgeType::METATRADER_FILE_TRANSPORT);
    EXPECT_TRUE(config.validate().first);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
