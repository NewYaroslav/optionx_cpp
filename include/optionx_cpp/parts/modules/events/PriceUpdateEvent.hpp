#pragma once
#ifndef _OPTIONX_MODULES_PRICE_UPDATE_EVENT_HPP_INCLUDED
#define _OPTIONX_MODULES_PRICE_UPDATE_EVENT_HPP_INCLUDED

/// \file PriceUpdateEvent.hpp
/// \brief Defines the PriceUpdateEvent class for updating tick information.

#include "../../pubsub/Event.hpp"
#include "../../utils/TickInfo.hpp"
#include <vector>
#include <memory>

namespace optionx {
namespace modules {

    /// \class PriceUpdateEvent
    /// \brief Event containing updated tick information for multiple symbols.
    class PriceUpdateEvent : public Event {
    public:
        /// \brief Constructor initializing the tick data.
        /// \param ticks A vector of tick information.
        explicit PriceUpdateEvent(std::vector<TickInfo> ticks)
            : m_ticks(std::move(ticks)) {}

        /// \brief Gets the tick data associated with this event.
        /// \return A constant reference to the vector of tick information.
        const std::vector<TickInfo>& get_ticks() const {
            return m_ticks;
        }

        /// \brief Default virtual destructor.
        virtual ~PriceUpdateEvent() = default;

        /// \brief Finds a tick by its symbol name.
        /// \param symbol The name of the symbol to find.
        /// \return An optional containing the found TickInfo.
        TickInfo get_tick_by_symbol(const std::string& symbol) const {
            for (const auto& tick : m_ticks) {
                if (tick.symbol == symbol) {
                    return tick;
                }
            }
            return TickInfo();
        }

    private:
        std::vector<TickInfo> m_ticks; ///< Tick information associated with this event.
    };

} // namespace modules
} // namespace optionx

#endif // _OPTIONX_MODULES_PRICE_UPDATE_EVENT_HPP_INCLUDED
