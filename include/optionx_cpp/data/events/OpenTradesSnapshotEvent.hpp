#pragma once
#ifndef _OPTIONX_EVENTS_OPEN_TRADES_SNAPSHOT_EVENT_HPP_INCLUDED
#define _OPTIONX_EVENTS_OPEN_TRADES_SNAPSHOT_EVENT_HPP_INCLUDED

/// \file OpenTradesSnapshotEvent.hpp
/// \brief Defines the event used to synchronize the open trade counter with a broker snapshot.

namespace optionx::events {

    /// \class OpenTradesSnapshotEvent
    /// \brief Carries a broker-side snapshot of active trades for queue counter synchronization.
    class OpenTradesSnapshotEvent : public utils::Event {
    public:
        int64_t open_trades = 0;                  ///< Broker-reported active trade count.
        std::vector<int64_t> close_times_ms;      ///< Planned close timestamps for active trades.
        int64_t close_buffer_ms = 0;              ///< Delay added before a snapshot trade is counted as closed.

        /// \brief Constructor initializing a broker active-trades snapshot.
        /// \param open_trades Broker-reported active trade count.
        /// \param close_times_ms Planned close timestamps for active trades.
        /// \param close_buffer_ms Safety delay after each close timestamp.
        OpenTradesSnapshotEvent(
            int64_t open_trades,
            std::vector<int64_t> close_times_ms,
            int64_t close_buffer_ms)
            : open_trades(open_trades),
              close_times_ms(std::move(close_times_ms)),
              close_buffer_ms(close_buffer_ms) {}

        /// \brief Default virtual destructor.
        virtual ~OpenTradesSnapshotEvent() = default;

        std::type_index type() const override {
            return typeid(OpenTradesSnapshotEvent);
        }

        const char* name() const override {
            return "OpenTradesSnapshotEvent";
        }
    };

} // namespace optionx::events

#endif // _OPTIONX_EVENTS_OPEN_TRADES_SNAPSHOT_EVENT_HPP_INCLUDED
