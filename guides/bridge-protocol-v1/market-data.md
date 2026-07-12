# Bridge Protocol v1 Draft - Market Data

## Market Data Commands

Market-data commands cover quotes, ticks and bars. They are intentionally
separate from trade and signal commands: a bridge may proxy them to a full
trading platform, such as an Intrade Bar platform adapter, or to a smaller
dedicated quote provider adapter.

Multiple subscribers may exist at the same time. Each subscriber owns its own
subscriptions and receives events only for those subscriptions. A subscriber is
usually an authenticated WebSocket connection, named-pipe client, HTTP session
or application component. `subscriber_id` is optional; when omitted, the bridge
uses the authenticated transport/session identity.

Commands:

- `market_data.providers.list`
- `market_data.provider.get`
- `market_data.subscribe`
- `market_data.unsubscribe`
- `market_data.subscriptions.list`
- `market_data.history.get`
- `market_data.ingest.ticks`
- `market_data.ingest.bars`

Provider list result:

```json
{
  "providers": [
    {
      "provider_id": "intradebar-live",
      "name": "Intrade Bar Live",
      "kind": "platform",
      "platform_type": "INTRADE_BAR",
      "status": "online",
      "supports": {
        "live_ticks": true,
        "live_bars": true,
        "history_ticks": false,
        "history_bars": true,
        "ingest_ticks": false,
        "ingest_bars": false
      },
      "limits": {
        "symbols": ["EURUSD", "BTCUSD"],
        "timeframes_ms": [1000, 60000, 300000],
        "max_history_items": 10000,
        "max_history_lookback_ms": 604800000,
        "history_depth_by_symbol": {
          "EURUSD": {
            "bars_1m_ms": 2592000000
          }
        }
      },
      "metadata": {}
    }
  ]
}
```

Providers should report unsupported features explicitly. If exact limits are
unknown, the provider may omit the field or return `null`, but it must not
pretend that unsupported history or live streams are available.

Live subscription request:

```json
{
  "subscriber_id": "mt5-synthetic-feed-1",
  "provider_id": "intradebar-live",
  "symbol": "BTCUSD",
  "stream": {
    "kind": "bars",
    "timeframe_ms": 60000,
    "price_type": "mid"
  },
  "prefill": {
    "mode": "count",
    "count": 100
  }
}
```

`stream.kind` values:

- `ticks`: subscribe to tick updates.
- `bars`: subscribe to bar/candle updates. `timeframe_ms` is required.

`prefill` asks the provider to send history before live events begin. It is
useful for synthetic symbols, chart bootstrapping and MT4/MT5 integrations that
need enough recent bars to calculate ordinary indicators.

Known `prefill.mode` values:

- `none`: send only live data.
- `count`: send the last `count` ticks or bars.
- `time_range`: send data for `[start_ms, end_ms)`.
- `lookback`: send data from `now - lookback_ms` to now.

Subscription response:

```json
{
  "status": "accepted",
  "subscription_id": "md-sub-1",
  "provider_id": "intradebar-live",
  "symbol": "BTCUSD",
  "stream": {
    "kind": "bars",
    "timeframe_ms": 60000,
    "price_type": "mid"
  },
  "prefill_status": "accepted",
  "warnings": []
}
```

If a provider cannot satisfy history or prefill, the bridge should reject the
request as a domain result or accept the live subscription with a
`prefill_status` such as `unsupported` or `limited`, depending on the requested
policy. JSON-RPC errors are reserved for malformed envelopes or invalid params.

History request:

```json
{
  "provider_id": "intradebar-live",
  "symbol": "EURUSD",
  "stream": {
    "kind": "bars",
    "timeframe_ms": 60000,
    "price_type": "mid"
  },
  "range": {
    "kind": "time_range",
    "start_ms": 1783470000000,
    "end_ms": 1783477200000
  },
  "limit": 1000,
  "cursor": null
}
```

History result:

```json
{
  "provider_id": "intradebar-live",
  "symbol": "EURUSD",
  "stream": {
    "kind": "bars",
    "timeframe_ms": 60000
  },
  "items": [
    {
      "open_time_ms": 1783470000000,
      "close_time_ms": 1783470060000,
      "open": "1.14060",
      "high": "1.14070",
      "low": "1.14055",
      "close": "1.14066",
      "volume": "76.0",
      "is_closed": true
    }
  ],
  "next_cursor": null,
  "has_more": false
}
```

Tick event payload:

```json
{
  "subscription_id": "md-sub-1",
  "provider_id": "binance-live",
  "symbol": "BTCUSDT",
  "time_ms": 1783476705177,
  "bid": "64114.80",
  "ask": "64115.10",
  "last": "64114.92",
  "volume": "0.0123",
  "prefill": false
}
```

Bar event payload:

```json
{
  "subscription_id": "md-sub-1",
  "provider_id": "intradebar-live",
  "symbol": "BTCUSD",
  "timeframe_ms": 60000,
  "open_time_ms": 1783476660000,
  "close_time_ms": 1783476720000,
  "open": "64100.00",
  "high": "64150.00",
  "low": "64090.00",
  "close": "64114.92",
  "volume": "12.5",
  "is_closed": false,
  "prefill": false
}
```

Market-data prices, volumes and spreads use the same decimal-string wire rule
as trading money values. Times use integer milliseconds.

Ingest/write commands allow external sources to feed quotes into the OptionX
ecosystem. This is useful when MT4/MT5, Binance or another source produces
ticks/bars and a bot, synthetic chart or simulator consumes them inside the
same application.

`market_data.ingest.ticks` example:

```json
{
  "source": {
    "kind": "mt5",
    "source_instance_id": "terminal-01",
    "symbol": "EURUSD"
  },
  "target": {
    "provider_id": "local-synthetic",
    "symbol": "EURUSD"
  },
  "mode": "append",
  "ticks": [
    {
      "time_ms": 1783476705177,
      "bid": "1.14060",
      "ask": "1.14062",
      "last": null,
      "volume": "1.0"
    }
  ],
  "identity": {
    "sample_id": "mt5:terminal-01:1783476705177"
  }
}
```

`market_data.ingest.bars` follows the same shape, but uses `bars` plus
`timeframe_ms`. Known ingest modes:

- `append`: add new data and reject duplicates.
- `upsert`: insert or replace by provider, symbol, timeframe and time.
- `snapshot`: replace the current in-memory snapshot for the target stream.

Ingest response:

```json
{
  "status": "accepted",
  "operation_id": "op-019c...",
  "accepted_count": 1,
  "rejected_count": 0,
  "warnings": []
}
```

Market-data reports should cover provider and continuity issues such as
`provider_unsupported`, `history_unavailable`, `subscription_failed`,
`stream_gap`, `prefill_limited`, `ingest_rejected`, `duplicate_tick`,
`duplicate_bar` and `timestamp_out_of_order`.
