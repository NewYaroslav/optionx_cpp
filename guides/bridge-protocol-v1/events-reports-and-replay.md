# Bridge Protocol v1 Draft - Events, Reports And Replay

## Reports

Commands:

- `reports.query`

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
    "expected_price": "1.14072",
    "actual_price": "1.14093",
    "ping_ms": 30,
    "configured_extra_delay_ms": 250
  }
}
```

## Subscriptions

Subscriptions are useful for WebSocket and named-pipe clients. Plain HTTP
clients should poll query/history commands with their own interval. A
future HTTP streaming binding may expose Server-Sent Events, but ordinary REST
does not deliver a live subscription stream.

Event subscription commands:

- `events.subscribe`
- `events.unsubscribe`
- `events.subscriptions.list`

`events.subscribe` delivers existing domain events to the client.
`market_data.subscribe` is different: it creates or attaches to a concrete quote
stream, which may then produce `market_data.tick` and `market_data.bar` events.
Reports are delivered through `events.subscribe` with the `report.created`
topic.

By default, a successful `market_data.subscribe` delivers its tick/bar events to
the same client without requiring a separate `events.subscribe` call. A client
may also use `events.subscribe` for market-data topics when it wants to observe
already-created streams through the general event bus. If both subscriptions
match the same event on the same connection, the bridge should send one event
and include the matched event subscription ids.

`events.subscribe` request:

```json
{
  "context": {
    "idempotency_key": "events:main-feed"
  },
  "client_subscription_key": "main-feed",
  "topics": [
    "trade.updated",
    "trade.result.updated",
    "account.updated",
    "balance.updated",
    "platform.status",
    "bridge.status",
    "report.created",
    "market_data.tick",
    "market_data.bar",
    "market_data.status"
  ],
  "filter": {
    "account_id": "1",
    "platform_type": "INTRADE_BAR",
    "symbol": "EURUSD"
  },
  "replay": {
    "mode": "last",
    "count_per_topic": 1
  }
}
```

Known `replay.mode` values:

- `none`: do not replay retained events.
- `last`: replay the last retained events matching the subscription filter,
  usually bounded by `count_per_topic`.
- `from_seq`: replay from a known `stream_id + seq` when event replay is
  supported.
- `from_token`: replay from an opaque `resume_token` when the bridge supports
  resumable subscriptions.

Exact replay forms:

```json
{
  "replay": {
    "mode": "from_seq",
    "stream_id": "bridge-instance-019c...",
    "seq": 1841
  }
}
```

```json
{
  "replay": {
    "mode": "from_token",
    "resume_token": "opaque-token"
  }
}
```

When replay is requested and available, the event order is:

```text
retained replay events
-> replay.completed control notification
-> live events
```

Response:

```json
{
  "event_subscription_id": "evt-sub-1",
  "topics": ["trade.updated", "report.created"]
}
```

Unsubscribe:

```json
{
  "event_subscription_id": "evt-sub-1"
}
```

`events.subscribe` should accept either `context.idempotency_key`,
`client_subscription_key`, or both, so a network retry does not create duplicate
subscriptions. For default session-scoped subscriptions, the retry scope is
`authenticated client + session_id + method + key`; a new session creates a new
subscription. Durable subscriptions may omit `session_id` from the scope and
reattach a reconnecting client to an existing subscription. The same key plus
the same normalized request returns the existing subscription in the current
scope; the same key plus a different request returns `idempotency_conflict`
unless a future explicit subscription-update contract is used. Repeating
`events.unsubscribe` for an already removed subscription should be a successful
no-op.

If one event matches several subscriptions on the same connection, the bridge
should send one event with one `event_id` and include all matched subscriptions:

```json
{
  "matched_event_subscription_ids": ["evt-sub-1", "evt-sub-7"]
}
```

Subscription resume is a wire-contract topic, not only a convenience feature.
WebSocket clients may reconnect every N seconds until explicitly stopped, but
reconnecting by itself does not guarantee lossless event delivery.

Default draft behavior:

- Subscriptions are connection/session scoped unless a bridge explicitly
  advertises durable subscriptions.
- With `event_replay = false`, delivery is best-effort within the active
  connection. Loss is possible during disconnects; duplicates are generally not
  expected but clients should tolerate them.
- Ordering is guaranteed only inside one `stream_id` and, for trade events,
  within increasing `revision`.
- If `event_replay` is false, reconnecting clients must resubscribe and accept
  that events produced during disconnect may be lost.
- If `event_replay` is true, clients may pass `replay.mode = "from_seq"` with
  `stream_id + seq`, or `replay.mode = "from_token"` with `resume_token`; the
  bridge replays retained events from its configured retention window. This is
  resumable best-effort replay unless the bridge also provides durable storage
  and checkpoints.

Durable replay across bridge restarts is a stronger guarantee. It means the
bridge persists event envelopes and replay checkpoints to storage, so a client
can reconnect after the bridge process crashed or was restarted and still ask
for events after the last observed `stream_id + seq` or `resume_token`.
Without durable storage, replay is only an in-memory convenience while the
bridge process is alive.

If a bridge advertises durable replay, the minimal storage model is an
append-only event log:

- key: `stream_id + seq`
- identity: `source + event_id`
- timestamps: `occurred_at_ms` and `emitted_at_ms`
- subject: trade/signal/account/subscription reference
- payload: full event snapshot
- retention: time-based and/or count-based pruning

Subscriber checkpoints may be stored by authenticated client, subscription or
explicit `resume_token`. The exact production model is still open because it
depends on whether replay should survive only reconnects, full bridge restarts,
or application/database migrations.

Durable replay with persisted checkpoints or acknowledgements is the mode that
can provide at-least-once delivery. Duplicates remain possible and clients must
deduplicate by `source + event_id`.

Future stable versions should define:

- exact durable replay checkpoint storage
- acknowledgement semantics
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
- `market_data.tick`
- `market_data.bar`
- `market_data.prefill.completed`
- `market_data.status`
- `market_data.report`

`trade.updated` and `trade.result.updated` should carry the full result
snapshot, not only a patch, unless the event explicitly declares
`patch: true`.

`replay.completed` is a control notification, not a domain event-log entry. It
does not consume an event stream `seq` and does not need to be persisted in the
event log. It marks the boundary between replayed retained events and live
events:

```json
{
  "event_subscription_id": "evt-sub-1",
  "last_replayed_seq": 1841,
  "live_from": {
    "stream_id": "bridge-instance-019c...",
    "seq": 1842
  }
}
```

Reports use the generic `report.created` topic with `payload.report_type`, for
example `signal_report`; they should not need a parallel signal-specific report
topic.

Event delivery follows the guarantee advertised by capabilities: best-effort
without replay,
resumable best-effort with retained replay, and at-least-once only when durable
replay plus checkpoint/ack semantics are enabled. Clients should deduplicate by
`source + event_id` and use `stream_id + seq` to detect gaps when a transport
supports replay. Within one trade, events must be emitted in increasing
`revision` order.

`source` should identify the installation as well as the bridge instance, for
example `optionx://installation-019c/bridge/2`, so `source + event_id` remains
stable across multiple local installations.
