#define SQLITE_THREADSAFE 1
#define LOGIT_BASE_PATH "E:\\_repoz\\optionx_cpp"
#define OPTIONX_LOG_UNIQUE_FILE_INDEX 2
#include <iostream>
#include "optionx_cpp/parts/utils.hpp"
#include "optionx_cpp/parts/storages/ServiceSessionDB.hpp"

int main() {
    LOGIT_ADD_CONSOLE_DEFAULT();
    LOGIT_ADD_FILE_LOGGER_DEFAULT();
    LOGIT_ADD_UNIQUE_FILE_LOGGER_DEFAULT_SINGLE_MODE();

    try {
        // Get the singleton instance of ServiceSessionDB
        auto& session_db = optionx::storage::ServiceSessionDB::get_instance();

        // Set an encryption key
        std::array<uint8_t, 32> encryption_key = {
            0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
            0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
            0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
            0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20
        };
        session_db.set_key(encryption_key);

        // Platform and email
        std::string platform = "example_platform";
        std::string email = "user@example.com";

        // Set a session value
        std::string session_value = "this_is_a_test_session_value";
        if (session_db.set_session_value(platform, email, session_value)) {
            std::cout << "Session value set successfully. Value: " << session_value << std::endl;
        } else {
            std::cerr << "Failed to set session value." << std::endl;
        }

        // Retrieve the session value
        auto retrieved_value = session_db.get_session_value(platform, email);
        if (retrieved_value) {
            std::cout << "Retrieved session value: " << *retrieved_value << std::endl;
        } else {
            std::cerr << "Session value not found." << std::endl;
        }

        // Remove the session value
        if (session_db.remove_session(platform, email)) {
            std::cout << "Session value removed successfully." << std::endl;
        } else {
            std::cerr << "Failed to remove session value." << std::endl;
        }

        // Clear the database
        if (session_db.clear()) {
            std::cout << "All session data cleared successfully." << std::endl;
        } else {
            std::cerr << "Failed to clear session data." << std::endl;
        }

    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
    }

    LOGIT_WAIT();
    return 0;
}
