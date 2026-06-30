#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>

#include <optionx_cpp/storages.hpp>
#include <optionx_cpp/utils.hpp>

namespace {

std::string unique_db_path(const std::string& name) {
    static std::atomic<std::uint64_t> counter{0};
    const auto stamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    return "data/" + name + "_" + std::to_string(stamp) + "_" +
           std::to_string(counter.fetch_add(1));
}

mdbxc::Config make_config(const std::string& name) {
    mdbxc::Config config;
    config.pathname = unique_db_path(name);
    config.max_dbs = 1;
    config.no_subdir = false;
    config.relative_to_exe = true;
    return config;
}

} // namespace

TEST(CryptoRandomTest, GeneratesKeysWithExpectedModeLengths) {
    optionx::crypto::AESCrypt aes_256(optionx::crypto::AesMode::CBC_256);
    EXPECT_EQ(aes_256.generate_key().size(), 32u);

    optionx::crypto::AESCrypt aes_192(optionx::crypto::AesMode::CBC_192);
    EXPECT_EQ(aes_192.generate_key().size(), 24u);

    optionx::crypto::AESCrypt aes_128(optionx::crypto::AesMode::CBC_128);
    EXPECT_EQ(aes_128.generate_key().size(), 16u);
}

TEST(ServiceSessionDBTest, StoresReadsRemovesAndClearsEncryptedSessionValues) {
    optionx::storage::ServiceSessionDB session_db(
        make_config("service_session_db_test"));
    ASSERT_TRUE(session_db.is_open());

    const std::array<std::uint8_t, 32> encryption_key = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
        0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
        0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20
    };
    ASSERT_TRUE(session_db.set_key(encryption_key));

    const std::string platform = "service-session-db-test";
    const std::string email = "user@example.test";
    const std::string value = "session-value";

    ASSERT_TRUE(session_db.clear());
    EXPECT_FALSE(session_db.get_session_value(platform, email).has_value());

    ASSERT_TRUE(session_db.set_session_value(platform, email, value));
    const auto stored_value = session_db.get_session_value(platform, email);
    ASSERT_TRUE(stored_value.has_value());
    EXPECT_EQ(*stored_value, value);

    EXPECT_TRUE(session_db.remove_session(platform, email));
    EXPECT_FALSE(session_db.get_session_value(platform, email).has_value());

    ASSERT_TRUE(session_db.set_session_value(platform, email, value));
    EXPECT_TRUE(session_db.clear());
    EXPECT_FALSE(session_db.get_session_value(platform, email).has_value());
}

TEST(ServiceSessionDBTest, ReportsDefaultAndCustomKeyState) {
    optionx::storage::ServiceSessionDB session_db(
        make_config("service_session_db_key_state_test"));
    ASSERT_TRUE(session_db.is_open());

    EXPECT_TRUE(session_db.uses_default_key());
    EXPECT_FALSE(session_db.has_custom_key());

    const std::array<std::uint8_t, 16> invalid_key = {
        0x01, 0x02, 0x03, 0x04,
        0x05, 0x06, 0x07, 0x08,
        0x09, 0x0A, 0x0B, 0x0C,
        0x0D, 0x0E, 0x0F, 0x10
    };
    EXPECT_FALSE(session_db.set_key(invalid_key));
    EXPECT_TRUE(session_db.uses_default_key());
    EXPECT_FALSE(session_db.has_custom_key());

    const std::array<std::uint8_t, 32> custom_key = {
        0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
        0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30,
        0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
        0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40
    };
    ASSERT_TRUE(session_db.set_key(custom_key));
    EXPECT_FALSE(session_db.uses_default_key());
    EXPECT_TRUE(session_db.has_custom_key());

    session_db.shutdown();
    EXPECT_FALSE(session_db.uses_default_key());
    EXPECT_FALSE(session_db.has_custom_key());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
