# OptionX TradingView Alert Extension

Small Chrome/Edge MV3 extension that watches visible TradingView alert toasts
and forwards them to a local bridge.

This is an experimental free TradingView path. It does not use official
TradingView webhooks, so it depends on TradingView's visible browser UI and can
break when TradingView changes that UI.

## Install

1. Open `chrome://extensions`.
2. Enable Developer mode.
3. Click Load unpacked.
4. Select `browser_extensions/tradingview-alert-extension`.
5. Open TradingView and reload the chart tab.
6. Open the extension popup and set the local bridge endpoint and optional
   shared secret.

Default endpoint:

```text
http://127.0.0.1:6560/api/v1/tradingview/signal
```

## What It Captures

The content script watches for alert toast cards with visible text similar to:

```html
Alert on EURUSD
EURUSD Crossing 1.14072
```

It extracts the toast title, description, symbol and a best-effort action:

- `BUY`, `LONG`, `CALL` -> `buy`;
- `SELL`, `SHORT`, `PUT` -> `sell`;
- anything else -> `alert`.

Level alerts such as `EURUSD Crossing 1.14072` are market events, not trade
commands. The local bridge should map these through user rules or reject them
unless the alert message contains an explicit command.

## Trigger vocabulary

The content script uses a best-effort vocabulary to label the toast direction.
The mapping is intentionally narrow; anything not matched becomes
`direction: null` so the bridge never invents a trade signal from random text.

Disclaimer: this parser was reverse-engineered from observable toast strings
on TradingView's free UI. TradingView can change the wording at any time; the
bridge must treat `direction` as advisory, not authoritative.

| Toast message fragment                       | `direction`         |
| -------------------------------------------- | ------------------- |
| `EURUSD Crossing 1.14145`                    | `cross`             |
| `EURUSD Crossing Up 1.14143`                 | `up`                |
| `EURUSD Crossing Down 1.14142`               | `down`              |
| `EURUSD Greater Than 1.15` / `Above 1.15`    | `above`             |
| `EURUSD Less Than 1.10` / `Below 1.10`       | `below`             |
| `EURUSD Moving Up`                           | `moving_up`         |
| `EURUSD Moving Up 1.0%`                      | `moving_up_pct`     |
| `EURUSD Moving Down`                         | `moving_down`       |
| `EURUSD Moving Down 1.0%`                    | `moving_down_pct`   |
| `EURUSD Entering Channel`                    | `entering_channel`  |
| `EURUSD Exiting Channel`                     | `exiting_channel`   |
| `EURUSD Inside Channel` / `In Channel`       | `inside_channel`    |
| `EURUSD Outside Channel` / `Out Of Channel`  | `outside_channel`   |

`action` is `buy` or `sell` only when the message is an explicit command such
as `BUY EURUSD` or `LONG BTCUSDT`; otherwise it falls back to `alert`.

## Security defaults

The extension ships with forwarding disabled. Users must opt in via the
popup before any signal leaves the browser. The bridge endpoint, shared
secret, and the `include_tab_url` flag are all stored in
`chrome.storage.local` and can be changed at any time. Existing users
upgrading from v0.1.0 keep their previous `enabled` state. Only fresh
installs start with `enabled: false`.

The full chart tab URL is **not** sent by default; only `host`,
`symbol_from_url` and `interval` extracted from the URL are attached to the
payload. Enabling the popup checkbox `Include full tab URL` is required to
transmit the raw URL.

The shared secret is sent as the request header `X-OptionX-Secret`, never
inside the JSON body.

## Test Indicator

`examples/optionx_noisy_test_signals.pine` is a Pine Script test indicator for
manual TradingView checks. It emits noisy RSI centerline `buy`/`sell` JSON
alerts with `alert.freq_once_per_bar`.

This is useful for:

- checking visible TradingView alert toasts;
- validating local duplicate suppression when the same signal appears in more
  than one study update.

See `examples/README.md` for TradingView setup steps and the separate
`alertcondition()` comparison fixture.

## Local Payload

The extension sends a `POST` with JSON:

```json
{
  "version": 1,
  "source": "tradingview_extension",
  "source_kind": "alert_toast_dom",
  "event_id": "tv_toast:<fnv1a-hash>",
  "fingerprint": "<fnv1a-hash>",
  "symbol": "EURUSD",
  "action": "alert",
  "raw_action": "BUY",
  "direction": "up",
  "raw_direction": "Crossing Up",
  "price": 1.14072,
  "trigger_value": null,
  "trigger_unit": null,
  "time": "2026-07-08T00:00:00.000Z",
  "title": "Alert on EURUSD",
  "message": "EURUSD Crossing 1.14072",
  "extension": {
    "id": "chrome-extension-id",
    "tab_id": 123,
    "host": "www.tradingview.com",
    "symbol_from_url": "EURUSD",
    "interval": "1"
  },
  "raw": {
    "title": "Alert on EURUSD",
    "description": "EURUSD Crossing 1.14072"
  }
}
```

Stable event_id is resolved via 4-level fallback. `event_id` and `fire_id` are used
directly (with prefix where applicable); `alert_id` alone is NOT sufficient — it is
the alert CONFIGURATION id, not the per-fire id. `alert_id` is composed with
`fire_time`/`bar_time`/`time`/`timenow` to form `tv_alert:<alert_id>:<time>`.
See [Event id semantics](#event-id-semantics) for the full resolution chain.

`extension.url` is included only when the popup option `Include full tab URL`
is enabled; by default only `host`, `symbol_from_url` and `interval` are sent.

The shared secret, when configured, is sent as the request header
`X-OptionX-Secret` and is **not** present in the JSON body. The bridge must
verify the header against the configured value.

Requests have a 3 second timeout; an unreachable bridge will surface as a
`Bridge timeout` entry in the popup log.

Expected bridge behavior:

- bind to loopback by default;
- require `X-OptionX-Secret` if a secret is configured;
- deduplicate by `event_id`; use `fingerprint` only with an explicit time/window policy;
- map `action` to `TradeSignal::order_type` only when it is `buy` or `sell`;
- keep `amount` on the local bridge/risk-management side.

## Dedup semantics

Two distinct identifiers are sent in each signal:

- **`fingerprint`** — content hash (FNV1a of `${source_kind}|${symbol}|${title}|${message}`). The same TradingView toast produces the same fingerprint, even across page reloads. Different alert actions on the same symbol produce different fingerprints.
- **`event_id`** — stable identifier for retry safety. Default format `tv_toast:<fingerprint>`. `event_id` and `fire_id` are used directly (with prefix where applicable); `alert_id` alone is NOT sufficient — it is the alert CONFIGURATION id, not the per-fire id, and is composed with `fire_time`/`bar_time`/`time`/`timenow` to form `tv_alert:<alert_id>:<time>`. See [Event id semantics](#event-id-semantics) for the full resolution chain.

**Layered responsibilities:**

- **Content script** suppresses DOM-level duplicates inside a `DUPLICATE_WINDOW_MS` window (5 seconds) by `fingerprint`. This is local debouncing only — NOT a long-term dedup policy.
- **Service worker** sends every accepted signal. It does NOT maintain a long-term dedup store.
- **Bridge** (`127.0.0.1:6560`) is free to use `fingerprint` and `event_id` as it sees fit. Long-term dedup, idempotency keys, retry suppression — all are the bridge's responsibility, not the extension's.

For unusual signals (same `BUY EURUSD` every hour), the bridge MUST decide its own dedup window. The extension makes no guarantees.

## Event id semantics

The `event_id` field follows a 4-level fallback resolution. Each level guarantees
strictly stronger uniqueness guarantees than the next:

1. **`parsed.event_id`** — canonical ID from a JSON payload. Used as-is (no prefix).
2. **`parsed.fire_id`** — prefixed `tv_fire:<id>`. The fire ID is the per-trigger
   identifier and remains unique across re-fires of the same alert.
3. **`parsed.alert_id` + time** — prefixed `tv_alert:<alert_id>:<time>` where time
   is the first available of `fire_time`, `bar_time`, `time`, `timenow`. The alert
   id alone is **NOT sufficient** as event id — it identifies the alert configuration,
   which is the same across all fires of that configuration.
4. **Fingerprint fallback** — `tv_toast:<fingerprint>`. Used when no other id source is
   available; guarantees per-toast-content uniqueness.

Bridge should treat any value with the same prefix as the same kind (e.g. all
`tv_alert:*` are per-fire). The `tv_toast:` prefix indicates toast-derived IDs that
may collide if the alert fires repeatedly with identical text.

### Payload schema

- `price`: number | null. First numeric value from message (or parsed.price). Null when `direction` is pct-based because `parsed.price` in pct triggers typically carries the symbol's absolute price (e.g. 1.14145) rather than a percentage value, which would mislead consumers.
- `trigger_value`: number | null. Trigger value for pct-based directions (`moving_up_pct`/`moving_down_pct`).
- `trigger_unit`: string | null. Unit of trigger_value (`"percent"` for pct, future: `"pips"`/`"points"`, null when not applicable).
- `direction`: enum: `cross`, `up`, `down`, `moving_up`, `moving_up_pct`, `moving_down`, `moving_down_pct`, `above`, `below`, `entering_channel`, `exiting_channel`, `inside_channel`, `outside_channel`, or null.

## Tests

Parser rules are unit-tested in plain Node. From the extension directory:

```bash
node --test tests/parser.test.mjs
```

Fixtures of captured TradingView toast fragments live under
`tests/fixtures/`. The service worker itself is not exercised by these tests
because it depends on the Chrome extension API.

## Notes

- The class name from the first observed toast looked like
  `description-ULNSeceN`, but the suffix is generated. The extension uses broad
  text/container detection instead of exact class matching.
- The content script suppresses identical toast text for 5 seconds. The local
  bridge must still have its own duplicate guard.
- If localhost requests fail, open the extension popup and check the recent
  event log.
