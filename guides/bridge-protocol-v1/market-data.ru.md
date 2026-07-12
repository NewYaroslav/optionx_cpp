# Bridge Protocol v1 Draft - Market Data

## Market Data Commands

Market-data commands покрывают котировки, тики и бары. Они намеренно отделены
от trade и signal commands: bridge может проксировать их в полноценную trading
platform, например adapter Intrade Bar, или в более легкий adapter конкретного
поставщика котировок.

Одновременно может быть несколько subscribers. У каждого subscriber свои
subscriptions, и он получает события только по ним. Subscriber обычно является
authenticated WebSocket connection, named-pipe client, HTTP session или
application component. `subscriber_id` optional; если он не задан, bridge
использует authenticated transport/session identity.

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

Providers должны явно сообщать unsupported features. Если точные limits
неизвестны, provider может опустить поле или вернуть `null`, но не должен
делать вид, что unsupported history или live streams доступны.

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

- `ticks`: подписка на tick updates.
- `bars`: подписка на bar/candle updates. `timeframe_ms` обязателен.

`prefill` просит provider отправить history до live events. Это полезно для
synthetic symbols, chart bootstrapping и MT4/MT5 integrations, которым нужны
последние бары для расчета обычных индикаторов.

Известные `prefill.mode` values:

- `none`: отправлять только live data.
- `count`: отправить последние `count` ticks или bars.
- `time_range`: отправить данные за `[start_ms, end_ms)`.
- `lookback`: отправить данные от `now - lookback_ms` до now.

Subscription response:

```json
{
  "status": "accepted",
  "market_data_subscription_id": "md-sub-1",
  "provider_id": "intradebar-live",
  "symbol": "BTCUSD",
  "stream": {
    "kind": "bars",
    "timeframe_ms": 60000,
    "price_type": "mid"
  },
  "prefill_status": "accepted",
  "prefill_snapshot_at_ms": 1783476705000,
  "live_from_seq": 9281,
  "warnings": []
}
```

Если provider не может выполнить history или prefill, bridge должен отклонить
request как domain result или принять live subscription с `prefill_status`
вроде `unsupported` или `limited`, в зависимости от requested policy.
JSON-RPC errors остаются для malformed envelopes или invalid params.

Когда `prefill_status` равен `accepted`, bridge должен сначала доставить
prefill events, затем emit `market_data.prefill.completed` event с boundary, и
после этого начать live events. Boundary может быть представлен как
`prefill_snapshot_at_ms`, `live_from_seq` или оба поля. Providers должны
документировать, является ли boundary gap-free, может ли содержать duplicates
или является только best-effort.

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
  "market_data_subscription_id": "md-sub-1",
  "provider_id": "binance-live",
  "symbol": "BTCUSDT",
  "tick_id": "binance-live:BTCUSDT:9281",
  "source_seq": 9281,
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
  "market_data_subscription_id": "md-sub-1",
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

Market-data prices, volumes и spreads используют то же wire-правило
decimal-string, что и торговые money values. Times используют integer
milliseconds.

`time_ms` сам по себе не является unique tick identity, потому что несколько
ticks могут прийти в одну миллисекунду. Providers должны добавлять
`source_seq`, `tick_id` или оба поля, когда источник может дать стабильный
ordering. Ingest и storage layers должны использовать эти поля для duplicate
detection, gap detection и append/upsert semantics.

Ingest/write commands позволяют внешним источникам писать котировки внутрь
OptionX ecosystem. Это полезно, когда MT4/MT5, Binance или другой источник
производит ticks/bars, а bot, synthetic chart или simulator потребляет их
внутри того же приложения.

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

`market_data.ingest.bars` имеет такую же форму, но использует `bars` плюс
`timeframe_ms`. Известные ingest modes:

- `append`: добавить новые данные и reject duplicates.
- `upsert`: insert or replace по provider, symbol, timeframe и time.
- `snapshot`: заменить текущий in-memory snapshot для target stream.

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

Market-data reports должны покрывать provider и continuity issues, например
`provider_unsupported`, `history_unavailable`, `subscription_failed`,
`stream_gap`, `prefill_limited`, `ingest_rejected`, `duplicate_tick`,
`duplicate_bar` и `timestamp_out_of_order`.
