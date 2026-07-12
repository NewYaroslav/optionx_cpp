# Bridge Protocol v1 Draft - Events, Reports And Replay

## Reports

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
    "expected_price": "1.14072",
    "actual_price": "1.14093",
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
    "market_data.tick",
    "market_data.bar",
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
WebSocket clients may reconnect every N seconds until explicitly stopped, but
reconnecting by itself does not guarantee lossless event delivery.

Default draft behavior:

- Subscriptions are connection/session scoped unless a bridge explicitly
  advertises durable subscriptions.
- Delivery is practically at-least-once. Clients must deduplicate by
  `source + event_id`.
- Ordering is guaranteed only inside one `stream_id` and, for trade events,
  within increasing `revision`.
- If `event_replay` is false, reconnecting clients must resubscribe and accept
  that events produced during disconnect may be lost.
- If `event_replay` is true, clients may pass `resume_from_seq` or a future
  `resume_token`; the bridge replays retained events from its configured
  retention window.

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

Future stable versions should define:

- `resume_from_seq`
- `resume_token`
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
- `market_data.status`
- `market_data.report`

`trade.updated` and `trade.result.updated` should carry the full result
snapshot, not only a patch, unless the event explicitly declares
`patch: true`.

Event delivery should be treated as practically at-least-once. Clients must
deduplicate by `source + event_id` and should use `stream_id + seq` to detect
gaps when a transport supports replay. Within one trade, events must be emitted
in increasing `revision` order.
