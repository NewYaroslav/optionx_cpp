#include <gtest/gtest.h>

#include <optionx_cpp/storages.hpp>

namespace {

struct DummyRecord {
    int value = 0;
};

} // namespace

TEST(StorageResultTest, WriteResultOkFollowsSuccessStatus) {
    optionx::storage::StorageWriteResult<DummyRecord> result;
    EXPECT_FALSE(result.ok());
    EXPECT_FALSE(static_cast<bool>(result));

    result.status = optionx::storage::StorageStatus::SUCCESS;

    EXPECT_TRUE(result.ok());
    EXPECT_TRUE(static_cast<bool>(result));
}

TEST(StorageResultTest, ReadResultRequiresSuccessAndFound) {
    optionx::storage::StorageReadResult<DummyRecord> result;
    result.status = optionx::storage::StorageStatus::SUCCESS;

    EXPECT_FALSE(result.ok());
    EXPECT_FALSE(static_cast<bool>(result));

    result.found = true;

    EXPECT_TRUE(result.ok());
    EXPECT_TRUE(static_cast<bool>(result));
}

TEST(StorageResultTest, TradeRecordDbResultAliasesUseCommonStorageStatus) {
    optionx::storage::TradeRecordDBWriteResult result;
    result.status = optionx::storage::TradeRecordDBStatus::SUCCESS;

    EXPECT_TRUE(result.ok());
    EXPECT_TRUE(static_cast<bool>(result));
    EXPECT_EQ(result.status, optionx::storage::StorageStatus::SUCCESS);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
