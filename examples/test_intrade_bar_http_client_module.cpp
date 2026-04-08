#define LOGIT_BASE_PATH "E:\\_repoz\\optionx_cpp"
#define OPTIONX_LOG_UNIQUE_FILE_INDEX 2
#define SQLITE_THREADSAFE 1
#include "optionx_cpp/parts/platforms/intrade_bar/PlatformAPI/HttpClientModule.hpp"
#include "optionx_cpp/parts/platforms/intrade_bar/AuthData.hpp"

int main() {
    LOGIT_ADD_CONSOLE_DEFAULT();
    LOGIT_ADD_FILE_LOGGER_DEFAULT();
    LOGIT_ADD_UNIQUE_FILE_LOGGER_DEFAULT_SINGLE_MODE();

    kurlyk::init();

    try {
        optionx::EventHub hub;
        auto account_info = std::make_shared<optionx::platforms::intrade_bar::AccountInfoData>();
        optionx::platforms::intrade_bar::HttpClientModule client(hub, account_info);

        auto auth_data = std::make_shared<optionx::platforms::intrade_bar::AuthData>();
        auth_data->set_email_password("bbotytch@yandex.ru", "SsgKizxk0AFgM");
        auth_data->account_type = optionx::AccountType::DEMO;
        auth_data->currency = optionx::CurrencyType::USD;

        optionx::modules::AuthDataEvent auth_event(auth_data);
        LOGIT_TRACE("notify AuthDataEvent");
        hub.notify(auth_event);

        optionx::modules::ConnectRequestEvent connect_event(
            [](bool success, const std::string& reason, std::unique_ptr<optionx::IAuthData> auth_data) {
                if (success) {
                    KURLYK_PRINT << "Authentication successful!" << std::endl;
                } else {
                    KURLYK_PRINT << "Authentication failed: " << reason << std::endl;
                }
            });

        LOGIT_TRACE("notify ConnectRequestEvent");
        hub.notify(connect_event);

        LOGIT_TRACE("start process");
        while (true) {
            client.process();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

    } catch (const std::exception& ex) {
        KURLYK_PRINT << "Exception: " << ex.what() << std::endl;
    }

    kurlyk::deinit();
    LOGIT_WAIT();
    return 0;
}
