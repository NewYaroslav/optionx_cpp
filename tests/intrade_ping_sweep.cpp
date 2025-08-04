#include <kurlyk.hpp>
#include <iostream>
#include <chrono>
#include <memory>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>
#include <optional>

int main() {
    kurlyk::init(true);

    kurlyk::HttpClient client;

    client.set_head_only(true);
    client.set_timeout(5);
    client.set_connect_timeout(5);
    client.set_retry_attempts(3, 1000);
    
    struct State {
        int total_requests = 0;
        std::atomic<int> completed_requests{0};
        std::vector<int> successful_indices;
        std::mutex mutex;
        std::function<void(std::optional<std::string>)> on_complete;
    };
    
    auto state = std::make_shared<State>();
    state->total_requests = 1000;
    state->on_complete = [](std::optional<std::string> result) {
        if (result) {
            std::cout << "\n=== LOWEST WORKING DOMAIN FOUND: " << *result << " ===\n";
        } else {
            std::cout << "\nNo working domain found.\n";
        }
    };
    
    // Простой цикл по всем доменам
    for (int i = 0; i <= 1000; ++i) {
        std::string host;
        if (i == 0) {
            host = "https://intrade.bar";
        } else {
            host = "https://intrade" + std::to_string(i) + ".bar";
        }
        client.set_host(host);

        client.get("/", {}, {}, [state, i, host](const kurlyk::HttpResponsePtr& response) {
            if (!response->ready) return;

            //std::cout << host
            //          << " | status: " << response->status_code
            //          << ", error: curl:" << response->error_code << '\n';

            if (response->status_code == 200) {
                std::lock_guard<std::mutex> lock(state->mutex);
                state->successful_indices.push_back(i);
            }
            
            int finished = ++state->completed_requests;
            if (finished == state->total_requests) {
                std::cout << "finished" << std::endl;
                std::optional<std::string> result;
                {
                    std::lock_guard<std::mutex> lock(state->mutex);
                    if (!state->successful_indices.empty()) {
                        int min_index = *std::min_element(
                            state->successful_indices.begin(), state->successful_indices.end());
                        if (min_index == 0) {
                            result = "https://intrade.bar";
                        } else {
                            result = "https://intrade" + std::to_string(min_index) + ".bar";
                        }
                    }
                }
                state->on_complete(result);
            }
        });
    }
  
    while (state->completed_requests != state->total_requests) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(5000));

    kurlyk::deinit();
    return 0;
}
