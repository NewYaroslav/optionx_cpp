# Bridge Protocol v1 Draft - Events, Reports И Replay

## Reports

Commands:

- `reports.query`

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

Subscriptions полезны для WebSocket и named-pipe clients. Обычные HTTP clients
должны опрашивать query/history commands со своей периодичностью.
Будущий HTTP streaming binding может экспортировать Server-Sent Events, но
обычный REST не доставляет live subscription stream.

Event subscription commands:

- `events.subscribe`
- `events.unsubscribe`
- `events.subscriptions.list`

`events.subscribe` доставляет existing domain events клиенту.
`market_data.subscribe` отличается: он создает или подключает concrete quote
stream, который затем может производить `market_data.tick` и
`market_data.bar` events. Reports доставляются через `events.subscribe` с topic
`report.created`.

Успешный `market_data.subscribe` по умолчанию доставляет свои tick/bar events
тому же client без отдельного вызова `events.subscribe`. Client также может
использовать `events.subscribe` для market-data topics, когда хочет наблюдать
уже созданные streams через general event bus. Если обе subscriptions совпали с
одним event на одном connection, bridge должен отправить один event и включить
`matched_event_subscription_ids`.

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

- `none`: не replay retained events.
- `last`: replay последних retained events, совпадающих с subscription filter,
  обычно ограниченный `count_per_topic`.
- `from_seq`: replay от известного `stream_id + seq`, когда event replay
  поддерживается.
- `from_token`: replay от opaque `resume_token`, когда bridge поддерживает
  resumable subscriptions.

Точные формы replay:

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

Когда replay запрошен и доступен, порядок событий такой:

```text
retained replay events
-> events.replay.completed
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

`events.subscribe` должен принимать `context.idempotency_key`,
`client_subscription_key` или оба поля, чтобы сетевой retry не создавал
дублирующиеся subscriptions. Для default session-scoped subscriptions retry
scope равен `authenticated client + session_id + method + key`; новая session
создает новую subscription. Durable subscriptions могут не включать
`session_id` в scope и повторно привязывать reconnecting client к существующей
subscription. Тот же key плюс тот же normalized request возвращает существующую
subscription в текущем scope; тот же key плюс другой request возвращает
`idempotency_conflict`, если не используется будущий явный subscription-update
contract. Повторный `events.unsubscribe` для уже удаленной subscription должен
быть успешным no-op.

Если одно event подходит сразу под несколько subscriptions на одном connection,
bridge должен отправить один event с одним `event_id` и включить все совпавшие
subscriptions:

```json
{
  "matched_event_subscription_ids": ["evt-sub-1", "evt-sub-7"]
}
```

Subscription resume - тема wire-contract, а не просто удобная фича. WebSocket
clients могут переподключаться каждые N секунд, пока их явно не остановили, но
само переподключение не гарантирует lossless event delivery.

Default draft behavior:

- Subscriptions являются connection/session scoped, если bridge явно не
  объявляет durable subscriptions.
- При `event_replay = false` delivery является best-effort внутри active
  connection. Во время disconnect возможна потеря; duplicates обычно не
  ожидаются, но clients должны их выдерживать.
- Ordering гарантирован только внутри одного `stream_id` и, для trade events,
  в порядке увеличения `revision`.
- Если `event_replay` равен false, reconnecting clients должны заново
  subscribe и принять, что events, произведенные во время disconnect, могут
  быть потеряны.
- Если `event_replay` равен true, clients могут передать
  `replay.mode = "from_seq"` со `stream_id + seq` или
  `replay.mode = "from_token"` с `resume_token`; bridge replay retained events
  из configured retention window. Это resumable best-effort replay, если bridge
  дополнительно не дает durable storage и checkpoints.

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

Durable replay с persisted checkpoints или acknowledgements - это режим,
который может дать at-least-once delivery. Duplicates остаются возможными, и
clients должны дедуплицировать по `source + event_id`.

Будущие stable versions должны определить:

- точное durable replay checkpoint storage
- acknowledgement semantics
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
- `events.replay.completed`
- `market_data.tick`
- `market_data.bar`
- `market_data.prefill.completed`
- `market_data.status`
- `market_data.report`

`trade.updated` и `trade.result.updated` должны нести полный result snapshot, а
не только patch, если event явно не объявляет `patch: true`.

`events.replay.completed` отмечает границу между retained replay events и live
events:

```json
{
  "event_subscription_id": "evt-sub-1",
  "stream_id": "bridge-instance-019c...",
  "last_replayed_seq": 1841,
  "live_from": {
    "stream_id": "bridge-instance-019c...",
    "seq": 1842
  }
}
```

Reports используют generic topic `report.created` с `payload.report_type`,
например `signal_report`; отдельный параллельный signal-specific report topic
для них не нужен.

Event delivery следует гарантии, объявленной capabilities: best-effort без
replay, resumable best-effort с retained replay, и at-least-once только когда
включены durable replay плюс checkpoint/ack semantics. Clients должны
дедуплицировать по `source + event_id` и использовать `stream_id + seq`, чтобы
обнаруживать gaps, когда transport поддерживает replay. Внутри одной trade
events должны emit в порядке увеличения `revision`.

`source` должен идентифицировать installation вместе с bridge instance,
например `optionx://installation-019c/bridge/2`, чтобы `source + event_id`
оставался устойчивым между несколькими local installations.
