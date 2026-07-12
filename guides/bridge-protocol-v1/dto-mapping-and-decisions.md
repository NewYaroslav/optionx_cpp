# Bridge Protocol v1 Draft - DTO Mapping And Decisions

## Mapping To Existing DTOs

- `signal.submit` maps primarily to `TradeSignal`.
- `trade.open` maps to an executable trade intent. Public bridge APIs should
  still keep the same operation/report/origin lifecycle as `signal.submit`;
  only lower-level platform execution code should bypass that lifecycle and
  convert directly to `TradeRequest`.
- `TradeSignal::assign_to_request()` maps signal fields to `TradeRequest`.
- `trade.result.get` maps to `TradeResult`.
- `trade.history.query` maps to `TradeRecordQuery` and returns `TradeRecord`.
- `reports.*` maps to `BridgeSignalReport` plus future report DTOs.
- Account commands map to `AccountInfoUpdate` / `BaseAccountInfoData` snapshots.

Important current mismatch:

- `TradeSignal` has `platform_type`; `TradeRequest` does not.
- Therefore platform/account routing should remain a protocol/application layer
  around the signal/request instead of being forced into `TradeRequest`.
- This matches the current intent: `TradeRequest` is the platform/broker-level
  request object, while platform identity belongs to routing, account data,
  platform instances and result/history snapshots.

## Resolved Draft Decisions

- `TradeRequest` should not grow `platform_type` for the public bridge protocol.
  Routing remains outside the platform request object.
- Public JSON field names use `lower_snake_case`; protocol-native enum values
  use `lower_snake_case`; domain DTO enum values use `UPPER_SNAKE_CASE` as
  canonical output while accepting both `UPPER_SNAKE_CASE` and
  `lower_snake_case` on input.
- `trade.open` should not skip the public operation/report/origin lifecycle.
  Direct conversion to `TradeRequest` is a lower-level execution concern.
- One signal producing many trades is modeled by one persisted signal/intake
  record plus many trade records with `origin_signal`.
- `TradeRecordQuery` uses selector/filter payloads and opaque cursor
  pagination.
- Backtests should normally use a separate database selected by application
  config, while protocol payloads still mark `source.kind = "backtest"`.
- Large MT4/MT5 raw buffers may use the advertised `binary_base64` compact
  encoding: little-endian `float64` values, little-endian `int64` timestamps
  and explicit typed arrays.
- Subscription reconnect is client behavior; replay is optional server behavior
  advertised through capabilities and bounded by configured retention.
- MVP auth is an optional local API key, transported through headers or an
  initial auth command. Permissions should become scope-based as soon as
  multiple clients share a bridge.

## Open Questions

- Exact durable subscription storage model for event replay across bridge
  restarts.

## Before Stable Wire v1

The following items are useful before this draft becomes a stable external wire
contract, but do not need to be implemented in the current documentation PR:

- JSON Schema 2020-12 for command, response, event and shared-object payloads.
- OpenAPI binding for HTTP and AsyncAPI binding for WebSocket/event streams.
- Compatibility tests for schema evolution.
- Optional CloudEvents-inspired naming for event `source`, `type`, `subject`
  and `time`.
- Optional FIX/FIXP-inspired session details: durable sequence persistence,
  heartbeat, reconnect/resynchronization and backpressure policy.
- Compatibility tests for `binary_base64` compact buffer encoding.
