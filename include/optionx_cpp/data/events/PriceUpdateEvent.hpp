#pragma once
#ifndef _OPTIONX_EVENTS_PRICE_UPDATE_EVENT_HPP_INCLUDED
#define _OPTIONX_EVENTS_PRICE_UPDATE_EVENT_HPP_INCLUDED

/// \file PriceUpdateEvent.hpp
/// \brief Defines the PriceUpdateEvent class for updating tick information.

#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>
#include <utility>

namespace optionx::events {

    /// \struct TickUpdateBatch
    /// \brief Batch of tick payloads sharing one source symbol and precision metadata.
    struct TickUpdateBatch {
        std::vector<Tick> items;       ///< Tick payloads delivered together.
        std::string symbol;            ///< Provider symbol shared by all items.
        std::string provider;          ///< Data provider that produced the ticks.
        std::uint32_t price_digits = 0; ///< Decimal places for price fields.
        std::uint32_t volume_digits = 0; ///< Decimal places for volume fields.

        /// \brief Returns true when the batch has no payload items.
        [[nodiscard]] bool empty() const noexcept {
            return items.empty();
        }

        /// \brief Returns the number of tick payloads.
        [[nodiscard]] std::size_t size() const noexcept {
            return items.size();
        }
    };

    /// \class PriceUpdateEvent
    /// \brief Event containing updated tick batches for multiple symbols.
    class PriceUpdateEvent : public utils::Event {
    public:
        /// \brief Constructor initializing tick batches.
        /// \param tick_batches Tick batches grouped by symbol and precision metadata.
        /// \param source Transport-level source that produced the ticks.
        explicit PriceUpdateEvent(
                std::vector<TickUpdateBatch> tick_batches,
                MarketDataUpdateSource source = MarketDataUpdateSource::POLLING)
            : m_tick_batches(std::move(tick_batches)),
              m_source(source) {}

        /// \brief Builds a single-item tick batch.
        /// \param tick Tick payload.
        /// \param symbol Provider symbol.
        /// \param provider Data provider name.
        /// \param price_digits Decimal places for price fields.
        /// \param volume_digits Decimal places for volume fields.
        static TickUpdateBatch make_tick_batch(
                Tick tick,
                std::string symbol,
                std::string provider,
                std::uint32_t price_digits,
                std::uint32_t volume_digits) {
            TickUpdateBatch batch;
            batch.symbol = std::move(symbol);
            batch.provider = std::move(provider);
            batch.price_digits = price_digits;
            batch.volume_digits = volume_digits;
            batch.items.push_back(std::move(tick));
            return batch;
        }

        /// \brief Gets the tick batches associated with this event.
        /// \return A constant reference to grouped tick payloads.
        const std::vector<TickUpdateBatch>& get_tick_batches() const {
            return m_tick_batches;
        }

        /// \brief Finds a tick batch by its normalized symbol name.
        /// \param symbol Symbol to find.
        /// \return Pointer to the matching tick batch, or nullptr when absent.
        const TickUpdateBatch* find_tick_batch_by_symbol(const std::string& symbol) const {
            for (const auto& batch : m_tick_batches) {
                if (batch.symbol == symbol) {
                    return &batch;
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

    private:
        std::vector<TickUpdateBatch> m_tick_batches; ///< Tick batches associated with this event.
        MarketDataUpdateSource m_source = MarketDataUpdateSource::UNKNOWN; ///< Tick update source.
    };

} // namespace optionx::events

#endif // _OPTIONX_EVENTS_PRICE_UPDATE_EVENT_HPP_INCLUDED
