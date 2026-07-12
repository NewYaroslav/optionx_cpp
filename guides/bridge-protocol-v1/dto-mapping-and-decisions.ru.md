# Bridge Protocol v1 Draft - DTO Mapping И Решения

## Mapping To Existing DTOs

- `signal.submit` в первую очередь мапится на `TradeSignal`.
- `trade.open` мапится на executable trade intent. Public bridge APIs должны
  все равно сохранять тот же operation/report/origin lifecycle, что и
  `signal.submit`; только lower-level platform execution code должен обходить
  этот lifecycle и конвертировать напрямую в `TradeRequest`.
- `TradeSignal::assign_to_request()` мапит signal fields в `TradeRequest`.
- `trade.result.get` мапится на `TradeResult`.
- `trade.history.query` мапится на `TradeRecordQuery` и возвращает
  `TradeRecord`.
- `reports.*` мапится на `BridgeSignalReport` плюс будущие report DTOs.
- Account commands мапятся на `AccountInfoUpdate` / `BaseAccountInfoData`
  snapshots.

Важное текущее расхождение:

- `TradeSignal` содержит `platform_type`; `TradeRequest` не содержит.
- Поэтому platform/account routing должен оставаться protocol/application
  layer вокруг signal/request, а не принудительно встраиваться в `TradeRequest`.
- Это соответствует текущему intent: `TradeRequest` - platform/broker-level
  request object, а platform identity принадлежит routing, account data,
  platform instances и result/history snapshots.

## Зафиксированные Draft-Решения

- `TradeRequest` не должен получать `platform_type` ради public bridge protocol.
  Routing остается снаружи platform request object.
- Public JSON field names используют `lower_snake_case`; protocol-native enum
  values используют `lower_snake_case`; domain DTO enum values используют
  `UPPER_SNAKE_CASE` как canonical output, принимая и `UPPER_SNAKE_CASE`, и
  `lower_snake_case` на input.
- `trade.open` не должен пропускать public operation/report/origin lifecycle.
  Direct conversion to `TradeRequest` - lower-level execution concern.
- Один signal, создающий много trades, моделируется как один persisted
  signal/intake record плюс много trade records с `origin_signal`.
- `TradeRecordQuery` использует selector/filter payloads и opaque cursor
  pagination.
- Backtests обычно должны использовать отдельную database, выбранную application
  config, при этом protocol payloads все равно помечают
  `source.kind = "backtest"`.
- Большие MT4/MT5 raw buffers могут использовать advertised `binary_base64`
  compact encoding: little-endian `float64` values, little-endian `int64`
  timestamps и explicit typed arrays.
- Subscription reconnect - client behavior; replay - optional server behavior,
  объявляемый через capabilities и ограниченный configured retention.
- MVP auth - optional local API key, передаваемый через headers или initial auth
  command. Permissions должны стать scope-based, как только несколько clients
  делят один bridge.

## Открытые Вопросы

- Точная durable subscription storage model для event replay между restarts
  bridge.

## Перед Стабильным Wire v1

Следующие пункты полезны до того, как этот draft станет стабильным external
wire contract, но их не нужно реализовывать в текущем documentation PR:

- JSON Schema 2020-12 для command, response, event и shared-object payloads.
- OpenAPI binding для HTTP и AsyncAPI binding для WebSocket/event streams.
- Compatibility tests для schema evolution.
- Optional CloudEvents-inspired naming для event `source`, `type`, `subject` и
  `time`.
- Optional FIX/FIXP-inspired session details: durable sequence persistence,
  heartbeat, reconnect/resynchronization и backpressure policy.
- Compatibility tests для `binary_base64` compact buffer encoding.
