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

`account_id` - стабильная идентичность account, назначенная host application
или account registry на стороне OptionX. Она используется для routing,
filtering и корреляции events. Broker/platform user identifiers не должны
попадать в `account_id`; когда они известны, их нужно отдавать отдельно как
`user_id` в account snapshots и account events.

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
  "unique_hash": "external-key",
  "signal_name": "rsi_cross",
  "comment": "optional user comment"
}
```

Существующие OptionX DTO сейчас используют numeric IDs плюс `unique_hash` и
`unique_id`. Bridges могут конвертировать opaque protocol strings в локальные
numeric DTO fields, если значение локально сгенерировано и представимо.

`unique_id`, `unique_hash` и `signal_name` являются опциональными domain
identity fields. Если значение неизвестно, поле нужно опустить. Не использовать
`"0"` или пустую строку как sentinel для "not specified": реальный внешний ID
теоретически может быть `"0"`.

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

`valid_until_ms` проверяется относительно времени приема/валидации на стороне
bridge и должен проверяться повторно прямо перед необратимой отправкой
broker/platform. Stale command должен быть отклонен с `stale_request`. После
фактической отправки command истечение `valid_until_ms` уже не отменяет
operation. `client_created_at_ms` является диагностической client timing
metadata и не должен использоваться как источник ordering. Будущие версии могут
разделить это на `accept_until_ms` и `execute_before_ms`, если двум deadline
нужна разная семантика.

### Money And Decimal Values

Money values, prices, payouts, refunds, percentages и indicator numeric values требуют
точной decimal-семантики. Monetary fields в canonical responses/events используют
`MoneyValue` object:

```json
{
  "value": "10.00",
  "currency": "USD"
}
```

`currency` должен присутствовать, когда он известен. Request schemas могут также
принимать plain decimal string или JSON number как developer-friendly shorthand,
если currency выводится из выбранного account, но bridge implementations должны
нормализовать все формы до одной decimal representation перед validation/storage.

Prices, payouts, refunds, percentages и indicator numeric values остаются
base-10 decimal strings, если конкретная schema не задаёт более богатый object.

Canonical decimal string rules:

- Использовать точку как decimal separator.
- Не использовать scientific notation.
- Сохранять sign, где он имеет смысл, например profit может быть `"-10.00"`.
- Сохранять meaningful scale, когда он известен, например `"10.00"` для USD cents.
- Использовать явные units или field semantics вместо зависимости от formatting.

Примеры:

```json
{
  "amount": {
    "value": "10.00",
    "currency": "USD"
  },
  "price": "1.14072",
  "profit": {
    "value": "-10.00",
    "currency": "USD"
  },
  "balance_percent": "2.5",
  "payout": "0.82"
}
```

Заметки:

- Clients, которым нужны точные decimal value и scale, должны отправлять decimal
  strings или `MoneyValue.value`. JSON numbers принимаются только как удобство
  для простых integrations и могут быть уже округлены JSON stack клиента.
- `amount`, `balance`, `profit`, `expected_profit` и похожие monetary values
  должны использовать `MoneyValue` в canonical responses/events.
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
  "amount": {
    "value": "10.00",
    "currency": "USD"
  }
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

Mode-specific field rules:

- `fixed_amount`: `amount` обязателен; `balance_percent` и `system`
  запрещены.
- `balance_percent`: `balance_percent` обязателен; `amount` и `system`
  запрещены.
- `risk_manager`: `system` обязателен, `params` optional; `amount` и
  `balance_percent` запрещены, если конкретный risk manager явно не
  документирует их как hints.
- `ignore_signal_amount`: `amount`, `balance_percent` и `system` запрещены.
- `none`: `amount`, `balance_percent`, `system` и `params` запрещены.

Текущие C++ bridge DTOs сохраняют `fixed_amount` sizing и common grouping
fields. `balance_percent`, `risk_manager`, `ignore_signal_amount`, `system` и
`params` являются protocol shapes для будущей typed DTO support; текущие C++
bridge implementations отклоняют их с `invalid_params`, а не молча теряют их
смысл.

### Origin Signal

Trades, созданные из signals, должны нести origin block, чтобы clients могли
запрашивать и стримить все trades, связанные с одним signal.

```json
{
  "signal_id": "101",
  "operation_id": "op-019c...",
  "bridge_id": "2",
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
