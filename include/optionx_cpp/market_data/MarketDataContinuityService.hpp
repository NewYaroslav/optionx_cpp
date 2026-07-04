#pragma once
#ifndef _OPTIONX_MARKET_DATA_CONTINUITY_SERVICE_HPP_INCLUDED
#define _OPTIONX_MARKET_DATA_CONTINUITY_SERVICE_HPP_INCLUDED

/// \file MarketDataContinuityService.hpp
/// \brief Defines a small helper for routing historical bars through market-data batches.

namespace optionx::market_data {

    /// \class MarketDataContinuityService
    /// \brief Bridges historical bar requests into the live market-data batch pipeline.
    ///
    /// The service intentionally stays thin: providers still own transport,
    /// retry, and stream lifecycle. This helper only tags recovered payloads
    /// as historical/backfill data and packages them into BarDataBatch objects.
    class MarketDataContinuityService {
    public:
        /// \brief Callback that receives a failed history request.
        using error_callback_t = std::function<void(BarHistoryResult)>;

        /// \brief Constructs the service around a market-data provider.
        /// \param provider Provider used for historical data requests.
        explicit MarketDataContinuityService(BaseMarketDataProvider& provider)
            : m_provider(provider) {}

        /// \brief Requests historical bars and delivers them as one batch.
        /// \param request Historical bar range to fetch.
        /// \param subscription Optional live subscription related to the backfill.
        /// \param callback Batch callback used by the consumer pipeline.
        /// \param error_callback Optional callback for typed fetch failures.
        /// \param backfill_marks Whether to add the BACKFILL flag in addition to HISTORICAL.
        /// \return True if the provider accepted the history request.
        bool request_bar_history_batch(
                BarHistoryRequest request,
                MarketDataSubscriptionHandle subscription,
                BaseMarketDataProvider::bars_callback_t callback,
                error_callback_t error_callback = nullptr,
                bool backfill_marks = true) {
            if (!callback) return false;

            const auto callback_request = request;
            return m_provider.fetch_bar_history(
                request,
                [request = callback_request,
                 subscription = std::move(subscription),
                 callback = std::move(callback),
                 error_callback = std::move(error_callback),
                 backfill_marks](BarHistoryResult result) mutable {
                    if (!result) {
                        if (error_callback) {
                            error_callback(std::move(result));
                        }
                        return;
                    }

                    auto batch = make_bar_batch(
                        std::move(result.sequence),
                        request,
                        std::move(subscription),
                        backfill_marks);
                    callback(std::move(batch));
                });
        }

        /// \brief Converts a historical bar sequence into a market-data batch.
        /// \param sequence Historical sequence returned by a provider.
        /// \param request Original request used as metadata fallback.
        /// \param subscription Optional related live subscription handle.
        /// \param backfill_marks Whether to add the BACKFILL flag.
        /// \return Batch ready for delivery to bar consumers.
        static std::unique_ptr<BarDataBatch> make_bar_batch(
                BarSequence sequence,
                const BarHistoryRequest& request,
                MarketDataSubscriptionHandle subscription = {},
                bool backfill_marks = true) {
            auto batch = std::make_unique<BarDataBatch>();
            batch->subscription = std::move(subscription);
            batch->type = MarketDataType::BARS;
            batch->symbol = sequence.symbol.empty() ? request.symbol : sequence.symbol;
            batch->timeframe = sequence.timeframe > 0 ? sequence.timeframe : request.timeframe;
            batch->price_digits = sequence.price_digits;
            batch->volume_digits = sequence.volume_digits;
            batch->items = std::move(sequence.bars);

            const auto price_type = price_type_from_price_source(
                sequence.price_source == BarPriceSource::UNKNOWN
                    ? request.price_source
                    : sequence.price_source);

            for (auto& bar : batch->items) {
                bar.set_flag(MarketDataFlags::HISTORICAL);
                bar.set_flag(MarketDataFlags::BACKFILL, backfill_marks);
                bar.set_price_type(price_type);
            }

            return batch;
        }

        /// \brief Maps a bar price source to the compact market price type.
        /// \param source Bar price source.
        /// \return Matching market price type, or UNKNOWN.
        static MarketPriceType price_type_from_price_source(BarPriceSource source) noexcept {
            switch (source) {
            case BarPriceSource::BID:
                return MarketPriceType::BID;
            case BarPriceSource::ASK:
                return MarketPriceType::ASK;
            case BarPriceSource::MID:
                return MarketPriceType::MID;
            case BarPriceSource::LAST:
                return MarketPriceType::LAST;
            case BarPriceSource::UNKNOWN:
            default:
                return MarketPriceType::UNKNOWN;
            }
        }

    private:
        BaseMarketDataProvider& m_provider; ///< Provider used for history fetches.
    };

} // namespace optionx::market_data

#endif // _OPTIONX_MARKET_DATA_CONTINUITY_SERVICE_HPP_INCLUDED
