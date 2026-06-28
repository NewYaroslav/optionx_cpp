#include <optionx_cpp/platforms.hpp>

int optionx_header_odr_companion() {
    const auto platform = optionx::to_str(optionx::PlatformType::INTRADE_BAR);
    const auto host = optionx::utils::remove_http_prefix("https://intrade.bar");
    const auto encoded = optionx::utils::Base36::encode_string("Az");

    return platform == "INTRADE_BAR" &&
           host == "intrade.bar" &&
           optionx::utils::Base36::decode_string(encoded) == "Az";
}
