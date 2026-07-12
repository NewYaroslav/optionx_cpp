# Bridge Protocol v1 Draft - Events, Reports И Replay

## Reports

Commands:

- `reports.query`
- `reports.subscribe`

Report categories:

- `signal_report`: accepted, rejected, invalid, duplicate, intake error.
- `trade_report`: queued, placed, opened, closed, failed.
- `routing_report`: выбранный account/platform и причина выбора.
- `risk_report`: money/risk-management decision или rejection.
- `account_report`: balance/status/account changes.
- `bridge_report`: transport/client/server lifecycle.
- `backtest_report`: import, simulation или stale-history issues.
- `market_data_report`: subscription, replay, gap и continuity issues.
- `broker_quality_report`: slippage, tick mismatch, artificial delay,
  changed payout, quote gap, suspicious execution.

Пример:

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

Subscriptions полезны для WebSocket и named-pipe clients. HTTP clients могут
использовать query commands или будущий event stream.

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

Subscription resume - тема wire-contract, а не просто удобная фича. WebSocket
clients могут переподключаться каждые N секунд, пока их явно не остановили, но
само переподключение не гарантирует lossless event delivery.

Default draft behavior:

- Subscriptions являются connection/session scoped, если bridge явно не
  объявляет durable subscriptions.
- Delivery практически at-least-once. Clients должны дедуплицировать по
  `source + event_id`.
- Ordering гарантирован только внутри одного `stream_id` и, для trade events,
  в порядке увеличения `revision`.
- Если `event_replay` равен false, reconnecting clients должны заново
  subscribe и принять, что events, произведенные во время disconnect, могут
  быть потеряны.
- Если `event_replay` равен true, clients могут передать `resume_from_seq` или
  future `resume_token`; bridge replay retained events из configured retention
  window.

Durable replay между restarts bridge - более сильная гарантия. Она означает,
что bridge сохраняет event envelopes и replay checkpoints в storage, поэтому
client может reconnect после crash или restart процесса bridge и запросить
events после последнего увиденного `stream_id + seq` или `resume_token`. Без
durable storage replay является только in-memory convenience, пока процесс
bridge жив.

Если bridge объявляет durable replay, минимальная storage model - append-only
event log:

- key: `stream_id + seq`
- identity: `source + event_id`
- timestamps: `occurred_at_ms` и `emitted_at_ms`
- subject: trade/signal/account/subscription reference
- payload: full event snapshot
- retention: time-based и/или count-based pruning

Subscriber checkpoints могут храниться по authenticated client, subscription
или explicit `resume_token`. Точная production model пока открыта, потому что
она зависит от того, должен ли replay переживать только reconnects, full bridge
restarts или application/database migrations.

Будущие stable versions должны определить:

- `resume_from_seq`
- `resume_token`
- event retention window
- heartbeat interval
- idle timeout
- max queued events per subscriber
- backpressure policy

## Events

Предлагаемые event names:

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

`trade.updated` и `trade.result.updated` должны нести полный result snapshot, а
не только patch, если event явно не объявляет `patch: true`.

Event delivery следует считать практически at-least-once. Clients должны
дедуплицировать по `source + event_id` и использовать `stream_id + seq`, чтобы
обнаруживать gaps, когда transport поддерживает replay. Внутри одной trade
events должны emit в порядке увеличения `revision`.
