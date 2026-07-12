# Bridge Protocol v1 Draft - Trading И Signals

## Commands

Примеры commands ниже показывают JSON-RPC `params` object для указанного method,
а не весь внешний JSON-RPC envelope.

### `protocol.hello`

Возвращает базовую server identity и выбранную protocol version. Это полезно
для clients, которые могут говорить с несколькими bridge implementations.

```json
{
  "client": {
    "name": "mgc-platform",
    "version": "0.1.0"
  },
  "requested_protocol_versions": ["draft"]
}
```

### `protocol.capabilities.get`

Возвращает supported methods, feature flags и limits.

```json
{
  "protocol_versions": ["draft"],
  "server_instance_id": "019c...",
  "supported_methods": [
    "signal.submit",
    "trade.open",
    "trade.result.get",
    "market_data.providers.list",
    "market_data.subscribe",
    "market_data.history.get",
    "market_data.ingest.ticks"
  ],
  "features": {
    "subscriptions": true,
    "event_replay": false,
    "trade_open_batch": false,
    "routing_all": true,
    "buffer_encoding": ["json", "binary_base64"],
    "auth": {
      "api_key": true,
      "scopes": true
    },
    "market_data": {
      "live_ticks": true,
      "live_bars": true,
      "history_ticks": true,
      "history_bars": true,
      "stream_prefill": true,
      "ingest_ticks": true,
      "ingest_bars": true
    }
  },
  "limits": {
    "max_message_bytes": 1048576,
    "max_buffers": 32,
    "max_buffer_values": 1000,
    "max_market_data_subscriptions": 64,
    "max_market_data_prefill_items": 1000,
    "max_market_data_ingest_items": 10000,
    "event_retention_ms": 0,
    "max_replay_events": 0,
    "max_page_size": 1000
  }
}
```

### `trade.open`

Использовать, когда внешний client хочет конкретную сделку с явными trade
parameters.

```json
{
  "protocol_version": "draft",
  "context": {
    "idempotency_key": "manual:client-trade-1",
    "client_created_at_ms": 1783476719900,
    "valid_until_ms": 1783476725000
  },
  "routing": {
    "selector": {
      "kind": "account",
      "account_id": "1"
    },
    "platform_type": "INTRADE_BAR"
  },
  "trade": {
    "symbol": "EURUSD",
    "order_type": "BUY",
    "option_type": "SPRINT",
    "amount": "10.00",
    "expiry": {
      "kind": "duration",
      "duration_ms": 60000
    },
    "min_payout": "0.75",
    "refund": "0.0"
  },
  "identity": {
    "unique_hash": "client-trade-1",
    "signal_name": "manual"
  },
  "metadata": {
    "rsi": "27.4",
    "price": "1.14072"
  }
}
```

Response result:

```json
{
  "status": "accepted",
  "final": false,
  "operation_id": "op-019c...",
  "trade_refs": [
    {
      "trade_id": "123",
      "account_id": "1",
      "platform_type": "INTRADE_BAR",
      "unique_hash": "client-trade-1"
    }
  ]
}
```

`trade.open` должен означать одну конкретную сделку на одном выбранном account в
stable protocol. Будущая команда `trade.open.batch` должна покрывать явные
multi-trade submissions. Fan-out в первую очередь относится к `signal.submit`.

`trade.open` все равно может проходить через тот же intake, validation, risk,
reporting и persistence lifecycle, что и `signal.submit`. Отличается intent:
`trade.open` уже содержит executable trade request, а `signal.submit` может
требовать sizing, routing и decision logic до появления какой-либо trade.
Lower-level execution component может конвертировать напрямую в `TradeRequest`,
но public bridge APIs должны сохранять observable operation/report/origin-signal
lifecycle.

### `signal.submit`

Использовать, когда внешний client отправляет strategy signal. Money
management, routing и filters позже могут породить ноль, одну или несколько
сделок.

```json
{
  "protocol_version": "draft",
  "context": {
    "idempotency_key": "tv:abc123",
    "client_created_at_ms": 1783476705177,
    "valid_until_ms": 1783476710000
  },
  "routing": {
    "selector": {
      "kind": "accounts",
      "account_ids": ["1", "2", "3"]
    },
    "policy": "best_payout"
  },
  "signal": {
    "symbol": "EURUSD",
    "order_type": "SELL",
    "option_type": "SPRINT",
    "expiry": {
      "kind": "duration",
      "duration_ms": 60000
    },
    "min_payout": "0.75"
  },
  "sizing": {
    "mode": "risk_manager",
    "system": "kelly",
    "params": {
      "fraction": 0.25
    }
  },
  "identity": {
    "signal_name": "noisy_rsi_test",
    "idempotency_key": "tv:abc123"
  },
  "metadata": {
    "rsi": "72.1",
    "price": "1.14072"
  }
}
```

Response result:

```json
{
  "status": "accepted",
  "final": false,
  "operation_id": "op-019c...",
  "signal_ref": {
    "signal_id": "101",
    "unique_hash": "tv:abc123",
    "signal_name": "noisy_rsi_test"
  },
  "trade_refs": []
}
```

`trade_refs` может быть пустым, потому что signal processing может быть
асинхронным или потому что risk/decision logic отклонила signal. Clients должны
использовать `signal.trades.query` или subscriptions, чтобы наблюдать созданные
trades.

### Operation Lifecycle

Long-running commands возвращают `operation_id`. Это отделяет синхронное
принятие command от последующей broker/risk-management обработки.

Известные operation states:

- `accepted`: command принята в обработку.
- `processing`: bridge/application ее обрабатывает.
- `completed`: все targets завершены успешно.
- `partially_completed`: часть fan-out targets успешна, часть failed.
- `rejected`: valid command отклонена бизнес-логикой.
- `failed`: processing неожиданно failed.
- `cancelled`: operation отменена до completion.

Commands:

- `operation.get`
- `operation.cancel`

`operation.get` result:

```json
{
  "operation_id": "op-019c...",
  "status": "partially_completed",
  "final": true,
  "targets": [
    {
      "account_id": "1",
      "status": "accepted",
      "trade_id": "123"
    },
    {
      "account_id": "2",
      "status": "rejected",
      "reason": {
        "code": "insufficient_balance"
      }
    }
  ]
}
```

### `signal.trades.query`

Возвращает trades, связанные с signal.

```json
{
  "selector": {
    "kind": "unique_hash",
    "value": "tv:abc123"
  },
  "include_active": true,
  "include_history": true
}
```

### `signal.buffers.submit`

Использовать для MT4/MT5-style indicator buffers. Bridge интерпретирует recent
buffer values вместо того, чтобы полагаться на MQL для финального решения о
signal.

MT4/MT5 indicator buffers - series arrays: index `0` это current/latest bar,
index `1` - previous bar и так далее.

```json
{
  "source": {
    "kind": "mt5_indicator",
    "source_instance_id": "terminal-01",
    "terminal": "MT5",
    "indicator": "SuperArrow",
    "indicator_version": "1.4",
    "settings_hash": "sha256:...",
    "symbol": "EURUSD",
    "timeframe_sec": 60,
    "observed_at_ms": 1783476705177,
    "sample_id": "mt5:terminal-01:1783476705177"
  },
  "indexing": {
    "mode": "mt_series",
    "zero_index": "current_bar"
  },
  "buffers": [
    {
      "name": "buy",
      "role": "buy",
      "empty_policy": "zero_or_empty_value",
      "empty_value": "2147483647.0",
      "values": ["0.0", "1.14072"]
    },
    {
      "name": "sell",
      "role": "sell",
      "empty_policy": "zero_or_empty_value",
      "empty_value": "2147483647.0",
      "values": ["0.0", "0.0"]
    }
  ],
  "bars": [
    {
      "index": 0,
      "open_time_ms": 1783476720000,
      "is_closed": false,
      "open": "1.14060",
      "high": "1.14070",
      "low": "1.14055",
      "close": "1.14066",
      "volume": "76.0"
    },
    {
      "index": 1,
      "open_time_ms": 1783476660000,
      "is_closed": true,
      "open": "1.14048",
      "high": "1.14072",
      "low": "1.14044",
      "close": "1.14060",
      "volume": "93.0"
    }
  ],
  "policy": {
    "lookback_bars": 5,
    "accept_current_bar": false,
    "accept_previous_bar": true,
    "max_lag_bars": 1
  },
  "identity": {
    "signal_name": "SuperArrow"
  },
  "metadata": {}
}
```

Заметки:

- `values` должен содержать только последние N bars, а не всю terminal history.
- `empty_value` нужен потому, что MQL indicators часто используют специальную
  константу EMPTY_VALUE.
- `empty_policy` нужен потому, что многие пользовательские indicators
  используют `0` как no data.
- Все массивы `buffers[].values` должны иметь одинаковую длину.
- `bars` optional. Если он присутствует, его длина должна совпадать с каждым
  buffer, а `bars[i]` описывает `values[i]`.
- Bar time - это `open_time_ms`. Использовать `is_closed`, чтобы отличать
  current bar от closed historical bars.
- `source_instance_id`, `indicator_version`, `settings_hash` и `sample_id`
  помогают обнаруживать replay, duplicated terminals и изменения настроек
  indicator.
- Bridge может emit reports вроде `late_signal`, `repainted_signal` или
  `signal_disappeared` вместо forwarding tradeable signal.

Известные `empty_policy` values:

- `empty_value`: только `empty_value` означает no data.
- `zero_is_empty`: только zero означает no data.
- `zero_or_empty_value`: zero и `empty_value` означают no data.
- `null_is_empty`: JSON null означает no data.

Default buffer encoding - JSON arrays, потому что их легко инспектировать и
легко производить из MQL scripts. Большие payloads могут использовать compact
encoding, если bridge объявляет его в `protocol.capabilities.get`.

Compact binary buffer example:

```json
{
  "encoding": {
    "kind": "binary_base64",
    "endianness": "little",
    "value_type": "float64",
    "layout": "buffer_major",
    "count": 512
  },
  "buffers": [
    {
      "name": "buy",
      "role": "buy",
      "empty_policy": "zero_or_empty_value",
      "empty_value": "2147483647.0",
      "values_base64": "AAAA..."
    }
  ],
  "bars": {
    "kind": "compact",
    "open_time_ms_base64": "AAAA...",
    "is_closed_bitmap_base64": "AQ=="
  }
}
```

Compact payloads все равно должны объявлять ту же semantic metadata, что и JSON
buffers: indexing mode, empty policy, value type, item count и смысл bar time.
Bridges, которые не объявили requested encoding, должны вернуть domain
rejection вроде `unsupported_buffer_encoding`.

Production `binary_base64` должен быть удобен для MT4/MT5 и C++:

- Numeric buffer values используют IEEE-754 `float64` (`double`) в little-endian
  byte order. Это соответствует естественному MQL/C++ представлению на целевых
  Windows platforms и убирает overhead decimal parsing.
- Integer timestamps используют signed little-endian `int64` milliseconds since
  Unix epoch. MQL `datetime` хранит seconds, поэтому adapters должны
  конвертировать их в milliseconds перед encoding.
- Default layout - `buffer_major`: каждый buffer является contiguous byte block
  для values `[0..count-1]`, сохраняя MT series indexing, где `0` - текущий или
  последний bar.
- Compact bars должны использовать отдельные typed arrays вроде
  `open_time_ms_base64`, `open_base64`, `high_base64`, `low_base64`,
  `close_base64`, `volume_base64` и `is_closed_bitmap_base64`. Не зависеть от
  compiler-specific C struct packing.
- JSON transports несут эти byte blocks как standard base64 strings. Future
  binary transport или length-prefixed named-pipe frame может нести те же byte
  blocks без base64.
- Compression можно добавить позже как явное поле `compression`; uncompressed
  format остается baseline interoperability format.

### `backtest.result.import`

Использовать, когда внешний tester уже рассчитал trade result.

```json
{
  "source": {
    "kind": "backtest",
    "engine": "mt5",
    "name": "EURUSD 1m RSI batch",
    "run_id": "bt-2026-07-12-a"
  },
  "signal": {
    "symbol": "EURUSD",
    "order_type": "BUY",
    "signal_name": "rsi_cross"
  },
  "trade": {
    "symbol": "EURUSD",
    "order_type": "BUY",
    "option_type": "SPRINT",
    "amount": "10.00",
    "expiry": {
      "kind": "duration",
      "duration_ms": 60000
    },
    "min_payout": "0.75"
  },
  "result": {
    "state": "closed",
    "outcome": "win",
    "final": true,
    "revision": 1,
    "payout": "0.82",
    "profit": "8.20",
    "open_price": "1.14072",
    "close_price": "1.14120",
    "open_time_ms": 1783476720000,
    "close_time_ms": 1783476780000,
    "spread": {
      "open": "0.0001",
      "close": "0.0001",
      "unit": "price"
    }
  },
  "metadata": {}
}
```

`source.kind = "backtest"` должен сохраняться даже когда физическое storage -
отдельная database. Предпочтительная deployment model для приложения -
отдельная backtest database, которая использует те же storage classes, что и
real/demo trading, но не смешивает historical tests с live trade history.
`PlatformType::SIMULATOR` уже существует в C++ domain model и полезен для live
simulation на real-time prices, например "запустить на неделю без реальных
сделок".

### `trade.result.get`

Возвращает текущий result snapshot одной trade. Это полезно и для intermediate
states, и для final results.

```json
{
  "selector": {
    "kind": "trade_id",
    "value": "123"
  }
}
```

Selectors избегают неоднозначного priority, когда передано несколько IDs.
Known selector kinds включают `trade_id`, `broker_option_id`,
`broker_option_hash` и `unique_hash`.

Result shape должен сохранять данные `TradeResult`, но экспортировать отдельные
lifecycle и financial outcome fields:

```json
{
  "trade_id": "123",
  "state": "opened",
  "outcome": "unknown",
  "final": false,
  "revision": 4,
  "failure": null,
  "broker_option_id": "456",
  "broker_option_hash": "abc",
  "amount": "10.00",
  "payout": "0.82",
  "profit": "0.00",
  "balance": "1000.00",
  "open_balance": "1000.00",
  "close_balance": "0.00",
  "open_price": "1.14072",
  "close_price": "0.0",
  "delay_ms": 120,
  "ping_ms": 30,
  "place_time_ms": 1783476719900,
  "send_time_ms": 1783476720000,
  "open_time_ms": 1783476720120,
  "close_time_ms": 1783476780000,
  "account_type": "DEMO",
  "currency": "USD",
  "platform_type": "INTRADE_BAR",
  "origin_signal": {}
}
```

Known lifecycle states:

- `queued`
- `submitted`
- `opened`
- `closed`
- `cancelled`
- `failed`

Known outcomes:

- `unknown`
- `win`
- `loss`
- `draw`
- `refund`

Минимально допустимые lifecycle transitions:

| From | To |
| --- | --- |
| `queued` | `submitted`, `cancelled`, `failed` |
| `submitted` | `opened`, `cancelled`, `failed` |
| `opened` | `closed`, `failed` |
| `closed` | terminal |
| `cancelled` | terminal |
| `failed` | terminal |

Существующий C++ `TradeState` enum смешивает lifecycle, errors и outcomes.
Bridge adapters должны мапить его в protocol `state` / `outcome` / `failure`
shape на границе, а не экспортировать internal enum values напрямую.

Для `signal.submit` clients обычно должны получать results через
`signal.trades.query`, filters в `trade.history.query` или `trade.updated`
events, которые включают `origin_signal`.

### `trade.history.query`

Возвращает closed или persisted trade records. Payload должен отражать
`TradeRecordQuery` и `TradeRecordFilter`.

Использовать один `selector` для direct lookup или `filter` плюс pagination для
list queries. Если присутствуют оба, `selector` должен сначала сузить query, а
`filter` все равно должен совпасть с той же record; bridges не должны молча
выбирать priority между конфликтующими IDs.

```json
{
  "time_range": {
    "start_ms": 1783000000000,
    "end_ms": 1783600000000,
    "field": "close_time"
  },
  "filter": {
    "account_id": "1",
    "platform_type": "INTRADE_BAR",
    "symbol": "EURUSD",
    "signal_name": "rsi_cross",
    "outcome": ["win", "loss"]
  },
  "limit": 1000,
  "cursor": null
}
```

Time ranges являются half-open: `[start_ms, end_ms)`. Cursor pagination
предпочтительнее offset pagination, потому что новые records могут появляться,
пока client листает history. Responses должны включать `next_cursor`,
`has_more` и `snapshot_at_ms`, когда используется pagination.

History response:

```json
{
  "items": [
    {
      "trade_id": "123",
      "state": "closed",
      "outcome": "win",
      "final": true,
      "revision": 7,
      "account_id": "1",
      "platform_type": "INTRADE_BAR",
      "symbol": "EURUSD",
      "amount": "10.00",
      "profit": "8.20",
      "open_time_ms": 1783476720120,
      "close_time_ms": 1783476780000,
      "origin_signal": {}
    }
  ],
  "next_cursor": "cursor-opaque",
  "has_more": true,
  "snapshot_at_ms": 1783600000000,
  "order": {
    "field": "close_time_ms",
    "direction": "desc",
    "tie_breaker": "trade_id"
  }
}
```

Cursors - opaque strings, которыми владеет bridge implementation. Clients не
должны парсить или изменять их. Cursor может кодировать snapshot boundary,
sort direction и last seen key. Bridge может отклонить expired cursors с
`cursor_expired`.

### `trade.active.query`

Возвращает trades, которые еще не closed.

```json
{
  "filter": {
    "account_id": "1",
    "platform_type": "INTRADE_BAR",
    "symbol": "EURUSD"
  }
}
```

### Account And Platform Commands

Commands:

- `account.list`
- `account.get`
- `account.balance.get`
- `account.status.get`
- `platform.list`

Account response item:

```json
{
  "account_id": "1",
  "platform_type": "INTRADE_BAR",
  "account_type": "DEMO",
  "currency": "USD",
  "balance": "1000.00",
  "connected": true,
  "trade_enabled": true,
  "open_trades": 2,
  "max_trades": 10,
  "metadata": {}
}
```

### Trading Control

Commands:

- `trading.pause`
- `trading.resume`
- `trading.status`

Payload:

```json
{
  "scope": {
    "account_id": "1",
    "platform_type": "INTRADE_BAR"
  },
  "reason": "manual_pause"
}
```

Пустой `scope` означает все accounts/platforms.
