# Bridge Protocol v1 Draft - Trading And Signals

## Commands

Command examples below show the JSON-RPC `params` object for the named method,
not the full outer JSON-RPC envelope.

### `protocol.hello`

Return basic server identity and selected protocol version. This is useful for
clients that can speak to multiple bridge implementations.

For HTTP, the selected protocol version is already implied by the route prefix
such as `/api/v1`; `protocol.hello` confirms it. For WebSocket, the selected
version is implied by the accepted subprotocol. For named-pipe and similar
session transports, `protocol.hello` may perform the actual version handshake.

Request params:

```json
{
  "client": {
    "name": "mgc-platform",
    "version": "0.1.0"
  },
  "requested_protocol_versions": ["1"]
}
```

Result:

```json
{
  "selected_protocol_version": "1",
  "installation_id": "installation-019c",
  "server_instance_id": "019c...",
  "session_id": "session-019c..."
}
```

### `protocol.capabilities.get`

Return supported methods, feature flags and limits.

Request params:

```json
{}
```

Result:

```json
{
  "protocol_versions": ["1"],
  "server_instance_id": "019c...",
  "supported_methods": [
    "protocol.hello",
    "protocol.capabilities.get",
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

The `supported_methods` list above is an excerpt. A real
`protocol.capabilities.get` result must report the exact methods supported by
that bridge instance.

## Core Method Summary

| Method | Side effect | Required scope | Idempotency | Async | Result/event |
| --- | ---: | --- | ---: | ---: | --- |
| `protocol.hello` | no | none | no | no | selected version/session |
| `protocol.capabilities.get` | no | none | no | no | capabilities snapshot |
| `trade.open` | yes | `trade:write` | required | yes | `operation_id`, `trade.updated` |
| `signal.submit` | yes | `trade:write` | required | yes | `operation_id`, `signal.*`, `trade.updated` |
| `operation.get` | no | matching read scope | no | no | operation snapshot |
| `operation.cancel` | yes | matching write/control scope | recommended | yes | operation snapshot/events |
| `trade.result.get` | no | `trade:read` | no | no | trade result snapshot |
| `trade.history.query` | no | `trade:read` | no | no | paged trade records |
| `trade.active.query` | no | `trade:read` | no | no | active trade snapshots |
| `market_data.providers.list` | no | `market_data:read` | no | no | provider list |
| `market_data.provider.get` | no | `market_data:read` | no | no | provider details |
| `market_data.subscribe` | yes | `market_data:read` | recommended | no | market-data subscription id |
| `market_data.unsubscribe` | yes | `market_data:read` | idempotent | no | subscription removed |
| `market_data.history.get` | no | `market_data:read` | no | maybe | history page |
| `market_data.ingest.ticks` | yes | `market_data:write` | recommended | maybe | operation/counts |
| `market_data.ingest.bars` | yes | `market_data:write` | recommended | maybe | operation/counts |
| `events.subscribe` | yes | topic read scopes | recommended | no | event subscription id |
| `events.unsubscribe` | yes | topic read scopes | idempotent | no | subscription removed |
| `events.subscriptions.list` | no | topic read scopes | no | no | subscription list |

### `trade.open`

Use this when the external client wants a concrete trade with explicit trade
parameters.

```json
{
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
    "amount": {
      "value": "10.00",
      "currency": "USD"
    },
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

`trade.open` should represent one concrete trade on one selected account in the
stable protocol. A future `trade.open.batch` command should cover explicit
multi-trade submissions. Fan-out belongs primarily to `signal.submit`.

Method-specific routing validation:

- `routing.selector.kind` may be `default` or `account`.
- `routing.selector.kind = accounts` and `all` are invalid for `trade.open`.
- `routing.policy` is invalid for `trade.open`.

`trade.open` is still allowed to pass through the same intake, validation,
risk, reporting and persistence lifecycle as `signal.submit`. The difference is
intent: `trade.open` already contains an executable trade request, while
`signal.submit` may require sizing, routing and decision logic before any trade
exists. A lower-level execution component may convert directly to `TradeRequest`,
but public bridge APIs should keep the operation/report/origin-signal lifecycle
observable.

### `signal.submit`

Use this when the external client submits a strategy signal. Money management,
routing and filters may later produce zero, one or many trades.

Method-specific routing validation:

- `routing.selector.kind` may be `default`, `account`, `accounts` or `all`.
- `routing.policy` is allowed and may select best payout, fan-out, risk-manager
  routing or another application-defined policy.

The current C++ HTTP/WebSocket server bridge emits one `TradeSignal` per command
and therefore accepts only routing shapes it can preserve today:
`default` and single numeric `account`. It rejects `accounts`, `all` and
`routing.policy` until fan-out routing is implemented.

The example below shows the fuller protocol-level shape. The current C++
HTTP/WebSocket and named-pipe bridges also reject `risk_manager`,
`balance_percent`, `system` and `params` sizing fields with `invalid_params`
until those concepts have typed DTO support.

```json
{
  "context": {
    "idempotency_key": "retry:tv-alert-abc123",
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
    "unique_hash": "tv:abc123",
    "signal_name": "noisy_rsi_test"
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

`trade_refs` may be empty because signal processing can be asynchronous or
because risk/decision logic rejected the signal. Clients should use
`signal.trades.query` or subscriptions to observe produced trades.

`identity.unique_hash`, `identity.unique_id` and `identity.signal_name` are
optional domain identity fields. The bridge must not derive `unique_hash` from
`context.idempotency_key` unless this behavior is explicitly configured and
documented. If no `unique_hash` is supplied or generated by the domain layer,
responses should omit it.

### Operation Lifecycle

Long-running commands return an `operation_id`. This separates synchronous
command acceptance from later broker/risk-management processing.

Known operation states:

- `accepted`: command accepted for processing.
- `processing`: bridge/application is working on it.
- `completed`: all targets completed successfully.
- `partially_completed`: some targets succeeded and at least one other target
  ended in a non-success terminal state.
- `rejected`: valid command rejected by business logic.
- `failed`: processing failed unexpectedly.
- `cancelled`: operation was cancelled before completion.

Known target states:

- `pending`: target has not completed yet.
- `succeeded`: target completed and produced a trade or final target result.
- `rejected`: target was rejected by business logic.
- `failed`: target failed unexpectedly.
- `cancelled`: target was cancelled before completion.

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
      "status": "succeeded",
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

`final` is a convenience flag but must be consistent with `status`:

- `accepted` and `processing` imply `final = false`.
- `completed`, `partially_completed`, `rejected`, `failed` and `cancelled`
  imply `final = true`.
- A snapshot that contradicts this invariant is a protocol/internal error.

Fan-out aggregation rules:

- If any target is still `pending`, the operation remains `accepted` or
  `processing` with `final = false`.
- Terminal aggregation is evaluated only after every target reaches a terminal
  state.
- All targets `succeeded` -> operation `completed`.
- Some terminal targets `succeeded` and some terminal targets did not ->
  `partially_completed`.
- All targets `rejected` -> `rejected`.
- No target succeeded and at least one target `failed` -> `failed`.
- All targets `cancelled` -> `cancelled`.
- Mixed terminal outcomes with no successes should use the most severe
  available status in this order: `failed`, `rejected`, `cancelled`.

`operation.cancel` is best effort. A trade already submitted to a broker may be
impossible to cancel, and cancelling an operation does not imply closing an
already opened trade. Bridges may expose cancellation intent as a flag while the
operation remains in its normal state:

```json
{
  "status": "processing",
  "cancellation_requested": true
}
```

### `signal.trades.query`

Return trades related to a signal.

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

Use this for MT4/MT5 style indicator buffers. The bridge interprets recent
buffer values instead of relying on MQL to decide the final signal.

MT4/MT5 indicator buffers are series arrays: index `0` is the current/latest
bar, index `1` is the previous bar, and so on.

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
    "timeframe_ms": 60000,
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
    "lookback_bars": 2,
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

Notes:

- `values` should contain only the last N bars, not the whole terminal history.
- `empty_value` exists because MQL indicators often use a special EMPTY_VALUE
  constant.
- `empty_policy` is necessary because many user indicators use `0` as no data.
- All `buffers[].values` arrays must have the same length.
- `policy.lookback_bars` must be less than or equal to `buffers[].values.size`.
- `bars` is optional. When present, it must have the same length as each buffer,
  and `bars[i]` describes `values[i]`.
- Bar time is `open_time_ms`. Use `is_closed` to distinguish the current bar
  from closed historical bars.
- `source_instance_id`, `indicator_version`, `settings_hash` and `sample_id`
  help detect replay, duplicated terminals and indicator setting changes.
- The bridge can emit reports such as `late_signal`, `repainted_signal` or
  `signal_disappeared` instead of forwarding a tradeable signal.

Known `empty_policy` values:

- `empty_value`: only `empty_value` means no data.
- `zero_is_empty`: only zero means no data.
- `zero_or_empty_value`: zero and `empty_value` mean no data.
- `null_is_empty`: JSON null means no data.

Default buffer encoding is JSON arrays because it is inspectable and easy for
MQL scripts to produce. Large payloads may use a compact encoding when the
bridge advertises it in `protocol.capabilities.get`.

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

Compact payloads must still declare the same semantic metadata as JSON buffers:
indexing mode, empty policy, value type, item count and bar-time meaning.
Bridges that do not advertise the requested encoding should return a domain
result with `status = "rejected"` and
`reason.code = "unsupported_buffer_encoding"`. Malformed encoding structure,
corrupted base64 or missing required encoding fields should be `-32602 invalid
params`.

Production `binary_base64` should be optimized for MT4/MT5 and C++:

- Numeric buffer values use IEEE-754 `float64` (`double`) in little-endian byte
  order. This matches the natural MQL/C++ representation on the target Windows
  platforms and avoids decimal parsing overhead.
- Integer timestamps use signed little-endian `int64` milliseconds since Unix
  epoch. MQL `datetime` values are seconds, so adapters must convert them to
  milliseconds before encoding.
- The default layout is `buffer_major`: each buffer is one contiguous byte block
  for values `[0..count-1]`, preserving MT series indexing where `0` is the
  current/latest bar.
- Compact bars should use separate typed arrays such as
  `open_time_ms_base64`, `open_base64`, `high_base64`, `low_base64`,
  `close_base64`, `volume_base64` and `is_closed_bitmap_base64`. Do not depend
  on compiler-specific C struct packing.
- JSON transports carry these byte blocks as standard base64 strings. A future
  binary transport or length-prefixed named-pipe frame may carry the same byte
  blocks without base64.
- Compression may be added later as an explicit `compression` field; the
  uncompressed format remains the baseline interoperability format.

### `backtest.result.import`

Use this when an external tester has already calculated the trade result.

```json
{
  "context": {
    "idempotency_key": "backtest:bt-2026-07-12-a:trade-1042"
  },
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
  "identity": {
    "unique_hash": "backtest:bt-2026-07-12-a:trade-1042"
  },
  "trade": {
    "symbol": "EURUSD",
    "order_type": "BUY",
    "option_type": "SPRINT",
    "amount": {
      "value": "10.00",
      "currency": "USD"
    },
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
    "profit": {
      "value": "8.20",
      "currency": "USD"
    },
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

`backtest.result.import` is state-changing. It should include
`context.idempotency_key` for RPC retry safety and `identity.unique_hash` for
domain-level deduplication of the imported trade record. `run_id` identifies the
whole test run and is not enough to identify one imported trade.

`source.kind = "backtest"` should be kept even when the physical storage is a
separate database. The preferred application deployment model is a separate
backtest database that uses the same storage classes as real/demo trading, but
does not mix historical tests with live trade history. `PlatformType::SIMULATOR`
already exists in the C++ domain model and is useful for live simulation on
real-time prices, for example "run for a week without real trades".

### `trade.result.get`

Return the current result snapshot for one trade. This is useful for
intermediate states as well as final results.

```json
{
  "selector": {
    "kind": "trade_id",
    "value": "123"
  }
}
```

Selectors avoid ambiguous priority when several IDs are present. Known selector
kinds include `trade_id`, `broker_option_id`, `broker_option_hash` and
`unique_hash`.

Result shape should preserve `TradeResult` data while exposing separate
lifecycle and financial outcome fields:

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
  "amount": {
    "value": "10.00",
    "currency": "USD"
  },
  "payout": "0.82",
  "profit": null,
  "expected_profit": {
    "value": "8.20",
    "currency": "USD"
  },
  "balance": {
    "value": "1000.00",
    "currency": "USD"
  },
  "open_balance": {
    "value": "1000.00",
    "currency": "USD"
  },
  "close_balance": null,
  "open_price": "1.14072",
  "close_price": null,
  "delay_ms": 120,
  "ping_ms": 30,
  "place_time_ms": 1783476719900,
  "send_time_ms": 1783476720000,
  "open_time_ms": 1783476720120,
  "close_time_ms": null,
  "expected_close_time_ms": 1783476780000,
  "account_type": "DEMO",
  "currency": "USD",
  "platform_type": "INTRADE_BAR",
  "origin_signal": {}
}
```

Unknown or unavailable snapshot values should be `null`. Fields that are not
applicable should be omitted. Known monetary zero remains a real money value,
for example `{ "value": "0.00", "currency": "USD" }`. `profit` is
realized/final profit; for an opened binary option it should normally be
`null`, while potential payout can be represented as `expected_profit`.

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

Allowed minimal lifecycle transitions:

| From | To |
| --- | --- |
| `queued` | `submitted`, `cancelled`, `failed` |
| `submitted` | `opened`, `cancelled`, `failed` |
| `opened` | `closed`, `failed` |
| `closed` | terminal |
| `cancelled` | terminal |
| `failed` | terminal |

The existing C++ `TradeState` enum mixes lifecycle, errors and outcomes. Bridge
adapters should map it to the protocol `state` / `outcome` / `failure` shape at
the boundary instead of exporting internal enum values directly.

For `signal.submit`, clients should normally get results through
`signal.trades.query`, `trade.history.query` filters, or `trade.updated` events
that include `origin_signal`.

### `trade.history.query`

Return closed or persisted trade records. The payload should mirror
`TradeRecordQuery` and `TradeRecordFilter`.

Use one `selector` for direct lookup, or `filter` plus pagination for list
queries. If both are present, `selector` should narrow the query first and
`filter` should still be required to match the same record; bridges should not
silently choose priority between conflicting IDs.

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

Time ranges are half-open: `[start_ms, end_ms)`. Cursor pagination is preferred
over offset pagination because new records may arrive while a client pages
through history. Responses should include `next_cursor`, `has_more` and
`snapshot_at_ms` when pagination is used.

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
      "amount": {
        "value": "10.00",
        "currency": "USD"
      },
      "profit": {
        "value": "8.20",
        "currency": "USD"
      },
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

Cursors are opaque strings owned by the bridge implementation. Clients must not
parse or modify them. A cursor may encode the snapshot boundary, sort direction
and last seen key. A bridge may reject expired cursors with
`cursor_expired`.

### `trade.active.query`

Return trades that are not closed yet.

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
  "user_id": "broker-user-123",
  "platform_type": "INTRADE_BAR",
  "account_type": "DEMO",
  "currency": "USD",
  "balance": {
    "value": "1000.00",
    "currency": "USD"
  },
  "connected": true,
  "trade_enabled": true,
  "open_trades": 2,
  "max_trades": 10,
  "metadata": {}
}
```

`account_id` is the internal OptionX account identity used by routing and
events. `user_id` is the broker/platform account or trader identity, if the
platform exposes one.

### Trading Control

Commands:

- `trading.pause`
- `trading.resume`
- `trading.status`

Payload:

```json
{
  "scope": {
    "kind": "account",
    "account_id": "1",
    "platform_type": "INTRADE_BAR"
  },
  "reason": "manual_pause"
}
```

Use an explicit all-scope for global pause/resume:

```json
{
  "scope": {
    "kind": "all"
  },
  "reason": "manual_pause"
}
```

Missing or empty `scope` must be rejected as `invalid_params`; it must not pause
all trading accidentally.
