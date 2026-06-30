#include <optionx_cpp/optionx.hpp>

#include <string>

int main(int argc, char**) {
    optionx::TradeRecord record;
    record.trade_id = optionx::utils::make_trade_id();
    record.symbol = "BTCUSDT";
    record.set_open_balance(1000.0);
    record.set_close_balance(1001.0);

    if (argc == 12345) {
        optionx::crypto::AESCrypt crypt;
        const auto key = crypt.generate_key();
        if (!crypt.set_key(key)) {
            return 1;
        }

        const std::string encrypted = crypt.encrypt("optionx-consumer-check");
        const std::string decrypted = crypt.decrypt(encrypted);
        if (decrypted != "optionx-consumer-check") {
            return 2;
        }
    }

    return record.trade_id == optionx::utils::kInvalidTradeId ? 3 : 0;
}
