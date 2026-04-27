// tradeup_ws_invalid_token_probe.cpp
//#define LOGIT_BASE_PATH "E:\\_repoz\\optionx_cpp"
#define OPTIONX_LOG_UNIQUE_FILE_INDEX 2

#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <unordered_map>
#include <fstream>
#include <sstream>

#include <kurlyk.hpp>
#include <logit_cpp/logit.hpp>

#include <optionx_cpp/utils.hpp> // utils::TaskManager

// ---------------- helpers ----------------

static std::unordered_map<std::string, std::string> read_config(const std::string& filename) {
    std::unordered_map<std::string, std::string> cfg;
    std::ifstream f(filename);
    std::string line;
    while (std::getline(f, line)) {
        std::istringstream iss(line);
        std::string k, v;
        if (std::getline(iss, k, '=') && std::getline(iss, v)) {
            cfg[k] = v;
        }
    }
    return cfg;
}

static std::string strip_http_prefix(std::string host) {
    const std::string http  = "http://";
    const std::string https = "https://";
    if (host.rfind(http, 0) == 0)  return host.substr(http.size());
    if (host.rfind(https, 0) == 0) return host.substr(https.size());
    return host;
}

static std::string make_invalid_token() {
    /*
    // псевдо-UUID из мусора; главное — заведомо невалидный
    static std::mt19937_64 rng{std::random_device{}()};
    std::uniform_int_distribution<uint64_t> dist;
    auto n = dist(rng);
    std::ostringstream oss;
    oss << "invalid-" << std::hex << n;
    return oss.str();
    */
    return "9c899e0f-c8e8-4204-9f3d-ac56910f64f49";
}

static void log_ws_submit_result(
        const char* operation,
        const kurlyk::SubmitResult& result) {
    if (!result) {
        LOGIT_PRINT_ERROR(operation, " rejected: ", result.error_code.message());
    }
}

// ---------------- main ----------------

int main(int argc, char** argv) {
    LOGIT_ADD_CONSOLE_DEFAULT();
    LOGIT_ADD_FILE_LOGGER_DEFAULT();
    LOGIT_ADD_UNIQUE_FILE_LOGGER_DEFAULT_SINGLE_MODE();
    kurlyk::init();

    optionx::utils::TaskManager tm; // для пингов и таймеров

    try {
        // Конфиг (опционально):
        // config.txt:
        // host=https://tradeup.net
        // cookie=...   (если нужно, можно оставить пустым)
        auto cfg   = read_config("config.txt");
        std::string host   = cfg.count("host")   ? cfg["host"]   : "https://tradeup.net";
        std::string cookie = cfg.count("cookie") ? cfg["cookie"] : "";
        std::string token = cfg.count("x-api-token") ? cfg["x-api-token"] : "";

        const std::string wss_host = "wss://" + strip_http_prefix(host);
        const std::string real_path = "/trade-api-ws/api/ws/user";
        const std::string demo_path = "/trade-api-ws/demo/ws/user";

        LOGIT_STREAM_INFO() << "Host: " << wss_host
                            << "\nREAL: " << real_path
                            << "\nDEMO: " << demo_path
                            << "\nToken: " << token;

        kurlyk::WebSocketClient real_ws;
        kurlyk::WebSocketClient demo_ws;

        std::atomic<bool> real_connected{false};
        std::atomic<bool> demo_connected{false};

        // epochs для пингов, чтобы не было двойных таймеров
        std::atomic<uint64_t> real_ping_epoch{0};
        std::atomic<uint64_t> demo_ping_epoch{0};

        // --- REAL handler ---
        real_ws.on_event([&token, &real_connected, &real_ping_epoch, &tm, &real_ws](std::unique_ptr<kurlyk::WebSocketEventData> e) {
            switch (e->event_type) {
                case kurlyk::WebSocketEventType::WS_OPEN: {
                    LOGIT_INFO("[REAL] OPEN: status=", e->status_code, " err=", e->error_code);
                    real_connected = true;

                    // Отправляем НЕВАЛИДНЫЙ токен
                    std::string auth = std::string("{\"x-api-token\":\"") + token + "\"}";
                    log_ws_submit_result("[REAL] send auth", e->sender->submit_message(auth, 0, [](const std::error_code& ec){
                        LOGIT_ERROR_IF(ec, "[REAL] send auth: ", ec);
                    }));

                    // Пинг каждые 30s
                    const auto epoch = ++real_ping_epoch;
                    tm.add_periodic_task("real_ping", 30000, [epoch, &real_connected, &real_ping_epoch, &real_ws](std::shared_ptr<optionx::utils::Task> task){
                        if (task->is_shutdown()) return;
                        if (epoch != real_ping_epoch) return;
                        if (!real_connected.load()) return;
                        static constexpr const char* kPing = R"({"id":"","param":"","operation":"PING"})";
                        log_ws_submit_result("[REAL] ping", real_ws.submit_message(kPing, 0, [](const std::error_code& ec){
                            LOGIT_ERROR_IF(ec, "[REAL] ping: ", ec);
                        }));
                    });
                    break;
                }
                case kurlyk::WebSocketEventType::WS_MESSAGE: {
                    LOGIT_STREAM_INFO() << "[REAL] MESSAGE: " << e->message;
                    break;
                }
                case kurlyk::WebSocketEventType::WS_ERROR: {
                    LOGIT_INFO("[REAL] ERROR: status=", e->status_code, " err=", e->error_code);
                    real_connected = false;
                    break;
                }
                case kurlyk::WebSocketEventType::WS_CLOSE: {
                    LOGIT_INFO("[REAL] CLOSE: status=", e->status_code, " err=", e->error_code);
                    real_connected = false;
                    break;
                }
                default: break;
            }
        });

        // --- DEMO handler ---
        demo_ws.on_event([&token, &demo_connected, &demo_ping_epoch, &tm, &demo_ws](std::unique_ptr<kurlyk::WebSocketEventData> e) {
            switch (e->event_type) {
                case kurlyk::WebSocketEventType::WS_OPEN: {
                    LOGIT_INFO("[DEMO] OPEN: status=", e->status_code, " err=", e->error_code);
                    demo_connected = true;

                    // Отправляем НЕВАЛИДНЫЙ токен
                    std::string auth = std::string("{\"x-api-token\":\"") + token + "\"}";
                    log_ws_submit_result("[DEMO] send auth", e->sender->submit_message(auth, 0, [](const std::error_code& ec){
                        LOGIT_ERROR_IF(ec, "[DEMO] send auth: ", ec);
                    }));

                    // Пинг каждые 40s
                    const auto epoch = ++demo_ping_epoch;
                    tm.add_periodic_task("demo_ping", 40000, [epoch, &demo_ping_epoch, &demo_connected, &demo_ws](std::shared_ptr<optionx::utils::Task> task){
                        if (task->is_shutdown()) return;
                        if (epoch != demo_ping_epoch) return;
                        if (!demo_connected.load()) return;
                        static constexpr const char* kPing = R"({"id":"","param":"","operation":"PING"})";
                        log_ws_submit_result("[DEMO] ping", demo_ws.submit_message(kPing, 0, [](const std::error_code& ec){
                            LOGIT_ERROR_IF(ec, "[DEMO] ping: ", ec);
                        }));
                    });
                    break;
                }
                case kurlyk::WebSocketEventType::WS_MESSAGE: {
                    LOGIT_STREAM_INFO() << "[DEMO] MESSAGE: " << e->message;
                    break;
                }
                case kurlyk::WebSocketEventType::WS_ERROR: {
                    LOGIT_INFO("[DEMO] ERROR: status=", e->status_code, " err=", e->error_code);
                    demo_connected = false;
                    break;
                }
                case kurlyk::WebSocketEventType::WS_CLOSE: {
                    LOGIT_INFO("[DEMO] CLOSE: status=", e->status_code, " err=", e->error_code);
                    demo_connected = false;
                    break;
                }
                default: break;
            }
        });

        // --- WS setup ---
        // Можно добавить user-agent/accept-language/encoding/proxy при необходимости
        real_ws.set_url(wss_host, real_path);
        if (!cookie.empty()) real_ws.set_cookie(cookie);
        real_ws.set_reconnect(true);
        real_ws.set_request_timeout(20);
        real_ws.set_user_agent("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/133.0.0.0 Safari/537.36");
        real_ws.set_accept_language("ru,ru-RU;q=0.9,en;q=0.8,en-US;q=0.7");
        real_ws.set_accept_encoding(true, true, true, true);

        demo_ws.set_url(wss_host, demo_path);
        if (!cookie.empty()) demo_ws.set_cookie(cookie);
        demo_ws.set_reconnect(true);
        demo_ws.set_request_timeout(20);
        demo_ws.set_user_agent("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/133.0.0.0 Safari/537.36");
        demo_ws.set_accept_language("ru,ru-RU;q=0.9,en;q=0.8,en-US;q=0.7");
        demo_ws.set_accept_encoding(true, true, true, true);
        
        // --- connect both ---
        LOGIT_INFO("Connecting REAL & DEMO...");
        real_ws.connect();
        demo_ws.connect();

        LOGIT_INFO("Press <Enter> to stop (or wait to observe responses)...");
        
        std::atomic<bool> stop{false};
        std::thread input_thread([&stop]{
            std::string line;
            std::getline(std::cin, line); // ждём Enter
            stop.store(true, std::memory_order_release);
        });
        
        while (!stop.load(std::memory_order_acquire)) {
            tm.process();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
		tm.shutdown();
		tm.process();
        input_thread.join();

        LOGIT_INFO("Disconnecting...");
		
        real_ws.disconnect();
        demo_ws.disconnect();

        // дадим сокетам время корректно закрыться
        std::this_thread::sleep_for(std::chrono::seconds(1));

    } catch (const std::exception& ex) {
        LOGIT_ERROR("Exception: ", ex.what());
    }

    kurlyk::deinit();
    LOGIT_WAIT();
    return 0;
}
