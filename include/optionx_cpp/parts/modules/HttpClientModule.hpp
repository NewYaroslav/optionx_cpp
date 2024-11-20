#pragma once
#ifndef _OPTIONX_MODULES_HTTP_CLIENT_MODULE_HPP_INCLUDED
#define _OPTIONX_MODULES_HTTP_CLIENT_MODULE_HPP_INCLUDED

/// \file HttpClientModule.hpp
/// \brief Provides the HttpClientModule class for handling HTTP requests and processing trade events.

#include "../pubsub/EventMediator.hpp"
#include "../interfaces/IAccountInfoData.hpp"
#include "events/AuthDataEvent.hpp"
#include "events/AccountInfoRequestEvent.hpp"
#include "events/AccountInfoUpdateEvent.hpp"
#include "events/BalanceRequestEvent.hpp"
#include "events/BalanceResponseEvent.hpp"
#include "events/TradeRequestEvent.hpp"
#include "events/TradeStatusEvent.hpp"
#include "events/ConnectRequestEvent.hpp"
#include "events/ConnectionStatusEvent.hpp"
#include "events/PriceUpdateEvent.hpp"
#include <kurlyk.hpp>
#include <log-it/LogIt.hpp>
#include <array>

#ifndef OPTIONX_TIMESTAMP_MS
#define OPTIONX_TIMESTAMP_MS time_shield::timestamp_ms()
#endif

namespace optionx {
namespace modules {

    /// \class HttpClientModule
    /// \brief Handles HTTP requests and processes trade request events.
    class HttpClientModule : public EventMediator {
    public:
        /// \enum RateLimitType
        /// \brief Defines the types of rate limits for HTTP requests.
        enum class RateLimitType : size_t {
            GENERAL,                ///< General limit for all requests.
            TRADE_EXECUTION,        ///< Limit for trade opening requests.
            TRADE_RESULT,           ///< Limit for trade result requests.
            BALANCE,                ///< Limit for balance requests.
            ACCOUNT_INFO,           ///< Limit for account information requests.
            ACCOUNT_SETTINGS,       ///< Limit for account type or currency change requests.
            TICK_DATA,              ///< Limit for tick data requests.
            COUNT                   ///< Total number of rate limit types.
        };

        /// \brief Constructor initializing the HTTP client module with an event hub.
        /// \param hub Reference to the event hub for event handling.
        /// \param account_info Shared pointer to account information data.
        explicit HttpClientModule(EventHub& hub, std::shared_ptr<IAccountInfoData> account_info)
            : EventMediator(hub), m_account_info(std::move(account_info)) {
            subscribe<AccountInfoRequestEvent>(this);
            subscribe<BalanceRequestEvent>(this);
            subscribe<TradeRequestEvent>(this);
            subscribe<TradeStatusEvent>(this);
            subscribe<AuthDataEvent>(this);
            subscribe<ConnectRequestEvent>(this);
        }

        /// \brief Default virtual destructor.
        virtual ~HttpClientModule() {
            deinitialize_rate_limits();
            m_client.cancel_requests();
        }

        void setup() {
            initialize_rate_limits();
            initialize_client();
        }

        /// \brief Handles an event notification received as a shared pointer.
        /// \param event The event received, passed as a shared pointer.
        void on_event(const std::shared_ptr<Event>& event) override {
            if (auto msg = std::dynamic_pointer_cast<AccountInfoRequestEvent>(event)) {
                handle_event(*msg);
            } else if (auto msg = std::dynamic_pointer_cast<BalanceRequestEvent>(event)) {
                handle_event(*msg);
            } else if (auto msg = std::dynamic_pointer_cast<TradeRequestEvent>(event)) {
                handle_event(*msg);
            } else if (auto msg = std::dynamic_pointer_cast<TradeStatusEvent>(event)) {
                handle_event(*msg);
            } else if (auto msg = std::dynamic_pointer_cast<AuthDataEvent>(event)) {
                handle_event(*msg);
            } else if (auto msg = std::dynamic_pointer_cast<ConnectRequestEvent>(event)) {
                handle_event(*msg);
            }
        }

        /// \brief Handles an event notification received as a raw pointer.
        /// \param event The event received, passed as a raw pointer.
        void on_event(const Event* const event) override {
            if (const auto* msg = dynamic_cast<const AccountInfoRequestEvent*>(event)) {
                handle_event(*msg);
            } else if (const auto* msg = dynamic_cast<const BalanceRequestEvent*>(event)) {
                handle_event(*msg);
            } else if (const auto* msg = dynamic_cast<const TradeRequestEvent*>(event)) {
                handle_event(*msg);
            } else if (const auto* msg = dynamic_cast<const TradeStatusEvent*>(event)) {
                handle_event(*msg);
            } else if (const auto* msg = dynamic_cast<const AuthDataEvent*>(event)) {
                handle_event(*msg);
            } else if (const auto* msg = dynamic_cast<const ConnectRequestEvent*>(event)) {
                handle_event(*msg);
            }
        }

        /// \brief Processes queued requests (to be implemented by derived classes).
        void process() {
            process_http_responses();
            process_additional_logic();
        }

        void shutdown() {
            m_client.cancel_requests();
        }

    protected:

        /// \brief Additional logic to be implemented in derived classes.
        virtual void process_additional_logic() = 0;

        /// \brief Handles the account information request event.
        /// \param event The account information request event.
        virtual void handle_event(const AccountInfoRequestEvent& event) = 0;

        /// \brief Handles the balance request event.
        /// \param event The balance request event.
        virtual void handle_event(const BalanceRequestEvent& event) = 0;

        /// \brief Handles the trade request event.
        /// \param event The trade request event.
        virtual void handle_event(const TradeRequestEvent& event) = 0;

        /// \brief Handles the trade status update event.
        /// \param event The trade status update event.
        virtual void handle_event(const TradeStatusEvent& event) = 0;

        /// \brief Handles the authorization data event.
        /// \param event The authorization data event.
        virtual void handle_event(const AuthDataEvent& event) = 0;

        /// \brief Handles the connection request event.
        /// \param event The connection request event, including a callback to process the connection result.
        virtual void handle_event(const ConnectRequestEvent& event) = 0;

        /// \brief Initializes rate limits.
        virtual void initialize_rate_limits() = 0;

        /// \brief Initializes HTTP client.
        virtual void initialize_client() = 0;

        /// \brief Gets the rate limit for the specified type.
        /// \param type The type of rate limit to retrieve.
        /// \return The rate limit value for the specified type.
        long get_rate_limit(RateLimitType type) const {
            return m_rate_limits[static_cast<size_t>(type)];
        }

        /// \brief Deinitializes all rate limits by resetting them.
        void deinitialize_rate_limits() {
            for (size_t i = 0; i < m_rate_limits.size(); ++i) {
                kurlyk::remove_limit(m_rate_limits[i]);
            }
        }

        /// \brief Adds a new HTTP request task to the list.
        /// \param future The future object representing the pending HTTP response.
        /// \param callback The callback function to handle the response.
        void add_http_request_task(std::future<kurlyk::HttpResponsePtr> future,
                                   std::function<void(kurlyk::HttpResponsePtr)> callback) {
            m_http_tasks.push_back({std::move(future), std::move(callback)});
        }

        /// \brief Creates a rate limit based on Requests Per Minute (RPM).
        /// \param
        /// \param requests_per_minute Maximum number of requests allowed per minute.
        void set_rate_limit_rpm(RateLimitType type, long requests_per_minute) {
            m_rate_limits[static_cast<size_t>(type)] = kurlyk::create_rate_limit_rpm(requests_per_minute);
        }

        /// \brief Creates a rate limit based on Requests Per Second (RPS).
        /// \param
        /// \param requests_per_second Maximum number of requests allowed per second.
        void set_rate_limit_rps(RateLimitType type, long requests_per_second) {
            m_rate_limits[static_cast<size_t>(type)] = kurlyk::create_rate_limit_rps(requests_per_second);
        }

        kurlyk::HttpClient                  m_client;        ///< The HTTP client for making requests.
        std::shared_ptr<IAccountInfoData>   m_account_info;  ///< Shared pointer to account information data.

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

        std::list<HttpRequestTask>          m_http_tasks;      ///< List of pending HTTP request tasks.
        std::array<long, static_cast<size_t>(RateLimitType::COUNT)> m_rate_limits = {}; ///< Array to store rate limits.

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
    }; // HttpClientModule

} // namespace modules
} // namespace optionx

#endif // _OPTIONX_MODULES_HTTP_CLIENT_MODULE_HPP_INCLUDED
