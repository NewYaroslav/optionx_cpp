# Bridge Protocol v1 Draft - Shared Objects

## Shared Objects

### Routing

`routing` describes account/platform selection before a signal becomes one or
more executable trade requests.

```json
{
  "selector": {
    "kind": "default"
  },
  "platform_type": "INTRADE_BAR"
}
```

Do not use `0` or an empty string as "not specified" in the external protocol.
Omit the field or use an explicit selector.

Selector examples:

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

Known selector kinds:

- `default`: bridge/application default account/platform.
- `account`: use one specified account.
- `accounts`: use a specified candidate list.
- `all`: duplicate to all matching accounts.

Known policies:

- `best_payout`: choose the account/platform with the best payout.
- `first_available`: choose the first account that can accept the trade.
- `round_robin`: rotate through candidate accounts.
- `random`: choose a random candidate account.

MVP should implement only `default`, `account` and optionally `all`. More
advanced policies may belong to risk management or a future node/blueprint
layer instead of the bridge itself.

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

Existing OptionX DTOs currently use numeric IDs plus `unique_hash` and
`unique_id`. Bridges may convert opaque protocol strings to local numeric DTO
fields when the value is locally generated and representable.

`unique_id`, `unique_hash` and `signal_name` are optional domain identity
fields. If a field is not known, omit it. Do not use `"0"` or an empty string as
a sentinel for "not specified"; a real external ID may legitimately be `"0"`.

### Expiry

Use one explicit expiry form instead of parallel `duration_sec` and
`expiry_time_sec` fields.

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

Exactly one expiry form should be present. Trading commands should also include
`context.valid_until_ms` when stale execution would be harmful.

`valid_until_ms` is checked against bridge receive/validation time and should
be checked again immediately before the irreversible broker/platform dispatch.
A stale command should be rejected with `stale_request`. After the command has
actually been dispatched, later expiry of `valid_until_ms` does not cancel the
operation. `client_created_at_ms` is diagnostic/client timing metadata and must
not be used as an ordering source. Future versions may split this into
`accept_until_ms` and `execute_before_ms` if the two deadlines need different
semantics.

### Money And Decimal Values

Money, prices, payouts, refunds, percentages and indicator numeric values need
decimal precision. Monetary fields in canonical responses and events use a
`MoneyValue` object:

```json
{
  "value": "10.00",
  "currency": "USD"
}
```

`currency` should be present when it is known. Request schemas may also accept a
plain decimal string or JSON number as a developer-friendly shorthand when the
currency is implied by the selected account, but bridge implementations should
normalize all forms to one decimal representation before validation and storage.

Prices, payouts, refunds, percentages and indicator numeric values remain
base-10 decimal strings unless their schema defines a richer object.

Canonical decimal string rules:

- Use a dot as the decimal separator.
- Do not use scientific notation.
- Preserve sign where meaningful, for example profit may be `"-10.00"`.
- Preserve meaningful scale when known, for example `"10.00"` for USD cents.
- Use explicit units or field semantics instead of relying on formatting.

Examples:

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

Notes:

- Clients that require exact decimal value and scale must send decimal strings.
  JSON numbers are accepted only as a convenience for simple integrations and
  may already be rounded by the client's JSON stack.
- `amount`, `balance`, `profit`, `expected_profit` and similar monetary values
  should use `MoneyValue` in canonical responses/events.
- `payout`, `refund` and `min_payout` are ratios in the `0..1` range unless a
  field explicitly says otherwise.
- `balance_percent` is a percent value, so `"2.5"` means 2.5%, not 0.025.
- Time fields such as `*_ms` stay JSON integers. Current epoch milliseconds are
  far below the JSON/JavaScript safe integer limit.

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

Known modes:

- `fixed_amount`: explicit amount.
- `balance_percent`: amount is derived from account balance.
- `risk_manager`: downstream risk manager decides amount.
- `ignore_signal_amount`: payload amount is intentionally ignored.
- `none`: no sizing instruction.

Known systems are open-ended: `kelly`, `martingale`, `anti_martingale`,
`labouchere`, custom names, etc. Typed C++ `IMoneyManagementParams` can be
restored by higher-level code; protocol payloads carry JSON params.

Mode-specific field rules:

- `fixed_amount`: `amount` is required; `balance_percent` and `system` are
  forbidden.
- `balance_percent`: `balance_percent` is required; `amount` and `system` are
  forbidden.
- `risk_manager`: `system` is required and `params` is optional; `amount` and
  `balance_percent` are forbidden unless a concrete risk manager explicitly
  documents them as hints.
- `ignore_signal_amount`: `amount`, `balance_percent` and `system` are
  forbidden.
- `none`: `amount`, `balance_percent`, `system` and `params` are forbidden.

### Origin Signal

Trades produced by signals should carry an origin block so clients can query
and stream all trades related to one signal.

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

A single `signal.submit` may produce zero, one or many trades. Therefore
trade-result commands and events should identify trades directly, but also
include `origin_signal` for correlation.

Persistent storage should keep a signal/intake record and link each produced
trade to it through `origin_signal`. Multiple trades can appear because of
account fan-out, best-payout retries, martingale/anti-martingale steps or other
money-management chains. Follow-up trades should keep the same origin signal
and may additionally carry `parent_trade_id`, `chain_id` or `step_index` inside
`metadata` until a dedicated money-management execution model is specified.

Direct `trade.open` commands may create a synthetic origin signal with
`source_kind = "direct_trade_open"` so the same query and event model works for
both concrete trade requests and higher-level strategy signals.
