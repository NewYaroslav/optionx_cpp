# Bridge Protocol v1 Draft - Envelope И Transport

## Envelope

Команды используют JSON-RPC 2.0 как внешний envelope:

```json
{
  "jsonrpc": "2.0",
  "id": "client-request-uuid",
  "method": "signal.submit",
  "params": {
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
`params`, но нет `id`. Events являются server-to-client only.

```json
{
  "jsonrpc": "2.0",
  "method": "trade.updated",
  "params": {
    "event_id": "evt-019c...",
    "source": "optionx://installation-019c/bridge/2",
    "stream_id": "bridge-instance-019c...",
    "seq": 1842,
    "matched_event_subscription_ids": ["evt-sub-1"],
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
- JSON-RPC batch requests не поддерживаются в protocol v1.
- Все client commands должны содержать `id`.
- Trade-affecting client notifications запрещены, потому что bridge не сможет
  вернуть operation id, rejection reason или idempotency conflict.
- Unknown client notification methods должны игнорироваться или отклоняться
  согласно transport policy; domain events являются server-to-client
  notifications only.
- `protocol.hello` является unversioned bootstrap method, но его роль в
  согласовании версии зависит от transport.
- HTTP задает protocol version через route prefix, например `/api/v1/...`;
  `protocol.hello` только подтверждает выбранную версию или может быть доступен
  на отдельном unversioned discovery endpoint.
- WebSocket задает protocol version через выбранный subprotocol, например
  `Sec-WebSocket-Protocol: optionx.bridge.v1`, и сохраняет его на всю session.
  `protocol.hello` не должен пересогласовывать другую версию внутри уже
  выбранной session.
- Named-pipe и другие session transports могут использовать `protocol.hello`
  как initial protocol-version handshake и затем сохранять выбранную версию на
  всю session.
- Если session transport уже выбрал v1, но `protocol.hello` запрашивает только
  неподдерживаемые версии, bridge должен вернуть `unsupported_protocol_version`.
- Business command `params` не должны содержать отдельный `protocol_version`,
  чтобы не возникали конфликты вроде `/api/v1` плюс
  `params.protocol_version = "2"`.
- Клиенты должны игнорировать неизвестные optional поля в responses/events.
- Неизвестные поля в trading command objects лучше отклонять, если они не
  находятся внутри `metadata` или явно namespaced `extensions` object.
- Время в protocol fields задается в миллисекундах Unix epoch, а имена таких
  полей заканчиваются на `_ms`.
- Transport authentication не должен попадать в `params`.
- `metadata` всегда JSON object.

### Категории Ошибок

JSON-RPC errors используются для request/envelope failures и ошибок исполнения
самого протокола:

- `-32700`: parse error.
- `-32600`: invalid request.
- `-32601`: method not found.
- `-32602`: invalid params.
- `-32603`: internal error.

OptionX protocol errors должны использовать implementation-defined code в
диапазоне `-32000..-32099` и стабильную строку в `error.data.code`, например:

- `authorization_failed`
- `rate_limited`
- `unsupported_protocol_version`
- `idempotency_conflict`
- `cursor_expired`
- `stale_request`

Бизнес/domain rejections вроде `risk_limit`, `insufficient_balance`,
`payout_too_low`, `broker_unavailable`, `duplicate_signal` или
`unsupported_buffer_encoding` должны быть обычным result метода со
`status = "rejected"` и `reason.code`, а не JSON-RPC errors. Malformed payload
structure, включая поврежденный encoding object или отсутствующие обязательные
encoding fields, должен быть `-32602 invalid params`.

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

Порядок idempotency lookup для trade-affecting commands:

1. Валидировать envelope, authentication и форму idempotency key.
2. Найти существующую idempotency record.
3. Если identical operation уже существует, вернуть ее original/current result,
   даже если новый retry пришел после `valid_until_ms`.
4. Только для новой operation валидировать `valid_until_ms`.
5. Проверить `valid_until_ms` еще раз прямо перед необратимой отправкой
   broker/platform.

Payload identity для idempotency вычисляется после protocol normalization:

- enum aliases нормализуются в canonical enum values;
- numeric IDs нормализуются в canonical string identifiers;
- decimal strings и JSON numbers нормализуются в одно decimal value;
- decimal input scale не входит в idempotency fingerprint; output scale
  определяется field, currency, symbol precision или schema;
- порядок JSON object fields не учитывается;
- `context.idempotency_key` исключается из fingerprint;
- transport metadata, authentication data и headers не входят в fingerprint.

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

### Naming And Enum Spelling

Публичный wire protocol должен использовать стабильное string spelling, а не
случайно экспортировать C++ enum names.

Правила:

- JSON field names используют `lower_snake_case`.
- JSON-RPC method names, event names и topics используют dot-separated
  namespaces, например `trade.open`, `market_data.tick` и `report.created`.
- Protocol-native enum values используют `lower_snake_case`, например
  `accepted`, `time_range`, `fixed_amount`, `append` и `pricealerts`.
- Domain DTO enum values используют `UPPER_SNAKE_CASE` как canonical output,
  потому что это соответствует текущей C++ DTO surface, например `BUY`,
  `SELL`, `SPRINT`, `INTRADE_BAR` и `DEMO`.
- ISO 4217 currency codes используют uppercase, например `USD`.
- Responses и events должны использовать canonical spelling из протокола.
- Receivers должны принимать и `UPPER_SNAKE_CASE`, и `lower_snake_case` aliases
  для known enum fields, например `BUY` и `buy`, но bridges должны
  нормализовать их перед хранением или emit.
- Unknown enum values следует отклонять в trade-affecting requests, если поле
  не является явно open-ended, например custom money-management system name.

## Transport Binding

HTTP:

- `POST /api/v1/bridge/command` принимает одну JSON-RPC command и возвращает
  один JSON-RPC response.
- `GET /api/v1/bridge/health` возвращает transport health.
- Обычный HTTP является request/response transport. Он сам по себе не доставляет
  live subscriptions; clients опрашивают query/history commands со своей
  периодичностью.
- Будущий HTTP streaming binding может экспортировать Server-Sent Events, но
  для v1 достаточно polling через query commands.
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

File Drop:

- File-drop transport привязывает те же JSON-RPC business methods к request,
  response и event files, в первую очередь для MT4/MT5 integrations, где нельзя
  удобно поставлять sockets или HTTP servers.
- Подробный file layout, atomic write rules и legacy adapter profiles описаны в
  [File Transport И Legacy Adapters](file-transport-and-adapters.ru.md).

### Auth And Permissions

MVP authentication может быть статическим API key, настроенным в local bridge.
Этого достаточно для local tools и browser extensions, но это должен быть
transport credential, а не часть business payloads.

Рекомендуемый transport binding:

- HTTP: `Authorization: Bearer <api_key>` или `X-OptionX-Api-Key`.
- WebSocket: тот же header во время handshake, если client может задавать
  headers, иначе initial `auth.login` command до любых trade-affecting commands.
- Named pipe: сначала OS-level pipe permissions; optional initial `auth.login`
  command, когда несколько local clients делят один pipe.

Правила:

- API keys не должны быть обязательными в query strings.
- Auth можно отключать только в explicit local/dev profiles.
- Non-loopback transports должны требовать auth в любом deployment profile.
- Bridge должен отдавать authenticated client identity в idempotency, rate-limit
  и audit code.
- Permissions должны стать scope-based, когда один bridge обслуживает несколько
  clients.

Предлагаемые permission scopes:

- `trade:write`: отправка signals и открытие trades.
- `trade:read`: чтение active и historical trades.
- `market_data:read`: подписка на quotes и запрос quote history.
- `market_data:write`: ingest ticks и bars.
- `account:read`: чтение account lists, balances и platform status.
- `trading:control`: pause/resume trading.
- `reports:read`: query и subscribe to reports.
- `admin`: изменение bridge configuration или управление другими clients.

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
- Query-string writes не входят в contract v1, потому что URLs часто логируются
  browsers, proxies и servers, а `GET` может быть prefetched или retried
  intermediaries.
- State-changing REST calls должны использовать `POST`, даже для простых
  clients, которые не собирают JSON-RPC envelopes.
- Decimal и ID compatibility rules такие же, как для JSON-RPC: simple form
  requests могут использовать scalar field values, responses используют
  canonical strings.
- Authentication должна использовать transport headers, bearer tokens, mTLS,
  local-only binding или другой transport mechanism; secrets не должны быть
  обязательными в query strings.

Предлагаемые endpoints:

| REST endpoint | Method | Maps to |
| --- | --- | --- |
| `/api/v1/bridge/health` | `GET` | transport health |
| `/api/v1/trades/open` | `POST` | `trade.open` |
| `/api/v1/trades/open/simple` | `POST` | `trade.open` simple form |
| `/api/v1/signals/submit` | `POST` | `signal.submit` |
| `/api/v1/signals/submit/simple` | `POST` | `signal.submit` simple form |
| `/api/v1/operations/{operation_id}` | `GET` | `operation.get` |
| `/api/v1/trades/{trade_id}` | `GET` | `trade.result.get` |
| `/api/v1/trades/active` | `GET` | `trade.active.query` |
| `/api/v1/trades/history` | `GET` | `trade.history.query` |
| `/api/v1/accounts` | `GET` | `account.list` |
| `/api/v1/accounts/{account_id}/balance` | `GET` | `account.balance.get` |
| `/api/v1/market-data/providers` | `GET` | `market_data.providers.list` |
| `/api/v1/market-data/providers/{provider_id}` | `GET` | `market_data.provider.get` |
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

Пример simple form:

```text
POST /api/v1/trades/open/simple
Content-Type: application/x-www-form-urlencoded

symbol=EURUSD&order_type=BUY&amount=10.00&duration_ms=60000&account_id=1&idempotency_key=manual%3Aclient-trade-1
```

Simple form field mapping:

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
- `idempotency_key` -> `context.idempotency_key`
- `signal_name`, `unique_hash`, `comment` -> identity/context
- `provider_id`, `stream`, `timeframe_ms`, `price_type` ->
  market-data provider and stream selection
- `start_ms`, `end_ms`, `count`, `lookback_ms`, `cursor`, `limit` ->
  market-data history and prefill selection
- Unknown simple-form fields should be rejected unless they use an agreed
  `metadata.` or `extensions.` prefix.

REST status mapping:

- `200 OK`: completed synchronously или query succeeded.
- `202 Accepted`: accepted for asynchronous processing.
- `400 Bad Request`: malformed request или unsupported simple form.
- `401 Unauthorized` / `403 Forbidden`: authentication или authorization failed.
- `409 Conflict`: idempotency conflict.
- `422 Unprocessable Content`: valid syntax, но invalid trading parameters.
- `429 Too Many Requests`: rate limit.
- `503 Service Unavailable`: bridge/platform temporarily unavailable.
