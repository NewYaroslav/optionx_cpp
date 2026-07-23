#pragma once
#ifndef OPTIONX_HEADER_BRIDGES_BRIDGE_HOST_HPP_INCLUDED
#define OPTIONX_HEADER_BRIDGES_BRIDGE_HOST_HPP_INCLUDED

/// \file BridgeHost.hpp
/// \brief Defines an application-side lifecycle helper for bridge instances.

#include "BaseBridge.hpp"

#include <functional>
#include <optional>
#include <utility>

namespace optionx::bridges {

    class BridgeHost;

    /// \struct BridgeHostHooks
    /// \brief Application hooks around bridge lifecycle calls.
    struct BridgeHostHooks {
        using hook_t = std::function<void(BridgeHost&)>;

        hook_t before_run; ///< Called before forwarding to `BaseBridge::run()`.
        hook_t after_run; ///< Called after `BaseBridge::run()` returns.
        hook_t before_shutdown; ///< Called before forwarding to `BaseBridge::shutdown()`.
        hook_t after_shutdown; ///< Called after `BaseBridge::shutdown()` returns.
        hook_t before_reset; ///< Called before `BridgeHost::reset()` starts shutdown.
        hook_t after_reset; ///< Called after `BridgeHost::reset()` completes shutdown.
    };

    /// \class BridgeHost
    /// \brief Coordinates application-level bridge preparation and teardown.
    ///
    /// `BridgeHost` does not own the bridge and does not replace the bridge's
    /// own thread-safety or transport lifecycle. It is a small host-side
    /// adapter for application tasks such as publishing the initial account
    /// snapshot before `run()`, wiring shutdown hooks, and resetting a bridge
    /// runtime through the same shutdown path used by normal teardown.
    class BridgeHost final {
    public:
        using account_info_provider_t =
            std::function<std::optional<AccountInfoUpdate>()>;

        /// \brief Creates a host wrapper for an existing bridge instance.
        explicit BridgeHost(BaseBridge& bridge) noexcept
            : m_bridge(&bridge) {}

        /// \brief Creates a host wrapper with initial lifecycle hooks.
        BridgeHost(BaseBridge& bridge, BridgeHostHooks hooks)
            : m_bridge(&bridge),
              m_hooks(std::move(hooks)) {}

        /// \brief Returns the wrapped bridge.
        BaseBridge& bridge() noexcept {
            return *m_bridge;
        }

        /// \brief Returns the wrapped bridge.
        const BaseBridge& bridge() const noexcept {
            return *m_bridge;
        }

        /// \brief Returns mutable lifecycle hooks.
        BridgeHostHooks& hooks() noexcept {
            return m_hooks;
        }

        /// \brief Returns immutable lifecycle hooks.
        const BridgeHostHooks& hooks() const noexcept {
            return m_hooks;
        }

        /// \brief Replaces all lifecycle hooks.
        void set_hooks(BridgeHostHooks hooks) {
            m_hooks = std::move(hooks);
        }

        /// \brief Returns the current account snapshot provider.
        account_info_provider_t& account_info_provider() noexcept {
            return m_account_info_provider;
        }

        /// \brief Returns the current account snapshot provider.
        const account_info_provider_t& account_info_provider() const noexcept {
            return m_account_info_provider;
        }

        /// \brief Sets an optional provider for the latest account snapshot.
        void set_account_info_provider(account_info_provider_t provider) {
            m_account_info_provider = std::move(provider);
        }

        /// \brief Publishes the latest account snapshot when a provider is set.
        /// \return True when a provider returned an update and it was published.
        bool refresh_account_info() {
            if (!m_account_info_provider) {
                return false;
            }

            auto update = m_account_info_provider();
            if (!update) {
                return false;
            }

            bridge().update_account_info(*update);
            return true;
        }

        /// \brief Runs `before_run`, then the bridge, then `after_run`.
        void run() {
            invoke(m_hooks.before_run);
            bridge().run();
            m_run_requested = true;
            invoke(m_hooks.after_run);
        }

        /// \brief Runs `before_shutdown`, then the bridge, then `after_shutdown`.
        void shutdown() {
            invoke(m_hooks.before_shutdown);
            bridge().shutdown();
            m_run_requested = false;
            invoke(m_hooks.after_shutdown);
        }

        /// \brief Executes a host-level reset through the normal shutdown path.
        ///
        /// This does not reconfigure the bridge. It gives applications one
        /// place to drain or clear their own state before and after the bridge
        /// transport is stopped.
        void reset() {
            invoke(m_hooks.before_reset);
            shutdown();
            invoke(m_hooks.after_reset);
        }

        /// \brief Returns whether this host has requested the bridge to run.
        bool run_requested() const noexcept {
            return m_run_requested;
        }

    private:
        void invoke(const BridgeHostHooks::hook_t& hook) {
            if (hook) {
                hook(*this);
            }
        }

        BaseBridge* m_bridge = nullptr;
        BridgeHostHooks m_hooks;
        account_info_provider_t m_account_info_provider;
        bool m_run_requested = false;
    };

} // namespace optionx::bridges

#endif // OPTIONX_HEADER_BRIDGES_BRIDGE_HOST_HPP_INCLUDED
