# Bridge Protocol v1 Draft

Status: draft. This document records the shared protocol direction for new
OptionX bridges. It is intentionally expected to change.

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

Every command uses the same envelope:

```json
{
  "v": 1,
  "id": "client-request-uuid",
  "op": "signal.submit",
  "ts_ms": 1783476705177,
  "payload": {}
}
```

Every command response uses:

```json
{
  "v": 1,
  "id": "client-request-uuid",
  "ok": true,
  "status": "accepted",
  "result": {}
}
```

Errors use:

```json
{
  "v": 1,
  "id": "client-request-uuid",
  "ok": false,
  "status": "rejected",
  "error": {
    "code": "invalid_payload",
    "message": "symbol is required",
    "details": {}
  }
}
```

Events use the same versioned envelope:

```json
{
  "v": 1,
  "event_id": "evt-123",
  "event": "trade.updated",
  "subscription_id": "sub-1",
  "ts_ms": 1783476705177,
  "payload": {}
}
```

Rules:

- `v` is required.
- `id` is required for commands and responses.
- `event_id` is required for events.
- Times in protocol fields use milliseconds since Unix epoch and end with
  `_ms`.
- Transport authentication must not be placed in `payload`.
- `metadata` is always a JSON object.
- Idempotency is represented by `idempotency_key` and/or `unique_hash`.

## Transport Binding

HTTP:

- `POST /api/v1/bridge/command` accepts one command envelope and returns one
  response envelope.
- `GET /api/v1/bridge/health` returns transport health.
- A future HTTP event stream may expose Server-Sent Events, but polling via
  query commands is enough for v1.

WebSocket:

- Commands and responses use the same envelopes on one socket.
- Events are pushed to subscribed clients.
- A response must repeat the command `id`.

Named Pipe:

- On message-oriented pipes, one pipe message is one UTF-8 JSON envelope.
- On stream-oriented transports, use newline-delimited JSON unless a bridge
  config explicitly selects length-prefixed frames.
- Subscribed events can be sent to connected pipe clients with the event
  envelope.

## Shared Objects

### Routing

`routing` describes account/platform selection before a signal becomes one or
more executable trade requests.

```json
{
  "account_id": 0,
  "account_ids": [1, 2],
  "platform_type": "INTRADE_BAR",
  "policy": "default"
}
```

`account_id = 0` means "not specified / bridge default". It does not mean
"all accounts".

Known policies:

- `default`: bridge/application default, usually account 0.
- `account_id`: use the specified account.
- `all`: duplicate to all matching accounts.
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
  "signal_id": 0,
  "trade_id": 0,
  "unique_id": 0,
  "unique_hash": "external-key",
  "idempotency_key": "client-stable-key",
  "signal_name": "rsi_cross",
  "comment": "optional user comment"
}
```

`idempotency_key` is a protocol-level key. Existing OptionX DTOs currently use
`unique_hash` and `unique_id`; bridges may map `idempotency_key` to
`unique_hash` when appropriate.

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
  "signal_id": 101,
  "bridge_id": 2,
  "unique_id": 0,
  "unique_hash": "tv:abc123",
  "signal_name": "noisy_rsi_test"
}
```

A single `signal.submit` may produce zero, one or many trades. Therefore
trade-result commands and events should identify trades directly, but also
include `origin_signal` for correlation.

## Commands

### `trade.open`

Use this when the external client wants a concrete trade with explicit trade
parameters.

```json
{
  "routing": {
    "account_id": 0,
    "platform_type": "INTRADE_BAR",
    "policy": "default"
  },
  "trade": {
    "symbol": "EURUSD",
    "order_type": "BUY",
    "option_type": "SPRINT",
    "amount": 10.0,
    "duration_sec": 60,
    "expiry_time_sec": 0,
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
  "accepted": true,
  "trade_refs": [
    {
      "trade_id": 123,
      "account_id": 0,
      "platform_type": "INTRADE_BAR",
      "unique_hash": "client-trade-1"
    }
  ]
}
```

Even `trade.open` can produce more than one trade when routing policy is `all`.

### `signal.submit`

Use this when the external client submits a strategy signal. Money management,
routing and filters may later produce zero, one or many trades.

```json
{
  "routing": {
    "policy": "best_payout",
    "account_ids": [1, 2, 3]
  },
  "signal": {
    "symbol": "EURUSD",
    "order_type": "SELL",
    "option_type": "SPRINT",
    "duration_sec": 60,
    "expiry_time_sec": 0,
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
  "accepted": true,
  "signal_ref": {
    "signal_id": 101,
    "unique_hash": "tv:abc123",
    "signal_name": "noisy_rsi_test"
  },
  "trade_refs": []
}
```

`trade_refs` may be empty because signal processing can be asynchronous or
because risk/decision logic rejected the signal. Clients should use
`signal.trades.query` or subscriptions to observe produced trades.

### `signal.trades.query`

Return trades related to a signal.

```json
{
  "signal_id": 101,
  "unique_hash": "tv:abc123",
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
    "terminal": "MT5",
    "indicator": "SuperArrow",
    "symbol": "EURUSD",
    "timeframe_sec": 60,
    "observed_at_ms": 1783476705177
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
      "values": [0.0, 1.14072, 0.0, 0.0, 0.0]
    },
    {
      "name": "sell",
      "role": "sell",
      "empty_policy": "zero_or_empty_value",
      "empty_value": 2147483647.0,
      "values": [0.0, 0.0, 0.0, 0.0, 0.0]
    }
  ],
  "bars": [
    {
      "index": 0,
      "time_ms": 1783476720000,
      "open": 1.14060,
      "high": 1.14070,
      "low": 1.14055,
      "close": 1.14066,
      "volume": 76.0
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
    "duration_sec": 60,
    "min_payout": 0.75
  },
  "result": {
    "trade_state": "WIN",
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
separate database. A future `PlatformType::SIMULATOR` is still useful for live
simulation on real-time prices, for example "run for a week without real
trades".

### `trade.result.get`

Return the current result snapshot for one trade. This is useful for
intermediate states as well as final results.

```json
{
  "trade_id": 123,
  "broker_option_id": 456,
  "broker_option_hash": "abc",
  "unique_hash": "client-trade-1"
}
```

Result shape should mirror `TradeResult` as closely as possible:

```json
{
  "trade_id": 123,
  "trade_state": "OPENED",
  "live_state": "UNKNOWN",
  "error_code": "SUCCESS",
  "error_desc": "",
  "broker_option_id": 456,
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
    "stop_ms": 1783600000000,
    "field": "close_time"
  },
  "filter": {
    "account_id": 0,
    "platform_type": "INTRADE_BAR",
    "symbol": "EURUSD",
    "signal_name": "rsi_cross",
    "trade_state": ["WIN", "LOSS"],
    "unique_hash": ""
  },
  "limit": 1000,
  "offset": 0
}
```

### `trade.active.query`

Return trades that are not closed yet.

```json
{
  "account_id": 0,
  "platform_type": "INTRADE_BAR",
  "symbol": "EURUSD"
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
  "account_id": 1,
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
    "account_id": 1,
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
  "account_id": 1,
  "platform_type": "INTRADE_BAR",
  "trade_ref": {
    "trade_id": 123,
    "broker_option_id": 456
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
    "account_id": 1,
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
- Whether `trade.open` should bypass `TradeSignal` in future bridge APIs.
- How to model one signal producing multiple trades in persistent storage.
- Exact `TradeRecordQuery` JSON shape and pagination contract.
- Whether `PlatformType::SIMULATOR` should be added.
- Where to store backtest data: separate DB by application config is preferred,
  but protocol must still mark `source.kind = "backtest"`.
- Compact raw-buffer encoding for MT4/MT5 when bars/buffers become large.
- Subscription replay semantics and delivery guarantees.
- Auth, permissions and capability discovery per transport.
