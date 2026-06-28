#include <gtest/gtest.h>

#include <optionx_cpp/platforms.hpp>

int optionx_header_odr_companion();

TEST(HeaderOnlyOdrTest, LinksMultipleTranslationUnitsIncludingPlatformUmbrella) {
    EXPECT_EQ(optionx_header_odr_companion(), 1);
}

TEST(HeaderOnlyOdrTest, InvalidEnumValuesUseUnknownFallback) {
    EXPECT_EQ(
        optionx::to_str(static_cast<optionx::PlatformType>(999)),
        std::string("UNKNOWN"));
    EXPECT_EQ(
        optionx::to_str(static_cast<optionx::TradeState>(999)),
        std::string("UNKNOWN"));
    EXPECT_EQ(
        optionx::to_str(static_cast<optionx::AccountUpdateStatus>(999)),
        std::string("UNKNOWN"));
}

TEST(HeaderOnlyOdrTest, Base36StringEncodingRoundTripsSingleDigitBytes) {
    std::string input;
    input.push_back(static_cast<char>(0));
    input.push_back(static_cast<char>(1));
    input.push_back(static_cast<char>(35));
    input.push_back(static_cast<char>(36));
    input.push_back(static_cast<char>(255));
    const std::string encoded = optionx::utils::Base36::encode_string(input);

    EXPECT_EQ(encoded.size(), input.size() * 2);
    EXPECT_EQ(optionx::utils::Base36::decode_string(encoded), input);
}

TEST(HeaderOnlyOdrTest, Base36StringDecodingRejectsInvalidChunks) {
    EXPECT_THROW(
        optionx::utils::Base36::decode_string("0"),
        std::invalid_argument);
    EXPECT_THROW(
        optionx::utils::Base36::decode_string("??"),
        std::invalid_argument);
    EXPECT_THROW(
        optionx::utils::Base36::decode_string("zz"),
        std::invalid_argument);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
