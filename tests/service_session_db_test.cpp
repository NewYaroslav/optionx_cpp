#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <string>

#include <optionx_cpp/storages.hpp>

TEST(ServiceSessionDBTest, StoresReadsRemovesAndClearsEncryptedSessionValues) {
    auto& session_db = optionx::storage::ServiceSessionDB::get_instance();
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

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
