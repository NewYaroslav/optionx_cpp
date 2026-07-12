# Bridge Protocol v1 Draft - Envelope And Transport

## Envelope

Commands use JSON-RPC 2.0 as the outer envelope:

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

Command responses use JSON-RPC `result` for both accepted and domain-rejected
business outcomes:

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

Protocol errors use JSON-RPC `error`. Use this only when the request cannot be
processed as a valid protocol message: parse error, method not found, invalid
params, authorization failure or internal server error.

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

Business rejections should remain a normal `result`, not a JSON-RPC `error`:

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

Events are JSON-RPC notifications: they have `jsonrpc`, `method` and `params`,
but no `id`. Events are server-to-client only.

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

Rules:

- `jsonrpc` must be `"2.0"` for JSON-RPC transports.
- `id` identifies one RPC call. It is not a business identifier.
- JSON-RPC batch requests are not supported in protocol v1.
- All client commands must contain `id`.
- Trade-affecting client notifications are forbidden because the bridge would
  not be able to return an operation id, rejection reason or idempotency
  conflict.
- Unknown client notification methods must be ignored or rejected according to
  transport policy; domain events are server-to-client notifications only.
- `protocol.hello` is an unversioned bootstrap method, but its negotiation role
  depends on the transport.
- HTTP binds the protocol version through the route prefix, for example
  `/api/v1/...`; `protocol.hello` only confirms that selected version or may be
  exposed on a separate unversioned discovery endpoint.
- WebSocket binds the protocol version through the selected subprotocol, for
  example `Sec-WebSocket-Protocol: optionx.bridge.v1`, then keeps it for the
  session. `protocol.hello` must not renegotiate a different version inside the
  selected session.
- Named-pipe and other session transports may use `protocol.hello` as the
  initial protocol-version handshake, then keep the selected version for the
  session.
- If a session transport already selected v1 but `protocol.hello` requests only
  unsupported versions, the bridge must return `unsupported_protocol_version`.
- Business command `params` must not contain a separate `protocol_version`,
  avoiding conflicts such as `/api/v1` plus `params.protocol_version = "2"`.
- Unknown optional response/event fields must be ignored by clients.
- Unknown fields in trading command objects should be rejected unless they live
  under `metadata` or an explicitly namespaced `extensions` object.
- Times in protocol fields use milliseconds since Unix epoch and end with
  `_ms`.
- Transport authentication must not be placed in `params`.
- `metadata` is always a JSON object.

### Error Taxonomy

JSON-RPC errors are reserved for request/envelope failures and protocol
execution errors:

- `-32700`: parse error.
- `-32600`: invalid request.
- `-32601`: method not found.
- `-32602`: invalid params.
- `-32603`: internal error.

OptionX protocol errors should use an implementation-defined code in the
`-32000..-32099` range and a stable string in `error.data.code`, for example:

- `authorization_failed`
- `rate_limited`
- `unsupported_protocol_version`
- `idempotency_conflict`
- `cursor_expired`
- `stale_request`
- `unsupported_buffer_encoding`

Business/domain rejections such as `risk_limit`, `insufficient_balance`,
`payout_too_low`, `broker_unavailable` or `duplicate_signal` should be normal
method results with `status = "rejected"` and `reason.code`, not JSON-RPC
errors.

### Identifiers

The protocol uses three independent identifier families:

- `id`: JSON-RPC request identifier. A retry may use a new `id`.
- `idempotency_key`: stable logical-operation key. A retry of the same logical
  command must reuse it.
- `unique_hash`: domain-level signal/trade deduplication key, often supplied by
  TradingView, MT4/MT5 or another external signal source.

`idempotency_key` rules:

- Uniqueness scope is `authenticated client + method`.
- Same key plus semantically identical payload should return the original
  result while the key is retained.
- Same key plus different payload should return `idempotency_conflict`.
- A bridge must document the retention window for idempotency keys.
- For fan-out commands, the key applies to the whole logical fan-out operation,
  not to each target account independently.

Payload identity for idempotency is computed after protocol normalization:

- enum aliases are normalized to canonical enum values;
- numeric IDs are normalized to canonical string identifiers;
- decimal strings and JSON numbers are normalized to the same decimal value;
- decimal input scale is not part of the idempotency fingerprint; output scale
  is determined by the field, currency, symbol precision or schema;
- JSON object field order is ignored;
- `context.idempotency_key` is excluded from the fingerprint;
- transport metadata, authentication data and headers are excluded.

`unique_hash` is not a transport retry mechanism. Two different transports may
deliver the same market signal with different RPC `id` and different
`idempotency_key`, while the same `unique_hash` still lets the domain layer
deduplicate the signal.

External identifiers are opaque references in the wire protocol, even when the
current C++ DTO stores them as integers. Canonical responses and events should
emit identifiers as strings. Requests may accept either strings or JSON integer
numbers for identifier fields when the value is a local numeric ID.

This keeps the contract stable for Python, Rust, MQL, JavaScript clients,
broker IDs, UUIDs, composite IDs and future wider integer types. Implementations
may parse locally generated numeric strings or integer inputs back into integer
DTO fields at the adapter boundary.

Identifier compatibility rules:

- Canonical output: strings, for example `"trade_id": "123"`.
- Accepted input: strings or JSON integers, for example `"trade_id": "123"` and
  `"trade_id": 123`.
- Do not accept floating-point, fractional or scientific-notation IDs.
- Numeric input loses leading zeros by definition; clients that need leading
  zeros must send a string.
- Unknown external/broker identifiers should be treated as opaque strings and
  compared lexically, not numerically.

### Naming And Enum Spelling

The public wire protocol should use stable string spelling instead of exposing
C++ enum names accidentally.

Rules:

- JSON field names use `lower_snake_case`.
- JSON-RPC method names, event names and topics use dot-separated namespaces,
  for example `trade.open`, `market_data.tick` and `report.created`.
- Protocol-native enum values use `lower_snake_case`, for example `accepted`,
  `time_range`, `fixed_amount`, `append` and `pricealerts`.
- Domain DTO enum values use `UPPER_SNAKE_CASE` as canonical output because
  that matches the current C++ DTO surface, for example `BUY`, `SELL`,
  `SPRINT`, `INTRADE_BAR` and `DEMO`.
- ISO 4217 currency codes use uppercase, for example `USD`.
- Responses and events must use the canonical spelling shown by the protocol.
- Receivers should accept both `UPPER_SNAKE_CASE` and `lower_snake_case`
  aliases for known enum fields, for example `BUY` and `buy`, but bridges must
  normalize them before storing or emitting data.
- Unknown enum values should be rejected in trade-affecting requests unless
  the field is explicitly open-ended, such as a custom money-management system
  name.

## Transport Binding

HTTP:

- `POST /api/v1/bridge/command` accepts one JSON-RPC command and returns one
  JSON-RPC response.
- `GET /api/v1/bridge/health` returns transport health.
- Plain HTTP is request/response. It does not deliver live subscriptions by
  itself; clients poll query/history commands with their own interval.
- A future HTTP streaming binding may expose Server-Sent Events, but polling via
  query commands is enough for v1.
- A REST convenience API may duplicate common commands for simple clients that
  cannot or should not construct JSON-RPC envelopes.

WebSocket:

- Commands and responses use JSON-RPC on one socket.
- Events are pushed to subscribed clients as JSON-RPC notifications.
- A response must repeat the command `id`.
- WebSocket bridges should use the subprotocol
  `Sec-WebSocket-Protocol: optionx.bridge.v1` once the wire contract is stable.

Named Pipe:

- On message-oriented pipes, one pipe message is one UTF-8 JSON-RPC document.
- On stream-oriented transports, use newline-delimited JSON unless a bridge
  config explicitly selects length-prefixed frames.
- Newline-delimited JSON requires compact JSON without embedded line breaks.
  Length-prefixed framing is the safer future default for binary-safe
  integrations.
- Subscribed events can be sent to connected pipe clients as JSON-RPC
  notifications.

### Auth And Permissions

MVP authentication can be a static API key configured by the local bridge. This
is enough for local tools and browser extensions, but it must be a transport
credential rather than part of business payloads.

Recommended transport binding:

- HTTP: `Authorization: Bearer <api_key>` or `X-OptionX-Api-Key`.
- WebSocket: the same header during handshake when the client can set headers,
  otherwise an initial `auth.login` command before any trade-affecting command.
- Named pipe: OS-level pipe permissions first; an optional initial `auth.login`
  command when several local clients share the same pipe.

Rules:

- API keys must not be required in query strings.
- Auth can be disabled only for explicit local/dev profiles.
- Non-loopback transports should require auth for every deployment profile.
- A bridge should expose the authenticated client identity to idempotency,
  rate-limit and audit code.
- Permissions should be scope-based once multiple clients are supported.

Suggested permission scopes:

- `trade:write`: submit signals and open trades.
- `trade:read`: read active and historical trades.
- `market_data:read`: subscribe to quotes and query quote history.
- `market_data:write`: ingest ticks and bars.
- `account:read`: read account lists, balances and platform status.
- `trading:control`: pause and resume trading.
- `reports:read`: query and subscribe to reports.
- `admin`: change bridge configuration or manage other clients.

### REST Convenience API

The JSON-RPC surface is the canonical HTTP command API. A bridge may also expose
REST-style endpoints that map one-to-one to JSON-RPC methods. This is intended
for simple clients such as curl, MQL scripts, browser bookmarks, no-code tools
and legacy integrations that can only call a URL with query parameters.

Rules:

- REST endpoints must produce the same normalized domain payloads as JSON-RPC.
- REST responses should reuse the same `result` shape as the corresponding
  JSON-RPC method.
- Query-string writes are not part of the v1 contract because URLs are commonly
  logged by browsers, proxies and servers, and `GET` may be prefetched or
  retried by intermediaries.
- State-changing REST calls must use `POST`, even for simple clients that do
  not construct JSON-RPC envelopes.
- Decimal and ID compatibility rules are the same as for JSON-RPC: simple form
  requests may use scalar field values, while responses use canonical strings.
- Authentication must use transport headers, bearer tokens, mTLS, local-only
  binding or another transport mechanism; secrets must not be required in query
  strings.

Suggested endpoints:

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

Example `POST /api/v1/trades/open` body:

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

Example simple form:

```text
POST /api/v1/trades/open/simple
Content-Type: application/x-www-form-urlencoded

symbol=EURUSD&order_type=BUY&amount=10.00&duration_ms=60000&account_id=1&idempotency_key=manual%3Aclient-trade-1
```

Simple form field mapping:

- `symbol` -> `trade.symbol` or `signal.symbol`
- `order_type` / `action` -> `BUY` or `SELL`
- `option_type` -> `SPRINT`, `CLASSIC`, etc.
- `amount` -> `trade.amount` or `sizing.amount`
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

- `200 OK`: completed synchronously or query succeeded.
- `202 Accepted`: accepted for asynchronous processing.
- `400 Bad Request`: malformed request or unsupported simple form.
- `401 Unauthorized` / `403 Forbidden`: authentication or authorization failed.
- `409 Conflict`: idempotency conflict.
- `422 Unprocessable Content`: valid syntax but invalid trading parameters.
- `429 Too Many Requests`: rate limit.
- `503 Service Unavailable`: bridge/platform temporarily unavailable.
