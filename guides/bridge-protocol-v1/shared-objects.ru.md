# Bridge Protocol v1 Draft - Общие Объекты

## Общие Объекты

### Routing

`routing` описывает выбор account/platform до того, как signal превращается в
один или несколько executable trade requests.

```json
{
  "selector": {
    "kind": "default"
  },
  "platform_type": "INTRADE_BAR"
}
```

Не использовать `0` или пустую строку как "not specified" во внешнем протоколе.
Поле нужно опускать или использовать явный selector.

Примеры selector:

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

Известные selector kinds:

- `default`: default account/platform на стороне bridge/application.
- `account`: использовать один указанный account.
- `accounts`: использовать указанный список candidates.
- `all`: дублировать на все подходящие accounts.

Известные policies:

- `best_payout`: выбрать account/platform с лучшим payout.
- `first_available`: выбрать первый account, который может принять сделку.
- `round_robin`: циклически перебирать candidate accounts.
- `random`: выбрать случайный candidate account.

MVP должен реализовать только `default`, `account` и опционально `all`.
Более продвинутые policies могут принадлежать risk management или будущему
node/blueprint layer вместо самого bridge.

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

Существующие OptionX DTO сейчас используют numeric IDs плюс `unique_hash` и
`unique_id`. Bridges могут конвертировать opaque protocol strings в локальные
numeric DTO fields, если значение локально сгенерировано и представимо.

### Expiry

Использовать одну явную форму expiry вместо параллельных `duration_sec` и
`expiry_time_sec`.

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

Должна присутствовать ровно одна форма expiry. Trading commands также должны
содержать `context.valid_until_ms`, если stale execution опасен.

### Decimal Values

Деньги, prices, payouts, refunds, percentages и indicator numeric values требуют
decimal precision. Канонические responses и events должны отдавать decimal
values как base-10 strings. Requests могут принимать decimal strings или JSON
numbers как developer-friendly input form, но bridge implementations должны
нормализовать их в decimal representation до validation и storage.

Правила canonical decimal string:

- Использовать точку как decimal separator.
- Не использовать scientific notation.
- Сохранять sign, где он имеет смысл, например profit может быть `"-10.00"`.
- Сохранять meaningful scale, когда он известен, например `"10.00"` для USD
  cents.
- Использовать явные units или field semantics вместо зависимости от
  formatting.

Примеры:

```json
{
  "amount": "10.00",
  "price": "1.14072",
  "profit": "-10.00",
  "balance_percent": "2.5",
  "payout": "0.82"
}
```

Заметки:

- Clients, которым нужны точные decimal value и scale, должны отправлять
  decimal strings. JSON numbers принимаются только как удобство для простых
  integrations и могут быть уже округлены JSON stack клиента.
- `amount`, `balance`, `profit` и похожие monetary values должны нести currency
  в окружающем object, когда это возможно.
- `payout`, `refund` и `min_payout` являются ratios в диапазоне `0..1`, если
  поле явно не говорит обратное.
- `balance_percent` является percent value, поэтому `"2.5"` означает 2.5%, а не
  0.025.
- Time fields вроде `*_ms` остаются JSON integers. Текущие epoch milliseconds
  намного ниже JSON/JavaScript safe integer limit.

### Sizing

```json
{
  "mode": "fixed_amount",
  "amount": "10.00",
  "balance_percent": "2.5",
  "system": "kelly",
  "params": {}
}
```

Известные modes:

- `fixed_amount`: явная сумма.
- `balance_percent`: сумма вычисляется от баланса account.
- `risk_manager`: downstream risk manager решает сумму.
- `ignore_signal_amount`: amount из payload намеренно игнорируется.
- `none`: sizing instruction отсутствует.

Known systems open-ended: `kelly`, `martingale`, `anti_martingale`,
`labouchere`, custom names и т.д. Typed C++ `IMoneyManagementParams` могут быть
восстановлены higher-level code; protocol payloads несут JSON params.

### Origin Signal

Trades, созданные из signals, должны нести origin block, чтобы clients могли
запрашивать и стримить все trades, связанные с одним signal.

```json
{
  "signal_id": "101",
  "operation_id": "op-019c...",
  "bridge_id": "2",
  "unique_id": "0",
  "unique_hash": "tv:abc123",
  "signal_name": "noisy_rsi_test",
  "source_kind": "tradingview_extension"
}
```

Один `signal.submit` может породить ноль, одну или несколько сделок. Поэтому
trade-result commands и events должны идентифицировать trades напрямую, но
также включать `origin_signal` для корреляции.

Persistent storage должен хранить signal/intake record и связывать с ним каждую
созданную trade через `origin_signal`. Несколько trades могут появиться из-за
fan-out по accounts, best-payout retries, martingale/anti-martingale steps или
других money-management chains. Follow-up trades должны сохранять тот же origin
signal и могут дополнительно нести `parent_trade_id`, `chain_id` или
`step_index` внутри `metadata`, пока отдельная модель исполнения
money-management не описана.

Direct `trade.open` commands могут создавать synthetic origin signal с
`source_kind = "direct_trade_open"`, чтобы одна и та же query/event model
работала и для concrete trade requests, и для higher-level strategy signals.
