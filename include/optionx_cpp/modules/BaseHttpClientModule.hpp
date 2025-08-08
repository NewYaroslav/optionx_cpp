#pragma once
#ifndef _OPTIONX_MODULES_BASE_HTTP_CLIENT_MODULE_HPP_INCLUDED
#define _OPTIONX_MODULES_BASE_HTTP_CLIENT_MODULE_HPP_INCLUDED

/// \file BaseHttpClientModule.hpp
/// \brief Provides the BaseHttpClientModule class for handling HTTP requests and processing trade events.

#ifndef OPTIONX_TIMESTAMP_MS
#define OPTIONX_TIMESTAMP_MS time_shield::timestamp_ms()
#endif

namespace optionx::modules {

    /// \class BaseHttpClientModule
    /// \brief Handles HTTP requests and processes trade request events.
    class BaseHttpClientModule : public modules::BaseModule {
    public:

        /// \brief Constructor initializing the HTTP client module with an event bus.
        /// \param bus Reference to the event bus for event handling.
        /// \param account_info Shared pointer to account information data.
        explicit BaseHttpClientModule(utils::EventBus& bus)
            : BaseModule(bus) {
        }

        /// \brief Default virtual destructor.
        virtual ~BaseHttpClientModule() {
            deinitialize_rate_limits();
            m_client.cancel_requests();
        }

        /// \brief Handles an event notification received as a raw pointer.
        /// \param event The event received, passed as a raw pointer.
        void on_event(const utils::Event* const event) override {}

        /// \brief Processes queued requests (to be implemented by derived classes).
        void process() override final {
            process_http_responses();
        }

        void shutdown() override final {
            m_client.cancel_requests();
        }

        kurlyk::HttpClient& get_http_client() {
            return m_client;
        }

        /// \brief Gets the rate limit for the specified type.
        /// \param rate_limit_id The type of rate limit to retrieve.
        /// \return The rate limit value for the specified type.
        template<class T>
        uint32_t get_rate_limit(T rate_limit_id) const {
            auto it = m_rate_limits.find(static_cast<uint32_t>(rate_limit_id));
            if (it == m_rate_limits.end()) return 0;
            return it->second;
        }

        /// \brief Adds a new HTTP request task to the list.
        /// \param future The future object representing the pending HTTP response.
        /// \param callback The callback function to handle the response.
        void add_http_request_task(
                std::future<kurlyk::HttpResponsePtr> future,
                std::function<void(kurlyk::HttpResponsePtr)> callback) {
            m_http_tasks.push_back({std::move(future), std::move(callback)});
        }

    protected:
        kurlyk::HttpClient m_client; ///< The HTTP client for making requests.
        std::unordered_map<uint32_t, uint32_t> m_rate_limits; ///< Rate limit handles by ID.

        /// \class HttpRequestTask
        /// \brief Represents a single HTTP request task with a future and a callback.
        struct HttpRequestTask {
            std::future<kurlyk::HttpResponsePtr>            future;     ///< Future holding the HTTP response.
            std::function<void(kurlyk::HttpResponsePtr)>    callback;   ///< Callback for processing the response.

            /// \brief Checks if the response is ready.
            /// \return True if the future is ready, false otherwise.
            bool ready() const {
                return future.valid() && future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
            }
        };

        std::list<HttpRequestTask> m_http_tasks;      ///< List of pending HTTP request tasks.

        /// \brief Deinitializes all rate limits by resetting them.
        void deinitialize_rate_limits() {
            for (auto item : m_rate_limits) {
                kurlyk::remove_limit(item.first);
            }
        }

        /// \brief Creates a rate limit based on Requests Per Minute (RPM).
        /// \param
        /// \param requests_per_minute Maximum number of requests allowed per minute.
        template<class T>
        void set_rate_limit_rpm(T rate_limit_id, uint32_t requests_per_minute) {
            m_rate_limits[static_cast<uint32_t>(rate_limit_id)] = kurlyk::create_rate_limit_rpm(requests_per_minute);
        }

        /// \brief Creates a rate limit based on Requests Per Second (RPS).
        /// \param
        /// \param requests_per_second Maximum number of requests allowed per second.
        template<class T>
        void set_rate_limit_rps(T rate_limit_id, uint32_t requests_per_second) {
            m_rate_limits[static_cast<uint32_t>(rate_limit_id)] = kurlyk::create_rate_limit_rps(requests_per_second);
        }

    private:

        /// \brief Processes all pending HTTP responses.
        void process_http_responses() {
            auto it = m_http_tasks.begin();
            while (it != m_http_tasks.end()) {
                if (it->ready()) {
                    try {
                        // Retrieve the response and call the callback.
                        auto response = it->future.get();
                        if (it->callback) it->callback(std::move(response));
                    } catch (const std::exception& ex) {
                        LOGIT_ERROR(ex);
                        try {
                            auto response = std::make_unique<kurlyk::HttpResponse>();
                            response->ready = true;
                            if (it->callback) it->callback(std::move(response));
                        } catch(...) {
                            LOGIT_ERROR("Unknown error");
                        }
                    }

                    it = m_http_tasks.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }; // BaseHttpClientModule

}

#endif // _OPTIONX_MODULES_BASE_HTTP_CLIENT_MODULE_HPP_INCLUDED
