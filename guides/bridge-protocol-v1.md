# Bridge Protocol Draft

Status: domain-protocol draft. This document records the shared protocol
direction for new OptionX bridges. It is intentionally expected to change and
must not yet be treated as a stable wire-level v1 contract.

This draft describes the business commands, events and shared objects that
HTTP, WebSocket and named-pipe bridges should expose. The recommended wire
envelope for commands and responses is JSON-RPC 2.0, because the future
`mgc-platform` integration already uses JSON-RPC surfaces and benefits from the
same request, response and error semantics.

The protocol is transport-independent. HTTP, WebSocket and named-pipe bridges
should expose the same command, response and event payloads. Transport details
such as HTTP headers, WebSocket authentication and pipe framing are outside the
business payload.

## Goals

- Accept explicit trade requests from external tools.
- Accept strategy signals that may later produce zero, one or many trades.
- Support money/risk-management delegation instead of requiring every signal to
  carry a final amount.
- Support MT4/MT5 indicator-buffer input where signal interpretation is moved
  from MQL into the bridge.
- Import historical test results when an external tester already knows the
  result.
- Query trade results, trade history, active trades, accounts, balances and
  platform status.
- Stream reports and state changes for WebSocket and named-pipe clients, while
  still supporting query-style access for HTTP clients.
- Keep the bridge as an adapter between external systems and existing OptionX
  DTOs, not a second trading platform implementation.

## Non-Goals For v1

- Building a full historical simulator.
- Storing or replaying tick/bar history inside the protocol layer.
- Encoding broker-specific authentication flows in command payloads.
- Defining a visual node/blueprint runtime. The protocol should leave room for
  that layer, but not depend on it.

## Envelope

Commands use JSON-RPC 2.0 as the outer envelope:

```json
{
  "jsonrpc": "2.0",
  "id": "client-request-uuid",
  "method": "signal.submit",
  "params": {
    "protocol_version": "draft",
    "context": {
      "idempotency_key": "tv:abc123",
      "client_created_at_ms": 1783476705177,
      "valid_until_ms": 1783476710000
    }
  }
}
```

Command responses use JSON-RPC `result` for both accepted and domain-rejected
business outcomes:

```json
{
  "jsonrpc": "2.0",
  "id": "client-request-uuid",
  "result": {
    "status": "accepted",
    "final": false,
    "operation_id": "op-019c..."
  }
}
```

Protocol errors use JSON-RPC `error`. Use this only when the request cannot be
processed as a valid protocol message: parse error, method not found, invalid
params, authorization failure or internal server error.

```json
{
  "jsonrpc": "2.0",
  "id": "client-request-uuid",
  "error": {
    "code": -32602,
    "message": "symbol is required",
    "data": {}
  }
}
```

Business rejections should remain a normal `result`, not a JSON-RPC `error`:

```json
{
  "jsonrpc": "2.0",
  "id": "client-request-uuid",
  "result": {
    "status": "rejected",
    "final": true,
    "reason": {
      "code": "risk_limit",
      "message": "Maximum exposure exceeded"
    }
  }
}
```

Events are JSON-RPC notifications: they have `jsonrpc`, `method` and `params`,
but no `id`.

```json
{
  "jsonrpc": "2.0",
  "method": "trade.updated",
  "params": {
    "protocol_version": "draft",
    "event_id": "evt-019c...",
    "source": "optionx://bridge/2",
    "stream_id": "bridge-instance-019c...",
    "seq": 1842,
    "subscription_id": "sub-1",
    "occurred_at_ms": 1783476720120,
    "emitted_at_ms": 1783476720145,
    "subject": {
      "trade_id": "123"
    },
    "revision": 7,
    "payload": {}
  }
}
```

Rules:

- `jsonrpc` must be `"2.0"` for JSON-RPC transports.
- `id` identifies one RPC call. It is not a business identifier.
- `params.protocol_version` is required for bridge protocol messages.
- Unknown optional response/event fields must be ignored by clients.
- Unknown fields in trading command objects should be rejected unless they live
  under `metadata` or an explicitly namespaced `extensions` object.
- Times in protocol fields use milliseconds since Unix epoch and end with
  `_ms`.
- Transport authentication must not be placed in `params`.
- `metadata` is always a JSON object.

### Identifiers

The protocol uses three independent identifier families:

- `id`: JSON-RPC request identifier. A retry may use a new `id`.
- `idempotency_key`: stable logical-operation key. A retry of the same logical
  command must reuse it.
- `unique_hash`: domain-level signal/trade deduplication key, often supplied by
  TradingView, MT4/MT5 or another external signal source.

`idempotency_key` rules:

- Uniqueness scope is `authenticated client + method`.
- Same key plus semantically identical payload should return the original
  result while the key is retained.
- Same key plus different payload should return `idempotency_conflict`.
- A bridge must document the retention window for idempotency keys.
- For fan-out commands, the key applies to the whole logical fan-out operation,
  not to each target account independently.

`unique_hash` is not a transport retry mechanism. Two different transports may
deliver the same market signal with different RPC `id` and different
`idempotency_key`, while the same `unique_hash` still lets the domain layer
deduplicate the signal.

External identifiers are opaque strings in the wire protocol, even when the
current C++ DTO stores them as integers. This keeps the contract stable for
JavaScript clients, broker IDs, UUIDs, composite IDs and future wider integer
types. Implementations may parse locally generated numeric strings back into
integer DTO fields at the adapter boundary.

## Transport Binding

HTTP:

- `POST /api/v1/bridge/command` accepts one JSON-RPC command and returns one
  JSON-RPC response.
- `GET /api/v1/bridge/health` returns transport health.
- A future HTTP event stream may expose Server-Sent Events, but polling via
  query commands is enough for v1.

WebSocket:

- Commands and responses use JSON-RPC on one socket.
- Events are pushed to subscribed clients as JSON-RPC notifications.
- A response must repeat the command `id`.
- WebSocket bridges should use the subprotocol
  `Sec-WebSocket-Protocol: optionx.bridge.v1` once the wire contract is stable.

Named Pipe:

- On message-oriented pipes, one pipe message is one UTF-8 JSON-RPC document.
- On stream-oriented transports, use newline-delimited JSON unless a bridge
  config explicitly selects length-prefixed frames.
- Newline-delimited JSON requires compact JSON without embedded line breaks.
  Length-prefixed framing is the safer future default for binary-safe
  integrations.
- Subscribed events can be sent to connected pipe clients as JSON-RPC
  notifications.

## Shared Objects

### Routing

`routing` describes account/platform selection before a signal becomes one or
more executable trade requests.

```json
{
  "selector": {
    "kind": "default"
  },
  "platform_type": "INTRADE_BAR"
}
```

Do not use `0` or an empty string as "not specified" in the external protocol.
Omit the field or use an explicit selector.

Selector examples:

```json
{ "kind": "default" }
```

```json
{ "kind": "account", "account_id": "1" }
```

```json
{ "kind": "accounts", "account_ids": ["1", "2"] }
```

```json
{ "kind": "all" }
```

Known selector kinds:

- `default`: bridge/application default account/platform.
- `account`: use one specified account.
- `accounts`: use a specified candidate list.
- `all`: duplicate to all matching accounts.

Known policies:

- `best_payout`: choose the account/platform with the best payout.
- `first_available`: choose the first account that can accept the trade.
- `round_robin`: rotate through candidate accounts.
- `random`: choose a random candidate account.

MVP should implement only `default`, `account_id` and optionally `all`. More
advanced policies may belong to risk management or a future node/blueprint
layer instead of the bridge itself.

### Identity

```json
{
  "signal_id": "101",
  "trade_id": "123",
  "unique_id": "0",
  "unique_hash": "external-key",
  "idempotency_key": "client-stable-key",
  "signal_name": "rsi_cross",
  "comment": "optional user comment"
}
```

Existing OptionX DTOs currently use numeric IDs plus `unique_hash` and
`unique_id`. Bridges may convert opaque protocol strings to local numeric DTO
fields when the value is locally generated and representable.

### Expiry

Use one explicit expiry form instead of parallel `duration_sec` and
`expiry_time_sec` fields.

```json
{
  "kind": "duration",
  "duration_ms": 60000
}
```

```json
{
  "kind": "absolute",
  "expires_at_ms": 1783476780000
}
```

Exactly one expiry form should be present. Trading commands should also include
`context.valid_until_ms` when stale execution would be harmful.

### Sizing

```json
{
  "mode": "fixed_amount",
  "amount": 10.0,
  "balance_percent": 2.5,
  "system": "kelly",
  "params": {}
}
```

Known modes:

- `fixed_amount`: explicit amount.
- `balance_percent`: amount is derived from account balance.
- `risk_manager`: downstream risk manager decides amount.
- `ignore_signal_amount`: payload amount is intentionally ignored.
- `none`: no sizing instruction.

Known systems are open-ended: `kelly`, `martingale`, `anti_martingale`,
`labouchere`, custom names, etc. Typed C++ `IMoneyManagementParams` can be
restored by higher-level code; protocol payloads carry JSON params.

### Origin Signal

Trades produced by signals should carry an origin block so clients can query
and stream all trades related to one signal.

```json
{
  "signal_id": "101",
  "bridge_id": "2",
  "unique_id": "0",
  "unique_hash": "tv:abc123",
  "signal_name": "noisy_rsi_test"
}
```

A single `signal.submit` may produce zero, one or many trades. Therefore
trade-result commands and events should identify trades directly, but also
include `origin_signal` for correlation.

## Commands

Command examples below show the JSON-RPC `params` object for the named method,
not the full outer JSON-RPC envelope.

### `protocol.hello`

Return basic server identity and selected protocol version. This is useful for
clients that can speak to multiple bridge implementations.

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

Return supported methods, feature flags and limits.

```json
{
  "protocol_versions": ["draft"],
  "server_instance_id": "019c...",
  "supported_methods": [
    "signal.submit",
    "trade.open",
    "trade.result.get"
  ],
  "features": {
    "subscriptions": true,
    "event_replay": false,
    "trade_open_batch": false,
    "routing_all": true,
    "buffer_encoding": ["json"]
  },
  "limits": {
    "max_message_bytes": 1048576,
    "max_buffers": 32,
    "max_buffer_values": 1000,
    "max_page_size": 1000
  }
}
```

### `trade.open`

Use this when the external client wants a concrete trade with explicit trade
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
    "amount": 10.0,
    "expiry": {
      "kind": "duration",
      "duration_ms": 60000
    },
    "min_payout": 0.75,
    "refund": 0.0
  },
  "identity": {
    "unique_hash": "client-trade-1",
    "signal_name": "manual"
  },
  "metadata": {
    "rsi": 27.4,
    "price": 1.14072
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

### `signal.submit`

Use this when the external client submits a strategy signal. Money management,
routing and filters may later produce zero, one or many trades.

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
    "min_payout": 0.75
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
    "rsi": 72.1,
    "price": 1.14072
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

### Operation Lifecycle

Long-running commands return an `operation_id`. This separates synchronous
command acceptance from later broker/risk-management processing.

Known operation states:

- `accepted`: command accepted for processing.
- `processing`: bridge/application is working on it.
- `completed`: all targets completed successfully.
- `partially_completed`: some fan-out targets succeeded and some failed.
- `rejected`: valid command rejected by business logic.
- `failed`: processing failed unexpectedly.
- `cancelled`: operation was cancelled before completion.

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
      "empty_value": 2147483647.0,
      "values": [0.0, 1.14072]
    },
    {
      "name": "sell",
      "role": "sell",
      "empty_policy": "zero_or_empty_value",
      "empty_value": 2147483647.0,
      "values": [0.0, 0.0]
    }
  ],
  "bars": [
    {
      "index": 0,
      "open_time_ms": 1783476720000,
      "is_closed": false,
      "open": 1.14060,
      "high": 1.14070,
      "low": 1.14055,
      "close": 1.14066,
      "volume": 76.0
    },
    {
      "index": 1,
      "open_time_ms": 1783476660000,
      "is_closed": true,
      "open": 1.14048,
      "high": 1.14072,
      "low": 1.14044,
      "close": 1.14060,
      "volume": 93.0
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

Notes:

- `values` should contain only the last N bars, not the whole terminal history.
- `empty_value` exists because MQL indicators often use a special EMPTY_VALUE
  constant.
- `empty_policy` is necessary because many user indicators use `0` as no data.
- All `buffers[].values` arrays must have the same length.
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

### `backtest.result.import`

Use this when an external tester has already calculated the trade result.

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
    "amount": 10.0,
    "expiry": {
      "kind": "duration",
      "duration_ms": 60000
    },
    "min_payout": 0.75
  },
  "result": {
    "state": "closed",
    "outcome": "win",
    "final": true,
    "revision": 1,
    "payout": 0.82,
    "profit": 8.2,
    "open_price": 1.14072,
    "close_price": 1.14120,
    "open_time_ms": 1783476720000,
    "close_time_ms": 1783476780000,
    "spread": {
      "open": 0.0001,
      "close": 0.0001,
      "unit": "price"
    }
  },
  "metadata": {}
}
```

`source.kind = "backtest"` should be kept even when the physical storage is a
separate database. `PlatformType::SIMULATOR` already exists in the C++ domain
model and is useful for live simulation on real-time prices, for example "run
for a week without real trades".

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
  "amount": 10.0,
  "payout": 0.82,
  "profit": 0.0,
  "balance": 1000.0,
  "open_balance": 1000.0,
  "close_balance": 0.0,
  "open_price": 1.14072,
  "close_price": 0.0,
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
  "platform_type": "INTRADE_BAR",
  "account_type": "DEMO",
  "currency": "USD",
  "balance": 1000.0,
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

An empty `scope` means all accounts/platforms.

### Reports

Commands:

- `reports.query`
- `reports.subscribe`

Report categories:

- `signal_report`: accepted, rejected, invalid, duplicate, intake error.
- `trade_report`: queued, placed, opened, closed, failed.
- `routing_report`: selected account/platform and selection reason.
- `risk_report`: money/risk-management decision or rejection.
- `account_report`: balance/status/account changes.
- `bridge_report`: transport/client/server lifecycle.
- `backtest_report`: import, simulation or stale-history issues.
- `market_data_report`: subscription, replay, gap and continuity issues.
- `broker_quality_report`: slippage, tick mismatch, artificial delay,
  changed payout, quote gap, suspicious execution.

Example:

```json
{
  "report_type": "broker_quality_report",
  "status": "warning",
  "reason_code": "execution_slippage",
  "message": "Open price differs from expected tick by 2.1 points.",
  "account_id": "1",
  "platform_type": "INTRADE_BAR",
  "trade_ref": {
    "trade_id": "123",
    "broker_option_id": "456"
  },
  "origin_signal": {},
  "context": {
    "expected_price": 1.14072,
    "actual_price": 1.14093,
    "ping_ms": 30,
    "configured_extra_delay_ms": 250
  }
}
```

## Subscriptions

Subscriptions are useful for WebSocket and named-pipe clients. HTTP clients can
use query commands or a future event stream.

Command:

```json
{
  "topics": [
    "trade.updated",
    "trade.result.updated",
    "signal.report",
    "account.updated",
    "balance.updated",
    "platform.status",
    "bridge.status",
    "report.created",
    "market_data.status"
  ],
  "filter": {
    "account_id": "1",
    "platform_type": "INTRADE_BAR",
    "symbol": "EURUSD"
  },
  "replay_last": true
}
```

Response:

```json
{
  "subscription_id": "sub-1",
  "topics": ["trade.updated", "report.created"]
}
```

Unsubscribe:

```json
{
  "subscription_id": "sub-1"
}
```

Subscription resume is a wire-contract topic, not only a convenience feature.
Future stable versions should define:

- `resume_from_seq`
- event retention window
- heartbeat interval
- idle timeout
- max queued events per subscriber
- backpressure policy

## Events

Suggested event names:

- `signal.accepted`
- `signal.rejected`
- `trade.queued`
- `trade.opened`
- `trade.updated`
- `trade.closed`
- `trade.failed`
- `trade.result.updated`
- `account.updated`
- `balance.updated`
- `platform.status`
- `trading.status`
- `bridge.status`
- `report.created`

`trade.updated` and `trade.result.updated` should carry the full result
snapshot, not only a patch, unless the event explicitly declares
`patch: true`.

Event delivery should be treated as practically at-least-once. Clients must
deduplicate by `source + event_id` and should use `stream_id + seq` to detect
gaps when a transport supports replay. Within one trade, events must be emitted
in increasing `revision` order.

## Mapping To Existing DTOs

- `signal.submit` maps primarily to `TradeSignal`.
- `trade.open` maps to an executable trade intent. The bridge may still emit a
  `TradeSignal` first because the current bridge callback API is signal-based.
- `TradeSignal::assign_to_request()` maps signal fields to `TradeRequest`.
- `trade.result.get` maps to `TradeResult`.
- `trade.history.query` maps to `TradeRecordQuery` and returns `TradeRecord`.
- `reports.*` maps to `BridgeSignalReport` plus future report DTOs.
- Account commands map to `AccountInfoUpdate` / `BaseAccountInfoData` snapshots.

Important current mismatch:

- `TradeSignal` has `platform_type`; `TradeRequest` does not.
- Therefore platform/account routing should remain a protocol/application layer
  around the signal/request instead of being forced into `TradeRequest`.

## Open Questions

- Should `TradeRequest` grow `platform_type`, or should routing always live
  outside it?
- Exact enum/string spelling for public JSON fields.
- Whether `trade.open` should bypass `TradeSignal` in future bridge APIs or
  only do so in a lower-level execution component.
- How to model one signal producing multiple trades in persistent storage.
- Exact `TradeRecordQuery` JSON shape and cursor implementation.
- Where to store backtest data: separate DB by application config is preferred,
  but protocol must still mark `source.kind = "backtest"`.
- Compact raw-buffer encoding for MT4/MT5 when bars/buffers become large.
- Exact subscription replay retention, persistence and reconnect behavior.
- Auth and permissions per transport.

## Before Stable Wire v1

The following items are useful before this draft becomes a stable external wire
contract, but do not need to be implemented in the current documentation PR:

- JSON Schema 2020-12 for command, response, event and shared-object payloads.
- OpenAPI binding for HTTP and AsyncAPI binding for WebSocket/event streams.
- Compatibility tests for schema evolution.
- Optional CloudEvents-inspired naming for event `source`, `type`, `subject`
  and `time`.
- Optional FIX/FIXP-inspired session details: durable sequence persistence,
  heartbeat, reconnect/resynchronization and backpressure policy.
- Binary or compact buffer encoding for large MT4/MT5 payloads.
