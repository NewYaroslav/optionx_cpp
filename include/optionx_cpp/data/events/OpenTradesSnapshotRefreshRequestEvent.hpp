#pragma once
#ifndef _OPTIONX_MODULES_OPEN_TRADES_SNAPSHOT_REFRESH_REQUEST_EVENT_HPP_INCLUDED
#define _OPTIONX_MODULES_OPEN_TRADES_SNAPSHOT_REFRESH_REQUEST_EVENT_HPP_INCLUDED

/// \file OpenTradesSnapshotRefreshRequestEvent.hpp
/// \brief Defines an event requesting a broker active-trades snapshot refresh.

namespace optionx::events {

    /// \class OpenTradesSnapshotRefreshRequestEvent
    /// \brief Requests a broker active-trades snapshot refresh from the platform-specific sync manager.
    class OpenTradesSnapshotRefreshRequestEvent : public utils::Event {
    public:
        std::string reason; ///< Human-readable reason for diagnostics.

        /// \brief Constructor initializing the refresh reason.
        /// \param reason Human-readable refresh reason.
        explicit OpenTradesSnapshotRefreshRequestEvent(std::string reason)
            : reason(std::move(reason)) {}

        /// \brief Default virtual destructor.
        virtual ~OpenTradesSnapshotRefreshRequestEvent() = default;

        std::type_index type() const override {
            return typeid(OpenTradesSnapshotRefreshRequestEvent);
        }

        const char* name() const override {
            return "OpenTradesSnapshotRefreshRequestEvent";
        }
    };

} // namespace optionx::events

#endif // _OPTIONX_MODULES_OPEN_TRADES_SNAPSHOT_REFRESH_REQUEST_EVENT_HPP_INCLUDED
