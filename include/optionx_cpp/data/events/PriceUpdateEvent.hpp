#pragma once
#ifndef _OPTIONX_EVENTS_PRICE_UPDATE_EVENT_HPP_INCLUDED
#define _OPTIONX_EVENTS_PRICE_UPDATE_EVENT_HPP_INCLUDED

/// \file PriceUpdateEvent.hpp
/// \brief Defines the PriceUpdateEvent class for updating tick information.

#include <vector>
#include <memory>

namespace optionx::events {

    /// \class PriceUpdateEvent
    /// \brief Event containing updated tick information for multiple symbols.
    class PriceUpdateEvent : public utils::Event {
    public:
        /// \brief Constructor initializing the tick data.
        /// \param ticks A vector of tick information.
        /// \param source Transport-level source that produced the ticks.
        explicit PriceUpdateEvent(
                std::vector<SingleTick> ticks,
                MarketDataUpdateSource source = MarketDataUpdateSource::POLLING)
            : m_ticks(std::move(ticks)),
              m_source(source) {}

        /// \brief Gets the tick data associated with this event.
        /// \return A constant reference to the vector of tick information.
        const std::vector<SingleTick>& get_ticks() const {
            return m_ticks;
        }

        /// \brief Finds a tick wrapper by its normalized symbol name.
        /// \param symbol Symbol to find.
        /// \return Pointer to the matching tick wrapper, or nullptr when absent.
        const SingleTick* find_tick_by_symbol(const std::string& symbol) const {
            for (const auto& tick : m_ticks) {
                if (tick.symbol == symbol) {
                    return &tick;
                }
            }
            return nullptr;
        }

        /// \brief Returns the source that produced this tick update.
        /// \return Transport-level market-data source for routing decisions.
        MarketDataUpdateSource source() const noexcept {
            return m_source;
        }

        /// \brief Default virtual destructor.
        virtual ~PriceUpdateEvent() = default;
        
        std::type_index type() const override {
            return typeid(PriceUpdateEvent);
        }

        const char* name() const override {
            return "PriceUpdateEvent";
        }

        /// \brief Finds a tick by its symbol name.
        /// \param symbol The name of the symbol to find.
        /// \return Matching tick wrapper, or a default wrapper when absent.
        SingleTick get_tick_by_symbol(const std::string& symbol) const {
            if (const auto* tick = find_tick_by_symbol(symbol)) {
                return *tick;
            }
            return SingleTick();
        }

    private:
        std::vector<SingleTick> m_ticks; ///< Tick information associated with this event.
        MarketDataUpdateSource m_source = MarketDataUpdateSource::UNKNOWN; ///< Tick update source.
    };

} // namespace optionx::events

#endif // _OPTIONX_EVENTS_PRICE_UPDATE_EVENT_HPP_INCLUDED
