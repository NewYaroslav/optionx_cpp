#define LOGIT_BASE_PATH "E:\\_repoz\\optionx_cpp"
#define OPTIONX_LOG_UNIQUE_FILE_INDEX 2
#define SQLITE_THREADSAFE 1

#include <iostream>
//#include <thread>
//#include <chrono>
//#include <atomic>
#include "optionx_cpp/optionx.hpp"

using namespace optionx;
using namespace optionx::platforms;
using namespace optionx::platforms::intrade_bar;

std::atomic<bool> trade_completed{false};

void trade_callback(std::unique_ptr<TradeRequest> request, std::unique_ptr<TradeResult> result) {
    LOGIT_STREAM_TRACE() << "Trade Result Received:\n"
                         << "Symbol: " << request->symbol << "\n"
                         << "Amount: " << request->amount << "\n"
                         << "State: " << to_str(result->trade_state) << "\n"
                         << "Profit: " << result->profit << "\n"
                         << "Payout: " << result->payout << "\n"
                         << "Balance: " << result->balance << "\n";

    if (result->trade_state == TradeState::WIN || result->trade_state == TradeState::LOSS) {
        trade_completed = true;
    }
}

void test_trade(IntradeBarPlatform& platform) {
    LOGIT_INFO("Starting Trade Test");

    // Создание запроса на торговлю
    auto trade_request = std::make_unique<TradeRequest>();
    trade_request->symbol = "BTCUSDT";
    trade_request->amount = 10.0;
    trade_request->option_type = OptionType::SPRINT;
    trade_request->order_type = OrderType::BUY;
    trade_request->duration = 300;  // 5 минут
    trade_request->add_callback(trade_callback);

    // Размещение сделки
    if (!platform.place_trade(std::move(trade_request))) {
        LOGIT_ERROR("Failed to place trade.");
        return;
    }

    LOGIT_INFO("Trade placed, waiting for completion...");

    // Ожидание завершения сделки
    /*
    while (!trade_completed) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    */
}

void test_trade_2(IntradeBarPlatform& platform) {
    LOGIT_INFO("Starting Trade Test");

    // Создание запроса на торговлю
    auto trade_request = std::make_unique<TradeRequest>();
    trade_request->symbol = "EURUSD";
    trade_request->amount = 1.0;
    trade_request->option_type = OptionType::SPRINT;
    trade_request->order_type = OrderType::BUY;
    trade_request->duration = 180;  // 3 минут
    trade_request->add_callback(trade_callback);

    // Размещение сделки
    if (!platform.place_trade(std::move(trade_request))) {
        LOGIT_ERROR("Failed to place trade.");
        return;
    }

    LOGIT_INFO("Trade placed, waiting for completion...");

    // Ожидание завершения сделки
    /*
    while (!trade_completed) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    */
}

void test_trade_3(IntradeBarPlatform& platform) {
    LOGIT_INFO("Starting Trade Test");

    // Создание запроса на торговлю
    auto trade_request = std::make_unique<TradeRequest>();
    trade_request->symbol = "USDJPY";
    trade_request->amount = 80.0;
    trade_request->option_type = OptionType::SPRINT;
    trade_request->order_type = OrderType::BUY;
    trade_request->duration = 180;  // 3 минут
    trade_request->add_callback(trade_callback);

    // Размещение сделки
    if (!platform.place_trade(std::move(trade_request))) {
        LOGIT_ERROR("Failed to place trade.");
        return;
    }

    LOGIT_INFO("Trade placed, waiting for completion...");

    // Ожидание завершения сделки
    /*
    while (!trade_completed) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    */
}

std::unordered_map<std::string, std::string> read_config(const std::string& filename) {
    std::unordered_map<std::string, std::string> config;
    std::ifstream file(filename);
    std::string line;

    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string key, value;
        if (std::getline(iss, key, '=') && std::getline(iss, value)) {
            config[key] = value;
        }
    }
    return config;
}

int main() {
    LOGIT_ADD_CONSOLE_DEFAULT();
    LOGIT_ADD_FILE_LOGGER_DEFAULT();
    LOGIT_ADD_UNIQUE_FILE_LOGGER_DEFAULT_SINGLE_MODE();
    kurlyk::init();

    try {
        // Создание объекта платформы
        IntradeBarPlatform platform;

        // Установка данных авторизации
        auto auth_data = std::make_unique<AuthData>();
        const std::string config_filename = "config.txt";
        auto config = read_config(config_filename);
        std::string email = config["email"];
        std::string password = config["password"];
        auth_data->set_email_password(email, password);
        auth_data->account_type = AccountType::DEMO;
        auth_data->currency = CurrencyType::USD;

        platform.on_account_info() = [](const AccountInfoUpdate& info) {
            LOGIT_STREAM_INFO()
                << "status: " << info.status
                << "; balance: "
                << info.account_info->get_info<double>(AccountInfoType::BALANCE) << " "
                << info.account_info->get_info<std::string>(AccountInfoType::CURRENCY) << "; "
                << info.account_info->get_info<std::string>(AccountInfoType::ACCOUNT_TYPE) << "; trades: "
                << info.account_info->get_info<int>(AccountInfoType::OPEN_TRADES);
        };

        platform.run();

        // Подключение
        LOGIT_INFO("Connecting to platform...");
        platform.configure_auth(std::move(auth_data));

        platform.connect([](const ConnectionResult& result) {
            if (result.success) {
                LOGIT_INFO("Connected successfully.");
            } else {
                LOGIT_ERROR("Connection failed: ", result.reason);
            }
        });

        // Ожидание подключения
        //std::this_thread::sleep_for(std::chrono::seconds(5));
        std::system("pause");

        // Запуск теста сделки
        test_trade(platform);
        std::system("pause");

        test_trade_2(platform);
        std::system("pause");

        test_trade_3(platform);
        std::system("pause");

        // Деинициализация платформы
        LOGIT_INFO("Shutting down platform...");
        platform.shutdown();
        LOGIT_INFO("Shutting platform ok");

        std::this_thread::sleep_for(std::chrono::seconds(3));

        // Повторное создание объекта платформы
        LOGIT_INFO("Re-initializing platform...");
        IntradeBarPlatform platform2;

        platform2.on_account_info() = [](const AccountInfoUpdate& info) {
            LOGIT_STREAM_INFO()
                << "status: " << info.status
                << "; balance: "
                << info.account_info->get_info<double>(AccountInfoType::BALANCE) << " "
                << info.account_info->get_info<std::string>(AccountInfoType::CURRENCY) << "; "
                << info.account_info->get_info<std::string>(AccountInfoType::ACCOUNT_TYPE) << "; trades: "
                << info.account_info->get_info<int>(AccountInfoType::OPEN_TRADES);
        };

        platform2.run();

        // Установка данных авторизации
        auto auth_data2 = std::make_unique<AuthData>();
        auth_data2->set_email_password(email, password);
        auth_data2->account_type = AccountType::DEMO;
        auth_data2->currency = CurrencyType::RUB;

        // Повторное подключение
        LOGIT_INFO("Reconnecting to platform...");
        platform2.configure_auth(std::move(auth_data2));

        platform2.connect([](const ConnectionResult& result) {
            if (result.success) {
                LOGIT_INFO("Reconnected successfully.");
            } else {
                LOGIT_PRINT_ERROR("Reconnection failed: ", result.reason);
            }
        });

        // Ожидание подключения
        std::system("pause");

        // Повторный запуск теста сделки
        test_trade(platform2);
        std::system("pause");

        // Отключение
        LOGIT_INFO("Disconnecting...");
        platform2.disconnect([](const ConnectionResult& result) {
            if (result.success) {
                LOGIT_INFO("Disconnected successfully.");
            } else {
                LOGIT_PRINT_ERROR("Disconnection failed: ", result.reason);
            }
        });

        std::system("pause");

        // Повторное подключение
        LOGIT_INFO("Reconnecting again...");
        platform2.connect([](const ConnectionResult& result) {
            if (result.success) {
                LOGIT_INFO("Reconnected successfully.");
            } else {
                LOGIT_PRINT_ERROR("Reconnection failed: ", result.reason);
            }
        });

        LOGIT_INFO("Deinit?");
        std::system("pause");

    } catch (const std::exception& ex) {
        LOGIT_ERROR("Exception: ", ex.what());
    }

    kurlyk::deinit();
    LOGIT_WAIT();
    return 0;
}
