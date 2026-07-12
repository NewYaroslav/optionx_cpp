# Черновик протокола мостов

Статус: черновик доменного протокола. Этот документ фиксирует общее
направление протокола для новых мостов OptionX. Он намеренно может меняться и
пока не должен считаться стабильным wire-level контрактом v1.

Каноническая версия документа находится в
[bridge-protocol-v1.md](bridge-protocol-v1.md). Эта русская версия является
переводом для обсуждения и должна синхронизироваться с английской версией, но
не наоборот.

Черновик описывает бизнес-команды, события и общие объекты, которые должны
экспортировать HTTP, WebSocket и named-pipe мосты. Рекомендуемый внешний
envelope для команд и ответов - JSON-RPC 2.0, потому что будущая интеграция с
`mgc-platform` уже использует JSON-RPC surfaces и выигрывает от тех же правил
request, response и error semantics.

Протокол транспортно-независимый. HTTP, WebSocket и named-pipe мосты должны
экспортировать одинаковые command, response и event payloads. Транспортные
детали вроде HTTP headers, WebSocket authentication и pipe framing находятся
вне бизнес-payload.

## Цели

- Принимать явные trade requests от внешних инструментов.
- Принимать strategy signals, которые позже могут породить ноль, одну или
  несколько сделок.
- Поддерживать делегирование money/risk-management вместо требования, чтобы
  каждый сигнал сразу содержал финальную сумму.
- Поддерживать MT4/MT5 indicator-buffer input, где интерпретация сигнала
  переносится из MQL в мост.
- Импортировать результаты исторических тестов, когда внешний тестер уже знает
  результат.
- Запрашивать результаты сделок, историю сделок, активные сделки, аккаунты,
  балансы и статус платформ.
- Стримить отчеты и изменения состояния для WebSocket и named-pipe клиентов,
  сохраняя query-style доступ для HTTP клиентов.
- Держать мост адаптером между внешними системами и существующими OptionX DTO,
  а не второй реализацией торговой платформы.

## Не Цели Для v1

- Строить полный исторический симулятор.
- Хранить или replay тиковую/баровую историю внутри слоя протокола.
- Кодировать broker-specific authentication flows внутри command payloads.
- Определять visual node/blueprint runtime. Протокол должен оставлять место
  для такого слоя, но не зависеть от него.

## Envelope

Команды используют JSON-RPC 2.0 как внешний envelope:

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

Ответы на команды используют JSON-RPC `result` и для принятых, и для отклоненных
по бизнес-логике исходов:

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

Protocol errors используют JSON-RPC `error`. Использовать это поле нужно только
когда request невозможно обработать как корректное protocol message: parse
error, method not found, invalid params, authorization failure или internal
server error.

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

Бизнес-отказы должны оставаться обычным `result`, а не JSON-RPC `error`:

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

События являются JSON-RPC notifications: у них есть `jsonrpc`, `method` и
`params`, но нет `id`.

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

Правила:

- `jsonrpc` должен быть `"2.0"` для JSON-RPC транспортов.
- `id` идентифицирует один RPC call. Это не бизнес-идентификатор.
- `params.protocol_version` обязателен для сообщений bridge protocol.
- Клиенты должны игнорировать неизвестные optional поля в responses/events.
- Неизвестные поля в trading command objects лучше отклонять, если они не
  находятся внутри `metadata` или явно namespaced `extensions` object.
- Время в protocol fields задается в миллисекундах Unix epoch, а имена таких
  полей заканчиваются на `_ms`.
- Transport authentication не должен попадать в `params`.
- `metadata` всегда JSON object.

### Идентификаторы

Протокол использует три независимых семейства идентификаторов:

- `id`: JSON-RPC request identifier. При retry можно использовать новый `id`.
- `idempotency_key`: стабильный ключ логической операции. Retry той же
  логической команды должен повторно использовать этот ключ.
- `unique_hash`: доменный ключ дедупликации сигнала/сделки, часто поставляемый
  TradingView, MT4/MT5 или другим внешним источником сигналов.

Правила для `idempotency_key`:

- Область уникальности: `authenticated client + method`.
- Тот же ключ плюс семантически тот же payload должен возвращать исходный
  result, пока ключ хранится.
- Тот же ключ плюс другой payload должен возвращать `idempotency_conflict`.
- Мост должен документировать retention window для idempotency keys.
- Для fan-out команд ключ относится ко всей логической fan-out операции, а не
  к каждому target account отдельно.

`unique_hash` не является механизмом transport retry. Два разных транспорта
могут доставить один и тот же market signal с разными RPC `id` и разными
`idempotency_key`, но один `unique_hash` все равно позволит доменному слою
дедуплицировать сигнал.

Внешние идентификаторы являются opaque references в wire protocol, даже если
текущий C++ DTO хранит их как целые числа. Канонические responses и events
должны отдавать identifiers строками. Requests могут принимать строки или JSON
integer numbers для identifier fields, когда значение является локальным numeric
ID.

Это сохраняет контракт устойчивым для Python, Rust, MQL, JavaScript clients,
broker IDs, UUIDs, composite IDs и будущих более широких integer types.
Реализации могут парсить локально сгенерированные numeric strings или integer
inputs обратно в integer DTO fields на границе adapter.

Правила совместимости identifiers:

- Canonical output: строки, например `"trade_id": "123"`.
- Accepted input: строки или JSON integers, например `"trade_id": "123"` и
  `"trade_id": 123`.
- Не принимать floating-point, fractional или scientific-notation IDs.
- Numeric input по определению теряет leading zeros; clients, которым нужны
  leading zeros, должны отправлять string.
- Unknown external/broker identifiers следует считать opaque strings и
  сравнивать лексически, а не численно.

## Transport Binding

HTTP:

- `POST /api/v1/bridge/command` принимает одну JSON-RPC command и возвращает
  один JSON-RPC response.
- `GET /api/v1/bridge/health` возвращает transport health.
- Будущий HTTP event stream может экспортировать Server-Sent Events, но для v1
  достаточно polling через query commands.
- REST convenience API может дублировать частые commands для простых клиентов,
  которым неудобно или невозможно собирать JSON-RPC envelopes.

WebSocket:

- Commands и responses используют JSON-RPC на одном socket.
- Events пушатся subscribed clients как JSON-RPC notifications.
- Response должен повторять command `id`.
- WebSocket bridges должны использовать subprotocol
  `Sec-WebSocket-Protocol: optionx.bridge.v1`, когда wire contract станет
  стабильным.

Named Pipe:

- На message-oriented pipes одно pipe message равно одному UTF-8 JSON-RPC
  document.
- На stream-oriented transports использовать newline-delimited JSON, если
  bridge config явно не выбирает length-prefixed frames.
- Newline-delimited JSON требует compact JSON без embedded line breaks.
  Length-prefixed framing - более безопасный будущий default для binary-safe
  integrations.
- Subscribed events можно отправлять connected pipe clients как JSON-RPC
  notifications.

### REST Convenience API

JSON-RPC surface является каноническим HTTP command API. Bridge также может
экспортировать REST-style endpoints, которые one-to-one мапятся на JSON-RPC
methods. Это нужно для простых клиентов вроде curl, MQL scripts, browser
bookmarks, no-code tools и legacy integrations, которые умеют только вызвать URL
с query parameters.

Правила:

- REST endpoints должны производить те же normalized domain payloads, что и
  JSON-RPC.
- REST responses должны переиспользовать тот же `result` shape, что и
  соответствующий JSON-RPC method.
- Query-string writes разрешены только для явно включенных local или trusted
  deployments, потому что URLs часто логируются browsers, proxies и servers.
- State-changing REST calls должны предпочитать `POST`. `GET` write shortcuts -
  compatibility feature и должны быть выключены по умолчанию в production
  profiles.
- Decimal и ID compatibility rules такие же, как для JSON-RPC: requests могут
  использовать простые query values, responses используют canonical strings.
- Authentication должна использовать transport headers, bearer tokens, mTLS,
  local-only binding или другой transport mechanism; secrets не должны быть
  обязательными в query strings.

Предлагаемые endpoints:

| REST endpoint | Method | Maps to |
| --- | --- | --- |
| `/api/v1/bridge/health` | `GET` | transport health |
| `/api/v1/trades/open` | `POST` | `trade.open` |
| `/api/v1/trades/open/simple` | `GET` | `trade.open` shortcut |
| `/api/v1/signals/submit` | `POST` | `signal.submit` |
| `/api/v1/signals/submit/simple` | `GET` | `signal.submit` shortcut |
| `/api/v1/operations/{operation_id}` | `GET` | `operation.get` |
| `/api/v1/trades/{trade_id}` | `GET` | `trade.result.get` |
| `/api/v1/trades/active` | `GET` | `trade.active.query` |
| `/api/v1/trades/history` | `GET` | `trade.history.query` |
| `/api/v1/accounts` | `GET` | `account.list` |
| `/api/v1/accounts/{account_id}/balance` | `GET` | `account.balance.get` |
| `/api/v1/market-data/providers` | `GET` | `market_data.providers.list` |
| `/api/v1/market-data/providers/{provider_id}` | `GET` | `market_data.provider.get` |
| `/api/v1/market-data/subscriptions` | `POST` | `market_data.subscribe` |
| `/api/v1/market-data/subscriptions` | `GET` | `market_data.subscriptions.list` |
| `/api/v1/market-data/subscriptions/{subscription_id}` | `DELETE` | `market_data.unsubscribe` |
| `/api/v1/market-data/history/ticks` | `GET` | `market_data.history.get` |
| `/api/v1/market-data/history/bars` | `GET` | `market_data.history.get` |
| `/api/v1/market-data/ingest/ticks` | `POST` | `market_data.ingest.ticks` |
| `/api/v1/market-data/ingest/bars` | `POST` | `market_data.ingest.bars` |
| `/api/v1/trading/pause` | `POST` | `trading.pause` |
| `/api/v1/trading/resume` | `POST` | `trading.resume` |
| `/api/v1/reports` | `GET` | `reports.query` |

Пример body для `POST /api/v1/trades/open`:

```json
{
  "protocol_version": "draft",
  "context": {
    "idempotency_key": "manual:client-trade-1",
    "valid_until_ms": 1783476725000
  },
  "routing": {
    "selector": {
      "kind": "account",
      "account_id": "1"
    }
  },
  "trade": {
    "symbol": "EURUSD",
    "order_type": "BUY",
    "option_type": "SPRINT",
    "amount": "10.00",
    "expiry": {
      "kind": "duration",
      "duration_ms": 60000
    },
    "min_payout": "0.75"
  }
}
```

Пример `GET` shortcut:

```text
GET /api/v1/trades/open/simple?symbol=EURUSD&order_type=BUY&amount=10.00&duration_ms=60000&account_id=1&idempotency_key=manual%3Aclient-trade-1
```

Shortcut query mapping:

- `symbol` -> `trade.symbol` или `signal.symbol`
- `order_type` / `action` -> `BUY` или `SELL`
- `option_type` -> `SPRINT`, `CLASSIC` и т.д.
- `amount` -> `trade.amount` или `sizing.amount`
- `balance_percent` -> `sizing.balance_percent`
- `duration_ms` -> `expiry.kind = "duration"`
- `expires_at_ms` -> `expiry.kind = "absolute"`
- `account_id` -> `routing.selector.kind = "account"`
- `platform_type` -> `routing.platform_type`
- `min_payout`, `refund` -> trade/signal payout constraints
- `signal_name`, `unique_hash`, `idempotency_key`, `comment` -> identity/context
- `provider_id`, `stream`, `timeframe_ms`, `price_type` ->
  market-data provider and stream selection
- `start_ms`, `end_ms`, `count`, `lookback_ms`, `cursor`, `limit` ->
  market-data history and prefill selection
- Unknown query parameters should be rejected unless they use an agreed
  `metadata.` or `extensions.` prefix.

REST status mapping:

- `200 OK`: completed synchronously или query succeeded.
- `202 Accepted`: accepted for asynchronous processing.
- `400 Bad Request`: malformed request или unsupported query shortcut.
- `401 Unauthorized` / `403 Forbidden`: authentication или authorization failed.
- `409 Conflict`: idempotency conflict.
- `422 Unprocessable Content`: valid syntax, но invalid trading parameters.
- `429 Too Many Requests`: rate limit.
- `503 Service Unavailable`: bridge/platform temporarily unavailable.

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
  "bridge_id": "2",
  "unique_id": "0",
  "unique_hash": "tv:abc123",
  "signal_name": "noisy_rsi_test"
}
```

Один `signal.submit` может породить ноль, одну или несколько сделок. Поэтому
trade-result commands и events должны идентифицировать trades напрямую, но
также включать `origin_signal` для корреляции.

## Commands

Примеры commands ниже показывают JSON-RPC `params` object для указанного method,
а не весь внешний JSON-RPC envelope.

### `protocol.hello`

Возвращает базовую server identity и выбранную protocol version. Это полезно
для clients, которые могут говорить с несколькими bridge implementations.

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

Возвращает supported methods, feature flags и limits.

```json
{
  "protocol_versions": ["draft"],
  "server_instance_id": "019c...",
  "supported_methods": [
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
    "buffer_encoding": ["json"],
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
    "max_page_size": 1000
  }
}
```

### `trade.open`

Использовать, когда внешний client хочет конкретную сделку с явными trade
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
    "amount": "10.00",
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

`trade.open` должен означать одну конкретную сделку на одном выбранном account в
stable protocol. Будущая команда `trade.open.batch` должна покрывать явные
multi-trade submissions. Fan-out в первую очередь относится к `signal.submit`.

### `signal.submit`

Использовать, когда внешний client отправляет strategy signal. Money
management, routing и filters позже могут породить ноль, одну или несколько
сделок.

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
    "signal_name": "noisy_rsi_test",
    "idempotency_key": "tv:abc123"
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

`trade_refs` может быть пустым, потому что signal processing может быть
асинхронным или потому что risk/decision logic отклонила signal. Clients должны
использовать `signal.trades.query` или subscriptions, чтобы наблюдать созданные
trades.

### Operation Lifecycle

Long-running commands возвращают `operation_id`. Это отделяет синхронное
принятие command от последующей broker/risk-management обработки.

Известные operation states:

- `accepted`: command принята в обработку.
- `processing`: bridge/application ее обрабатывает.
- `completed`: все targets завершены успешно.
- `partially_completed`: часть fan-out targets успешна, часть failed.
- `rejected`: valid command отклонена бизнес-логикой.
- `failed`: processing неожиданно failed.
- `cancelled`: operation отменена до completion.

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

Возвращает trades, связанные с signal.

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

Использовать для MT4/MT5-style indicator buffers. Bridge интерпретирует recent
buffer values вместо того, чтобы полагаться на MQL для финального решения о
signal.

MT4/MT5 indicator buffers - series arrays: index `0` это current/latest bar,
index `1` - previous bar и так далее.

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

Заметки:

- `values` должен содержать только последние N bars, а не всю terminal history.
- `empty_value` нужен потому, что MQL indicators часто используют специальную
  константу EMPTY_VALUE.
- `empty_policy` нужен потому, что многие пользовательские indicators
  используют `0` как no data.
- Все массивы `buffers[].values` должны иметь одинаковую длину.
- `bars` optional. Если он присутствует, его длина должна совпадать с каждым
  buffer, а `bars[i]` описывает `values[i]`.
- Bar time - это `open_time_ms`. Использовать `is_closed`, чтобы отличать
  current bar от closed historical bars.
- `source_instance_id`, `indicator_version`, `settings_hash` и `sample_id`
  помогают обнаруживать replay, duplicated terminals и изменения настроек
  indicator.
- Bridge может emit reports вроде `late_signal`, `repainted_signal` или
  `signal_disappeared` вместо forwarding tradeable signal.

Известные `empty_policy` values:

- `empty_value`: только `empty_value` означает no data.
- `zero_is_empty`: только zero означает no data.
- `zero_or_empty_value`: zero и `empty_value` означают no data.
- `null_is_empty`: JSON null означает no data.

### Market Data Commands

Market-data commands покрывают котировки, тики и бары. Они намеренно отделены
от trade и signal commands: bridge может проксировать их в полноценную trading
platform, например adapter Intrade Bar, или в более легкий adapter конкретного
поставщика котировок.

Одновременно может быть несколько subscribers. У каждого subscriber свои
subscriptions, и он получает события только по ним. Subscriber обычно является
authenticated WebSocket connection, named-pipe client, HTTP session или
application component. `subscriber_id` optional; если он не задан, bridge
использует authenticated transport/session identity.

Commands:

- `market_data.providers.list`
- `market_data.provider.get`
- `market_data.subscribe`
- `market_data.unsubscribe`
- `market_data.subscriptions.list`
- `market_data.history.get`
- `market_data.ingest.ticks`
- `market_data.ingest.bars`

Provider list result:

```json
{
  "providers": [
    {
      "provider_id": "intradebar-live",
      "name": "Intrade Bar Live",
      "kind": "platform",
      "platform_type": "INTRADE_BAR",
      "status": "online",
      "supports": {
        "live_ticks": true,
        "live_bars": true,
        "history_ticks": false,
        "history_bars": true,
        "ingest_ticks": false,
        "ingest_bars": false
      },
      "limits": {
        "symbols": ["EURUSD", "BTCUSD"],
        "timeframes_ms": [1000, 60000, 300000],
        "max_history_items": 10000,
        "max_history_lookback_ms": 604800000,
        "history_depth_by_symbol": {
          "EURUSD": {
            "bars_1m_ms": 2592000000
          }
        }
      },
      "metadata": {}
    }
  ]
}
```

Providers должны явно сообщать unsupported features. Если точные limits
неизвестны, provider может опустить поле или вернуть `null`, но не должен
делать вид, что unsupported history или live streams доступны.

Live subscription request:

```json
{
  "subscriber_id": "mt5-synthetic-feed-1",
  "provider_id": "intradebar-live",
  "symbol": "BTCUSD",
  "stream": {
    "kind": "bars",
    "timeframe_ms": 60000,
    "price_type": "mid"
  },
  "prefill": {
    "mode": "count",
    "count": 100
  }
}
```

`stream.kind` values:

- `ticks`: подписка на tick updates.
- `bars`: подписка на bar/candle updates. `timeframe_ms` обязателен.

`prefill` просит provider отправить history до live events. Это полезно для
synthetic symbols, chart bootstrapping и MT4/MT5 integrations, которым нужны
последние бары для расчета обычных индикаторов.

Известные `prefill.mode` values:

- `none`: отправлять только live data.
- `count`: отправить последние `count` ticks или bars.
- `time_range`: отправить данные за `[start_ms, end_ms)`.
- `lookback`: отправить данные от `now - lookback_ms` до now.

Subscription response:

```json
{
  "status": "accepted",
  "subscription_id": "md-sub-1",
  "provider_id": "intradebar-live",
  "symbol": "BTCUSD",
  "stream": {
    "kind": "bars",
    "timeframe_ms": 60000,
    "price_type": "mid"
  },
  "prefill_status": "accepted",
  "warnings": []
}
```

Если provider не может выполнить history или prefill, bridge должен отклонить
request как domain result или принять live subscription с `prefill_status`
вроде `unsupported` или `limited`, в зависимости от requested policy.
JSON-RPC errors остаются для malformed envelopes или invalid params.

History request:

```json
{
  "provider_id": "intradebar-live",
  "symbol": "EURUSD",
  "stream": {
    "kind": "bars",
    "timeframe_ms": 60000,
    "price_type": "mid"
  },
  "range": {
    "kind": "time_range",
    "start_ms": 1783470000000,
    "end_ms": 1783477200000
  },
  "limit": 1000,
  "cursor": null
}
```

History result:

```json
{
  "provider_id": "intradebar-live",
  "symbol": "EURUSD",
  "stream": {
    "kind": "bars",
    "timeframe_ms": 60000
  },
  "items": [
    {
      "open_time_ms": 1783470000000,
      "close_time_ms": 1783470060000,
      "open": "1.14060",
      "high": "1.14070",
      "low": "1.14055",
      "close": "1.14066",
      "volume": "76.0",
      "is_closed": true
    }
  ],
  "next_cursor": null,
  "has_more": false
}
```

Tick event payload:

```json
{
  "subscription_id": "md-sub-1",
  "provider_id": "binance-live",
  "symbol": "BTCUSDT",
  "time_ms": 1783476705177,
  "bid": "64114.80",
  "ask": "64115.10",
  "last": "64114.92",
  "volume": "0.0123",
  "prefill": false
}
```

Bar event payload:

```json
{
  "subscription_id": "md-sub-1",
  "provider_id": "intradebar-live",
  "symbol": "BTCUSD",
  "timeframe_ms": 60000,
  "open_time_ms": 1783476660000,
  "close_time_ms": 1783476720000,
  "open": "64100.00",
  "high": "64150.00",
  "low": "64090.00",
  "close": "64114.92",
  "volume": "12.5",
  "is_closed": false,
  "prefill": false
}
```

Market-data prices, volumes и spreads используют то же wire-правило
decimal-string, что и торговые money values. Times используют integer
milliseconds.

Ingest/write commands позволяют внешним источникам писать котировки внутрь
OptionX ecosystem. Это полезно, когда MT4/MT5, Binance или другой источник
производит ticks/bars, а bot, synthetic chart или simulator потребляет их
внутри того же приложения.

`market_data.ingest.ticks` example:

```json
{
  "source": {
    "kind": "mt5",
    "source_instance_id": "terminal-01",
    "symbol": "EURUSD"
  },
  "target": {
    "provider_id": "local-synthetic",
    "symbol": "EURUSD"
  },
  "mode": "append",
  "ticks": [
    {
      "time_ms": 1783476705177,
      "bid": "1.14060",
      "ask": "1.14062",
      "last": null,
      "volume": "1.0"
    }
  ],
  "identity": {
    "sample_id": "mt5:terminal-01:1783476705177"
  }
}
```

`market_data.ingest.bars` имеет такую же форму, но использует `bars` плюс
`timeframe_ms`. Известные ingest modes:

- `append`: добавить новые данные и reject duplicates.
- `upsert`: insert or replace по provider, symbol, timeframe и time.
- `snapshot`: заменить текущий in-memory snapshot для target stream.

Ingest response:

```json
{
  "status": "accepted",
  "operation_id": "op-019c...",
  "accepted_count": 1,
  "rejected_count": 0,
  "warnings": []
}
```

Market-data reports должны покрывать provider и continuity issues, например
`provider_unsupported`, `history_unavailable`, `subscription_failed`,
`stream_gap`, `prefill_limited`, `ingest_rejected`, `duplicate_tick`,
`duplicate_bar` и `timestamp_out_of_order`.

### `backtest.result.import`

Использовать, когда внешний tester уже рассчитал trade result.

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
    "amount": "10.00",
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
    "profit": "8.20",
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

`source.kind = "backtest"` должен сохраняться даже когда физическое storage -
отдельная database. `PlatformType::SIMULATOR` уже существует в C++ domain model
и полезен для live simulation на real-time prices, например "запустить на неделю
без реальных сделок".

### `trade.result.get`

Возвращает текущий result snapshot одной trade. Это полезно и для intermediate
states, и для final results.

```json
{
  "selector": {
    "kind": "trade_id",
    "value": "123"
  }
}
```

Selectors избегают неоднозначного priority, когда передано несколько IDs.
Known selector kinds включают `trade_id`, `broker_option_id`,
`broker_option_hash` и `unique_hash`.

Result shape должен сохранять данные `TradeResult`, но экспортировать отдельные
lifecycle и financial outcome fields:

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
  "amount": "10.00",
  "payout": "0.82",
  "profit": "0.00",
  "balance": "1000.00",
  "open_balance": "1000.00",
  "close_balance": "0.00",
  "open_price": "1.14072",
  "close_price": "0.0",
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

Минимально допустимые lifecycle transitions:

| From | To |
| --- | --- |
| `queued` | `submitted`, `cancelled`, `failed` |
| `submitted` | `opened`, `cancelled`, `failed` |
| `opened` | `closed`, `failed` |
| `closed` | terminal |
| `cancelled` | terminal |
| `failed` | terminal |

Существующий C++ `TradeState` enum смешивает lifecycle, errors и outcomes.
Bridge adapters должны мапить его в protocol `state` / `outcome` / `failure`
shape на границе, а не экспортировать internal enum values напрямую.

Для `signal.submit` clients обычно должны получать results через
`signal.trades.query`, filters в `trade.history.query` или `trade.updated`
events, которые включают `origin_signal`.

### `trade.history.query`

Возвращает closed или persisted trade records. Payload должен отражать
`TradeRecordQuery` и `TradeRecordFilter`.

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

Time ranges являются half-open: `[start_ms, end_ms)`. Cursor pagination
предпочтительнее offset pagination, потому что новые records могут появляться,
пока client листает history. Responses должны включать `next_cursor`,
`has_more` и `snapshot_at_ms`, когда используется pagination.

### `trade.active.query`

Возвращает trades, которые еще не closed.

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
  "balance": "1000.00",
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

Пустой `scope` означает все accounts/platforms.

### Reports

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

Subscription resume - тема wire-contract, а не просто удобная фича. Будущие
stable versions должны определить:

- `resume_from_seq`
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

## Mapping To Existing DTOs

- `signal.submit` в первую очередь мапится на `TradeSignal`.
- `trade.open` мапится на executable trade intent. Bridge все еще может сначала
  emit `TradeSignal`, потому что текущий bridge callback API signal-based.
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

## Открытые Вопросы

- Должен ли `TradeRequest` получить `platform_type`, или routing всегда должен
  жить снаружи?
- Точное enum/string spelling для public JSON fields.
- Должен ли `trade.open` в будущем bridge APIs обходить `TradeSignal`, или
  только в lower-level execution component.
- Как моделировать один signal, который создает несколько trades в persistent
  storage.
- Точная JSON shape и cursor implementation для `TradeRecordQuery`.
- Где хранить backtest data: отдельная DB через application config
  предпочтительнее, но protocol все равно должен помечать
  `source.kind = "backtest"`.
- Compact raw-buffer encoding для MT4/MT5, когда bars/buffers станут большими.
- Точная subscription replay retention, persistence и reconnect behavior.
- Auth и permissions для каждого transport.

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
- Binary или compact buffer encoding для больших MT4/MT5 payloads.
