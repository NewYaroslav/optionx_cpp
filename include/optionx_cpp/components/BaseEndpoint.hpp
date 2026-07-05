#pragma once
#ifndef OPTIONX_HEADER_COMPONENTS_BASE_ENDPOINT_HPP_INCLUDED
#define OPTIONX_HEADER_COMPONENTS_BASE_ENDPOINT_HPP_INCLUDED

/// \file BaseEndpoint.hpp
/// \brief Declares the common runtime contract for public service endpoints.

namespace optionx {

    /// \class BaseEndpoint
    /// \brief Common facade contract for configurable services with lifecycle and connection state.
    class BaseEndpoint {
    public:
        /// \brief Virtual destructor for polymorphic endpoint implementations.
        virtual ~BaseEndpoint() noexcept = default;

        /// \brief Configures the endpoint with a typed configuration DTO.
        /// \param config Endpoint configuration object.
        /// \return True if the configuration was accepted; false otherwise.
        virtual bool configure(std::unique_ptr<IEndpointConfig> config) {
            (void)config;
            return false;
        }

        /// \brief Initiates a connection for the configured endpoint.
        /// \param callback Callback receiving the connection result.
        virtual void connect(connection_callback_t callback) {
            if (callback) {
                callback(ConnectionResult(false, "Endpoint does not support connect()."));
            }
        }

        /// \brief Disconnects the endpoint.
        /// \param callback Callback receiving the disconnection result.
        virtual void disconnect(connection_callback_t callback) {
            if (callback) {
                callback(ConnectionResult(false, "Endpoint does not support disconnect()."));
            }
        }

        /// \brief Checks whether the endpoint is ready for its configured work.
        /// \return True if the endpoint considers itself connected.
        virtual bool is_connected() const {
            return false;
        }

        /// \brief Starts the endpoint lifecycle.
        /// \param start_worker_thread Whether the endpoint should launch its own worker thread.
        virtual void run(bool start_worker_thread = true) {
            (void)start_worker_thread;
        }

        /// \brief Processes one lifecycle tick for externally driven endpoints.
        virtual void process() {}

        /// \brief Stops endpoint tasks and releases runtime resources.
        virtual void shutdown() noexcept {}
    };

} // namespace optionx

#endif // OPTIONX_HEADER_COMPONENTS_BASE_ENDPOINT_HPP_INCLUDED
