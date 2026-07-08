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
`chrome.storage.local` and can be changed at any time.

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
  "event_id": "tv_toast:hash:timestamp",
  "dedupe_key": "alert_toast_dom|EURUSD|Alert on EURUSD|EURUSD Crossing 1.14072",
  "symbol": "EURUSD",
  "action": "alert",
  "price": 1.14072,
  "time": "2026-07-08T00:00:00.000Z",
  "title": "Alert on EURUSD",
  "message": "EURUSD Crossing 1.14072",
  "secret": "shared-token",
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

`extension.url` is included only when the popup option `Include full tab URL`
is enabled; by default only `host`, `symbol_from_url` and `interval` are sent.

Expected bridge behavior:

- bind to loopback by default;
- require `secret` if configured;
- deduplicate by `dedupe_key` or `event_id`;
- map `action` to `TradeSignal::order_type` only when it is `buy` or `sell`;
- keep `amount` on the local bridge/risk-management side.

## Notes

- The class name from the first observed toast looked like
  `description-ULNSeceN`, but the suffix is generated. The extension uses broad
  text/container detection instead of exact class matching.
- The content script suppresses identical toast text for 5 seconds. The local
  bridge must still have its own duplicate guard.
- If localhost requests fail, open the extension popup and check the recent
  event log.
