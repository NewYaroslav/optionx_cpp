# OptionX TradingView Alert Extension

Small Chrome/Edge MV3 extension that watches TradingView alert events and
forwards them to a local bridge. It supports visible alert toasts, the private
`pricealerts/alert_fired` pushstream used by fresh level alerts, and indicator
`alertMessages[]` emitted on the chart data WebSocket.

This is an experimental free TradingView path. It does not use official
TradingView webhooks, so it depends on TradingView's browser UI and private
browser-side alert streams. It can break when TradingView changes either one.

## Install

1. Open `chrome://extensions`.
2. Enable Developer mode.
3. Click Load unpacked.
4. Select `browser_extensions/tradingview-alert-extension`.
5. Open TradingView and reload the chart tab.
6. Open the extension popup and set the local bridge endpoint and optional
   shared secret.

After reloading the TradingView tab, the popup log should show
`TradingView observer active (...)`. If the bridge status is `online` but that
observer-active line is missing, the popup can reach the local bridge but the
content script is not attached to the TradingView page yet.

Default endpoint:

```text
http://127.0.0.1:6560/api/v1/tradingview/signal
```

## What It Captures

There are two independent capture modes in the popup:

- **Visible alert toasts**: a DOM observer reads alert cards shown in the chart
  page. This is useful as a low-risk fallback and for visible level alerts.
- **Private alert feed**: a page-world WebSocket hook watches only
  `wss://pushstream.tradingview.com/message-pipe-ws/private_feed` frames where
  `text.channel == "pricealerts"` and `text.content.m == "alert_fired"`. It
  ignores lifecycle messages such as `alerts_created` and `alerts_updated`.
- **Indicator study alerts**: the same page-world WebSocket hook watches
  `wss://data.tradingview.com/socket.io/websocket?...type=chart...` frames and
  extracts `du.p[1].<study-id>.ns.d.data.alertMessages[]` emitted by Pine
  `alert()` calls.

The private feed hook stays inside the browser page. It does not expose
TradingView cookies, session tokens or full WebSocket traffic to the local
bridge; only normalized alert events are forwarded.

### Visible Alert Toasts

The content script watches for alert toast cards with visible text similar to:

```html
Alert on EURUSD
EURUSD Crossing 1.14072
```

It extracts the toast title, description, symbol and a best-effort action:

- `BUY`, `LONG`, `CALL` -> `buy`;
- `SELL`, `SHORT`, `PUT` -> `sell`;
- anything else -> `alert`.

TradingView's configurable alert title/name is sent separately as
`alert_name` when the toast exposes it through the observed TradingView
`name-*` element. It also participates in action detection, so a title such as
`BUY Test99` can produce `action=buy` while the main alert text remains in
`message`. Generic DOM names such as `username` are intentionally ignored.

Level alerts such as `EURUSD Crossing 1.14072` are market events, not trade
commands. The local bridge should map these through user rules or reject them
unless the alert message contains an explicit command.

### Private Alert Feed

Fresh TradingView price alert popups have also been observed on:

```text
wss://pushstream.tradingview.com/message-pipe-ws/private_feed
```

The extension injects a page-context WebSocket wrapper at `document_start` and
forwards only confirmed `pricealerts/alert_fired` events. Event identity comes
from TradingView's per-fire id:

```json
{
  "source": "tradingview_extension",
  "source_kind": "private_pricealerts_ws",
  "method": "alert_fired",
  "event_id": "tv_price_alert:53256556946",
  "fire_id": "53256556946",
  "alert_id": "5099741779",
  "symbol": "FX:EURUSD",
  "action": "alert",
  "message": "EURUSD Crossing 1.14072"
}
```

This mode is for fresh price/level alerts. Chart-socket indicator messages are
handled by the separate study-alert mode below; full historical replay remains
a separate backtesting/research path.

### Indicator Study Alerts

Indicator `alert()` messages from the open chart have been observed on the
chart data WebSocket, not on `private_feed`:

```text
wss://data.tradingview.com/socket.io/websocket?from=chart/...&type=chart&auth=sessionid
```

The useful payload is not the outgoing `create_study` request. It is a later
study update:

```text
du.p[1].<study-id>.ns.d
  -> JSON
  -> data.alertMessages[]
  -> msg
```

`msg` can contain our Pine-provided JSON, for example:

```json
{
  "source": "tradingview",
  "signal_name": "noisy_rsi_test",
  "action": "buy",
  "symbol": "BTCUSD",
  "tickerid": "CRYPTO:BTCUSD",
  "price": 64130.49,
  "time": 1783764000000
}
```

The extension normalizes that to `source_kind:
private_chart_study_alert_messages` and forwards it through the same local HTTP
bridge path. Duplicate copies of the same study alert, for example the same
`msg` repeated under two study ids, are suppressed by a bounded in-page key
cache.

This mode is for live indicator capture. A separate history/replay API can
intentionally consume older chart-socket data in a future PR.

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

The extension sends a `POST` with JSON. Visible toast payloads look like:

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

The popup checks bridge availability with fixed `GET /health` on the same origin
as the signal endpoint. For the default endpoint
`http://127.0.0.1:6560/api/v1/tradingview/signal`, the health probe is:

```text
http://127.0.0.1:6560/health
```

The check runs when the popup opens, after saving settings, and then every 5
seconds while the popup remains open. Recent events are also refreshed live
while the popup is open whenever the background worker writes a new log entry.
This status check does not send the shared secret or any TradingView payload.

The content script also writes `TradingView observer active (...)` and
`private_feed_hook_injected` to the popup log when it attaches to a TradingView
tab. If the bridge shows `online` but real TradingView alerts do not appear in
**Recent events**, first reload the TradingView chart tab. The WebSocket hook
must be present before TradingView opens the private feed or chart sockets. If
toast capture is enabled but no toast events appear, verify that the TradingView
alert has popup notifications enabled and that a visible toast is shown in the
chart page.

For indicator signals, the popup log should show
`TradingView content status: chart_socket_hook_attached` after the chart
opens its `data.tradingview.com` WebSocket. If that line is missing, reload the
TradingView tab after reloading the extension. If it is present but no signal is
sent, verify that the Pine script emits `alert()` with a JSON message and that
the TradingView chart WebSocket actually contains `du ... alertMessages[]`
frames.

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

Four suites, all via `node --test`:

- **Unit** (`tests/parser.test.mjs`): pure-function parser tests, 65+ cases (action/symbol/price/direction parsing, raw_action/raw_direction, makeEventId 4-level resolution, resolveTriggerMetadata, DEFAULTS).
- **Integration** (`tests/integration.test.mjs`): jsdom + real DOM pipeline. Loads `content_scripts/lib/parser.js` and `content_scripts/tradingview_alerts.js` into a jsdom environment with mocked `chrome.runtime.sendMessage`. Verifies the actual production DOM-to-payload path on real TradingView toast HTML, including: dynamic insertion, characterData mutations, multi-description guard, 5s dedup window, pct triggers, parsed JSON overrides, and BUY command parsing.
- **Private/WebSocket feed** (`tests/private_feed.test.mjs`): jsdom + page hook
  tests for `pushstream.tradingview.com/message-pipe-ws/private_feed` and the
  chart data socket. Verifies that `pricealerts/alert_fired` becomes a
  normalized payload, duplicate indicator `alertMessages[]` are forwarded once,
  and lifecycle/non-target events are ignored.
- **Popup** (`tests/popup.test.mjs`): jsdom popup checks for live log refresh from `chrome.storage.onChanged`.

Running:
```bash
cd browser_extensions/tradingview-alert-extension
npm ci                  # reproducible install from package-lock.json
npm test                # all suites
npm run test:unit       # unit only
npm run test:integration # integration only
npm run test:private-feed # private feed hook only
```

Standalone (no jsdom):
```bash
node --test tests/parser.test.mjs
```

## Troubleshooting fetch errors

The extension classifies fetch errors into 3 honest categories:

- **`timeout`** — `AbortError` after 3000ms. Bridge is hung. Check that the bridge process is responsive.
- **`network_or_cors`** — `TypeError` "Failed to fetch". Cannot distinguish between bridge offline / connection refused / CORS preflight rejected from a single error object. To diagnose: send a manual `OPTIONS` request from the bridge host (see "Bridge requirements" below).
- **`other`** — Any other error. Inspect the popup log for full message.

The popup status uses the separate `/health` endpoint to detect a reachable
bridge before any signal fires. A green `online` status means the bridge origin
is reachable and returns a successful health response. If signal sending still
fails after that, inspect the POST/CORS/secret path.

`fetch` uses `mode: "cors"` and `credentials: "omit"` explicitly. Cookies from the extension are never sent to the bridge.

## Troubleshooting content script errors

`Extension context invalidated` means Chrome invalidated an already-injected
content script, usually because the extension was reloaded while the TradingView
tab stayed open. Reloading the extension alone does not replace the old script
inside the existing page. Reload the TradingView chart tab after reloading the
extension.

Newer extension builds catch this case and stop the old observer quietly, but
Chrome's `chrome://extensions` error list can still show old stored errors until
you remove them from that page.

## Bridge requirements

The local bridge at `http://127.0.0.1:6560` must:
- Accept `GET /health` and return `200 OK` with CORS headers
- Accept `POST /api/v1/tradingview/signal` with `Content-Type: application/json` and `X-OptionX-Secret` header
- Answer `OPTIONS` (CORS preflight) with `Access-Control-Allow-Origin: chrome-extension://<extension-id>` (or `*` for dev)
- Include `Access-Control-Allow-Headers: Content-Type, X-OptionX-Secret`
- Return `200 OK` on successful signal processing

**Diagnostic tip**: To distinguish `network_or_cors` failures:

```bash
curl -X OPTIONS http://127.0.0.1:6560/api/v1/tradingview/signal \
  -H "Origin: chrome-extension://<extension-id>" \
  -H "Access-Control-Request-Method: POST" \
  -i
```

- Connection refused / DNS failure → `network` issue (bridge offline)
- 200 OK without CORS headers → `cors` issue (bridge missing preflight handler)
- Other response → check bridge logs

## Notes

- The class name from the first observed toast looked like
  `description-ULNSeceN`, but the suffix is generated. The extension uses broad
  text/container detection instead of exact class matching.
- The content script suppresses identical toast text for 5 seconds. The local
  bridge must still have its own duplicate guard.
- If localhost requests fail, open the extension popup and check the recent
  event log.
