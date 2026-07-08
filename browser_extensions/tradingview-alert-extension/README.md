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
  "raw": {
    "title": "Alert on EURUSD",
    "description": "EURUSD Crossing 1.14072"
  }
}
```

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
