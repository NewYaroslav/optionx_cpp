#pragma once
#ifndef OPTIONX_HEADER_MARKET_DATA_MARKET_DATA_BATCH_HPP_INCLUDED
#define OPTIONX_HEADER_MARKET_DATA_MARKET_DATA_BATCH_HPP_INCLUDED

/// \file MarketDataBatch.hpp
/// \brief Defines market-data delivery batches.

namespace optionx::market_data {

    /// \struct MarketDataBatch
    /// \brief A routed batch of market-data payloads sharing one stream context.
    /// \tparam Payload Tick or bar payload type.
    template<class Payload>
    struct MarketDataBatch {
        std::vector<Payload> items; ///< Payload items delivered together.
        MarketDataSubscriptionHandle subscription; ///< Source subscription, if any.
        MarketDataType type = MarketDataType::UNKNOWN; ///< Payload type.
        std::string symbol; ///< Provider symbol shared by all items.
        BarTimeframe timeframe = 0; ///< Bar timeframe in seconds, or 0 for ticks.
        std::uint32_t price_digits = 0; ///< Decimal places for price fields.
        std::uint32_t volume_digits = 0; ///< Decimal places for volume fields.

        /// \brief Returns true when the batch has no payload items.
        [[nodiscard]] bool empty() const noexcept {
            return items.empty();
        }

        /// \brief Returns the number of payload items.
        [[nodiscard]] std::size_t size() const noexcept {
            return items.size();
        }
    };

    using TickDataBatch = MarketDataBatch<Tick>; ///< Tick delivery batch.
    using BarDataBatch = MarketDataBatch<Bar>;   ///< Bar delivery batch.

} // namespace optionx::market_data

#endif // OPTIONX_HEADER_MARKET_DATA_MARKET_DATA_BATCH_HPP_INCLUDED
