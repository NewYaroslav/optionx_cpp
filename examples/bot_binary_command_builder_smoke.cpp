#include <optionx_cpp/bridges.hpp>

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

bool has_arg(int argc, char** argv, const std::string& value) {
    for (int i = 1; i < argc; ++i) {
        if (argv[i] == value) {
            return true;
        }
    }
    return false;
}

std::string option_value(int argc, char** argv, const std::string& name) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (argv[i] == name) {
            return argv[i + 1];
        }
    }
    return {};
}

void print_usage() {
    std::cout
        << "Usage: bot_binary_command_builder_smoke [--self-test] [--base-url url]\n"
        << "Prints BotBinary/BinaryBot HTTP request and file-signal command values.\n";
}

} // namespace

int main(int argc, char** argv) {
    if (has_arg(argc, argv, "--help")) {
        print_usage();
        return 0;
    }

    namespace bot = optionx::bridges::bot_binary;

    bot::BotBinaryAdapterConfig config;
    const auto base_url = option_value(argc, argv, "--base-url");
    if (!base_url.empty()) {
        config.http_base_url = base_url;
    }

    try {
        optionx::TradeRequest request;
        request.symbol = "frxEURAUD";
        request.order_type = optionx::OrderType::BUY;
        request.duration = 5 * 60;

        const auto prepared = bot::prepare_bot_binary_command(
            request,
            "1.00",
            "bot-binary-smoke-idempotency",
            config);

        std::cout << "BotBinary request value: "
                  << prepared.request_query_value << '\n';
        std::cout << "BotBinary HTTP URL: "
                  << prepared.http_url << '\n';
        std::cout << "BotBinary file signal name: "
                  << prepared.file_name << '\n';
        std::cout << "Stable transport suffix: "
                  << prepared.transport_suffix << '\n';

        if (has_arg(argc, argv, "--self-test")) {
            const auto parsed_request =
                bot::parse_bot_binary_request_value(prepared.request_query_value);
            const auto parsed_file =
                bot::parse_bot_binary_file_signal_name(prepared.file_name);
            const auto signal =
                bot::bot_binary_to_trade_signal(parsed_file, "bot_binary_smoke");

            const bool ok =
                prepared.request_query_value ==
                    "frxEURAUD=CALL=1.00=duration=5=m=" &&
                prepared.http_url.find(
                    "?request=frxEURAUD%3DCALL%3D1.00%3Dduration%3D5%3Dm%3D") !=
                        std::string::npos &&
                prepared.file_name.rfind(
                    "frxEURAUD=CALL=1.00=duration=5=m=ox_",
                    0) == 0u &&
                prepared.file_name.size() > 4 &&
                prepared.file_name.substr(prepared.file_name.size() - 4) == ".txt" &&
                parsed_request.symbol == "frxEURAUD" &&
                parsed_request.order_type == optionx::OrderType::BUY &&
                parsed_request.transport_suffix.empty() &&
                parsed_file.transport_suffix == prepared.transport_suffix &&
                signal.symbol == "frxEURAUD" &&
                signal.signal_name == "bot_binary_smoke" &&
                signal.duration == 5u * 60u;
            if (!ok) {
                std::cerr << "Self-test did not produce expected BotBinary fields.\n";
                return 3;
            }
        }
    } catch (const std::exception& ex) {
        std::cerr << "BotBinary command builder failed: " << ex.what() << '\n';
        return 1;
    }

    return 0;
}
