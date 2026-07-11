# TradingView Bridge Research

Research note for future TradingView bridge work. Checked on 2026-07-08.

## Goal

We want TradingView to provide signal intent, not position sizing. For
`optionx_cpp` the safe boundary is:

```text
TradingView signal
    -> bridge protocol parser
    -> TradeSignal
    -> duplicate/risk/money-management layer
    -> TradeRequest
    -> platform execution
```

`amount` should normally come from our side: bridge config, account/risk rules,
or money management. TradingView can send direction, symbol, order id, price,
time and position metadata. `{{strategy.order.contracts}}` is useful metadata,
but it must not become `TradeRequest::amount` by default for binary/options
platforms.

## Official Webhook Path

Source:
[TradingView webhook help](https://www.tradingview.com/support/solutions/43000529348-how-to-configure-webhook-alerts/),
[TradingView strategy alerts](https://www.tradingview.com/support/solutions/43000481368-strategy-alerts/),
[TradingView alert placeholders](https://www.tradingview.com/support/solutions/43000531021-how-to-use-a-variable-value-in-alert/),
[TradingView pricing](https://www.tradingview.com/pricing/).

TradingView webhooks send an HTTP POST to the configured URL. The alert message
is the request body. If the message is valid JSON, TradingView sends it with
`application/json`; otherwise it sends `text/plain`.

Technical constraints from the official help page:

- only ports `80` and `443` are accepted when a URL includes a port;
- request processing is cancelled after roughly 3 seconds;
- IPv6 is not supported for webhooks;
- delivery can fail, so the alert log webhook status must be treated as
  diagnostic evidence, not a guaranteed delivery ledger;
- webhook alerts require two-factor authentication.

Pricing/current product shape:

- TradingView lists alert counts and `Webhook notifications` as plan features.
- The video review showed the practical user-facing issue: free alerts exist,
  but webhook delivery was unavailable on the free plan in that UI.
- Therefore the official path should be documented as stable but not free for
  our target user.

Useful strategy placeholders:

- `{{strategy.order.action}}` -> `buy` or `sell`;
- `{{strategy.order.contracts}}` -> number of contracts in the strategy order;
- `{{strategy.order.price}}` -> order execution price;
- `{{strategy.order.id}}` and `{{strategy.order.comment}}` -> order identity
  and user metadata;
- `{{strategy.order.alert_message}}` -> Pine-provided custom message;
- `{{strategy.market_position}}`, `{{strategy.prev_market_position}}` and
  position-size variants -> context for entry/exit/reversal handling;
- generic placeholders such as `{{ticker}}`, `{{exchange}}`, `{{close}}`,
  `{{time}}`, `{{timenow}}`.

Recommended official webhook alert body:

```json
{
  "version": 1,
  "source": "tradingview_webhook",
  "secret": "shared-token",
  "event_id": "{{strategy.order.id}}:{{time}}:{{strategy.order.action}}",
  "symbol": "{{exchange}}:{{ticker}}",
  "action": "{{strategy.order.action}}",
  "price": "{{strategy.order.price}}",
  "bar_time": "{{time}}",
  "trigger_time": "{{timenow}}",
  "strategy": {
    "order_id": "{{strategy.order.id}}",
    "order_comment": "{{strategy.order.comment}}",
    "contracts": "{{strategy.order.contracts}}",
    "market_position": "{{strategy.market_position}}",
    "prev_market_position": "{{strategy.prev_market_position}}"
  }
}
```

## Video Notes

Source: <https://www.youtube.com/watch?v=D5Inyt2RalQ>

The video demonstrates a direct `TradingView Alert webhook -> Bybit Webhook
Signal Trading` flow. Useful observations for our bridge design:

- The exchange-side webhook has a secret URL and a message template. These must
  be treated as credentials.
- The exchange-side receiver may interpret quantity as contracts, not money.
- Test with the smallest safe contract/lot first.
- TradingView strategy alerts can continue to fire from TradingView servers,
  even if the chart browser tab is not actively driving the strategy.
- Direct exchange webhooks are broker/platform-specific. For `optionx_cpp` the
  reusable part is the alert message contract, not the Bybit UI flow.

## Free/Local Alternatives

Free alternatives avoid TradingView's webhook delivery. They trade stability for
local convenience.

### Browser Extension

Most relevant for a desktop/local bridge:

```text
TradingView tab
    -> Chrome/Edge extension
    -> localhost HTTP or WebSocket
    -> local app using optionx_cpp bridge/parser
```

Possible signal sources:

- visible Strategy Tester `List of Trades` DOM;
- visible alert toast DOM;
- visible alert log or notification UI;
- explicit text embedded into strategy entry/comment names;
- browser notifications, if the browser exposes enough structured text;
- internal TradingView push streams.

Signal source choice depends on the TradingView script type:

- `strategy()` scripts can produce Strategy Tester orders. The visible
  `List of Trades` route can work for these scripts, provided the user keeps
  the relevant TradingView tab/panel available.
- `indicator()` scripts with `alertcondition()` or `alert()` do not necessarily
  produce Strategy Tester trades. For these scripts, a `List of Trades` scraper
  is not enough; the bridge must capture the alert itself from the alert log,
  notification surface, email path or TradingView's internal alert stream.

Low-risk MVP should avoid internal TradingView push streams while proving the
local protocol and receiver. That does not mean pushstream is irrelevant. For a
generic free bridge that must handle arbitrary indicator alerts without paid
webhooks, internal alert delivery may be the only practical low-latency source.
Treat that as a separate high-fragility mode, not as the baseline architecture.

### Visible Alert Toast

Observed on 2026-07-08: a price-level alert produced a TradingView toast with
visible text `Alert on EURUSD` and description:

```html
<span class="description-ULNSeceN">EURUSD Crossing 1.14072</span>
```

This is useful because it is not tied to Strategy Tester trades. A content
script can watch TradingView's toast container with `MutationObserver`, extract
the alert description and the nearby symbol/title, then forward the raw alert
message to the local bridge.

Important caveats:

- the class suffix is generated and must not be treated as stable; selectors
  should look for a toast/notification container and text roles/patterns, not
  exactly `description-ULNSeceN`;
- simple level alerts such as `EURUSD Crossing 1.14072` are market events, not
  trade intents. The local protocol must either map them through user rules or
  require indicator/alert messages to include explicit `buy`/`sell`;
- if several alerts fire quickly, the extension must deduplicate by visible text
  plus timestamp/symbol window.

### Private Chart WebSocket

Observed on 2026-07-08 from a browser capture of:

```text
wss://data.tradingview.com/socket.io/websocket?from=chart/<layout-id>/&date=<iso-time>&type=chart&auth=sessionid
```

This is TradingView's private chart data channel. It is useful research for a
quotes bridge, but it is not a stable public API and should be treated as an
experimental/high-fragility integration.

Important: the first captured dump did not prove that alert events are
delivered on this socket. It did show chart data, quote data and study data. A
user-visible alert firing near a price level can coincide with ordinary
`qsd`/`du` updates; that alone is not evidence of an alert-delivery message.

Framing:

- messages use TradingView's `~m~<length>~m~<payload>` framing;
- JSON payloads have a method name in `m` and positional params in `p`;
- heartbeats look like `~m~4~m~~h~<n>` and the client echoes them back.

Auth/session shape:

- the browser opens the WebSocket in the authenticated TradingView page
  context;
- the client sends a `set_auth_token` frame after the socket opens;
- captured tokens/cookies are account credentials and must never be committed,
  logged in full or forwarded to the local bridge.

Observed outgoing client methods:

- `set_auth_token`
- `set_locale`
- `chart_create_session`
- `quote_create_session`
- `quote_set_fields`
- `switch_timezone`
- `quote_add_symbols`
- `resolve_symbol`
- `create_series`
- `create_study`

Observed incoming methods:

- `symbol_resolved` - symbol metadata such as exchange, pricescale, sessions
  and description;
- `series_loading`, `series_completed`, `series_timeframe` - chart series
  lifecycle;
- `timescale_update` - historical bars for the requested series. A sample bar
  vector was shaped like `[time, open, high, low, close, volume]`;
- `du` - realtime updates for the active bar/study series, with the same
  compact vector shape for bars;
- `qsd` - quote data updates. Observed fields included `lp`, `ch`, `chp`,
  `volume`, `lp_time`, `bid`, `ask`, `bid_size`, `ask_size`, `short_name`,
  `pro_name` and `exchange`;
- `study_loading`, `study_completed` - indicator/study lifecycle;
- `quote_completed` - quote subscription completion/status.

#### Study Alert Messages

A later focused capture did show a useful signal-shaped payload inside a `du`
study update. It was not a separate `m: "alert"` method. The path was:

```text
du.p[1].<study-id>.ns.d
    -> JSON string
    -> data.alertMessages[]
    -> msg
    -> JSON signal string
```

Observed shape, sanitized:

```json
{
  "barInfo": {
    "barIndex": 10320,
    "close": 1.14054,
    "high": 1.14064,
    "low": 1.14053,
    "open": 1.14063,
    "time": 1783476180000,
    "updateTime": 1783476196715,
    "volume": 54
  },
  "msg": "{\"source\":\"tradingview\",\"signal_name\":\"noisy_rsi_test\",\"action\":\"sell\",\"symbol\":\"EURUSD\",\"tickerid\":\"FX:EURUSD\",\"price\":1.14054,\"time\":1783476180000}"
}
```

This is very useful for indicator-driven signals because the inner `msg` can be
our own JSON alert contract. The capture contained the same alert payload twice
under two different study ids, so local code must deduplicate by at least:

- parsed `msg`;
- `barInfo.time` or parsed signal `time`;
- `barInfo.updateTime`;
- chart session/study id only as diagnostic metadata, not as the primary event
  id.

Extraction contract for this private API mode:

```text
TradingView frame
    -> unwrap ~m~<length>~m~ payload
    -> JSON.m == "du"
    -> JSON.p[0] as chart_session
    -> each property in JSON.p[1] as study_or_series_id
    -> property.ns.d, if non-empty
    -> parse property.ns.d as JSON
    -> data.alertMessages[]
    -> parse alertMessages[].msg as the bridge signal JSON
```

Recommended normalized local fields:

- `source_kind`: `private_chart_study_alert_messages`;
- `chart_session`: `du.p[0]`;
- `study_id`: current key from `du.p[1]`, diagnostic only;
- `bar`: copied from `alertMessages[].barInfo`;
- `message`: parsed `alertMessages[].msg`;
- `dedupe_key`: hash of parsed `msg`, or
  `tickerid|signal_name|action|time|price`.

Interpretation:

- this confirms that some indicator/study alert messages can be observed through
  the private chart WebSocket;
- it does not yet prove that all TradingView platform alerts, especially plain
  price-level alerts, are delivered this way;
- it may depend on how the Pine script emits `alert()`/`alertcondition()` and on
  whether the study is loaded in the open chart.

Confirmed Pine source for the captured `noisy_rsi_test` messages is saved at
`browser_extensions/tradingview-alert-extension/examples/optionx_noisy_test_signals.pine`.
It uses RSI crossing a center level and emits explicit JSON via `alert()`:

```json
{
  "source": "tradingview",
  "signal_name": "noisy_rsi_test",
  "action": "buy",
  "symbol": "EURUSD",
  "tickerid": "FX:EURUSD",
  "price": 1.14055,
  "time": 1783476660000
}
```

The test indicator intentionally reacts on the realtime bar. Its plotted signal
can disappear and appear again as the bar updates and RSI crosses back around
the center level. That is useful for stress-testing capture and deduplication.
For a less noisy production-style script, emit only on confirmed bars or use a
bar-close alert frequency.

TradingView's Pine docs describe `alert()` as the more flexible path for dynamic
runtime messages. `alertcondition()` remains useful for separate selectable
conditions such as `BUY` and `SELL`, but its Pine `message` is a constant string;
dynamic values must come from placeholders. Because our confirmed WebSocket
captures came from `alert()` calls, `alertcondition()` must be captured
separately before we treat it as equivalent for the private WebSocket bridge.
The comparison fixture is saved at
`browser_extensions/tradingview-alert-extension/examples/optionx_noisy_test_alertcondition.pine`.

#### Price-Level Alert Capture

A focused EURUSD-filtered capture around a manual price-level crossing alert
showed only quote and chart/study update frames:

- `qsd` quote updates for `FX:EURUSD`;
- `du` bar updates for `sds_1`;
- `du` numeric study output for `ZntB35`;
- empty `ns.d` fields on all `du` frames.

No `alertMessages`, `msg`, `Crossing`, notification payload or alert-like method
was present in that chart WebSocket fragment. Because the capture was filtered
to EURUSD, it is not conclusive for TradingView's full alert delivery path.
However, it is another data point that plain price-level alerts may arrive via
DOM toast, alert log, pushstream/EventSource or another channel rather than the
chart data WebSocket.

#### Private Price Alert Pushstream WebSocket

A later capture confirmed that visible price-level popup alerts are delivered
through a separate TradingView pushstream WebSocket, not through the chart data
WebSocket:

```text
wss://pushstream.tradingview.com/message-pipe-ws/private_feed
```

Observed request shape:

- WebSocket upgrade from the authenticated TradingView browser session;
- `Origin: https://www.tradingview.com`;
- no alert payload in the URL itself;
- browser cookies/session state are still sensitive credentials and must stay
  inside the browser boundary.

Observed message wrapper:

```json
{
  "id": 31,
  "text": {
    "content": {
      "m": "alert_fired",
      "p": {},
      "id": "emrv-244490662",
      "_rts": 1783478179763
    },
    "channel": "pricealerts"
  }
}
```

Confirmed methods on `channel: "pricealerts"`:

- `alerts_created` - alert creation/configuration snapshot;
- `alert_fired` - the actual event that should become a local bridge alert;
- `alerts_updated` - alert state synchronization after fire, activation,
  deactivation or auto-stop. Useful for diagnostics, but should not by itself
  create a trade/signal event.

Observed `alert_fired.p` fields for a level crossing:

```json
{
  "fire_id": 53256558326,
  "alert_id": 5099741779,
  "symbol": "={\"symbol\":\"FX:EURUSD\",\"adjustment\":\"splits\",\"session\":\"regular\",\"currency-id\":\"USD\"}",
  "pro_symbol": "={\"symbol\":\"FX:EURUSD\",\"adjustment\":\"splits\",\"session\":\"regular\",\"currency-id\":\"USD\"}",
  "bar_time": "2026-07-08T02:36:00Z",
  "message": "EURUSD Crossing 1.14110",
  "popup": true,
  "cross_interval": true,
  "fire_time": "2026-07-08T02:36:22Z",
  "kinds": ["regular"],
  "resolution": "1"
}
```

Observed `alerts_updated.p[]` adds alert configuration/state:

- `type: "price"`;
- `condition.type`, observed as `cross`, `cross_up` and `cross_down`;
- `condition.series[1].value`, for example `1.1411`;
- `frequency: "on_first_fire"`;
- `active`;
- `auto_deactivate`;
- `last_fire_time`;
- `last_fire_bar_time`;
- `last_stop_reason`, for example `auto`;
- `web_hook`, `email`, `popup`, `mobile_push`.

A later capture around manual crossing alerts showed why `alerts_updated` must
not be treated as a fired signal. While the user adjusted or recreated a level,
TradingView emitted multiple state updates for the same `alert_id` without
`fire_id` and without `last_fire_time`:

```text
alerts_created  active=false  cross_up    1.14113
alerts_updated  active=true   cross_up    1.14113
alerts_updated  active=false  cross_up    1.14112
alerts_updated  active=true   cross_up    1.14112
alerts_updated  active=false  cross_up    1.14116
alerts_updated  active=true   cross_up    1.14116
alerts_updated  active=false  cross_up    1.14115
alerts_updated  active=true   cross_up    1.14115
alert_fired                   fire_id=53256619224
alerts_updated  active=false  last_stop_reason=auto
```

Those active false/true flips are alert lifecycle/configuration updates, not
market events. Treat them as diagnostics or state cache only. A local signal is
accepted only when `content.m == "alert_fired"` and `p.fire_id` is present.

Extraction contract for this private API mode:

```text
TradingView pushstream frame
    -> JSON.text.channel == "pricealerts"
    -> JSON.text.content.m == "alert_fired"
    -> JSON.text.content.p as price alert event
    -> parse p.symbol/pro_symbol when they start with ="{...}"
    -> normalize message into source_kind private_pricealerts_ws
```

Recommended normalized local fields:

- `source_kind`: `private_pricealerts_ws`;
- `event_id`: `tv_price_alert:<fire_id>`;
- `dedupe_key`: `fire_id`, or `alert_id|fire_time|message`;
- `symbol`: parsed `p.symbol.symbol`, for example `FX:EURUSD`;
- `message`: `p.message`;
- `action`: `alert` unless user rules map the level alert to `buy`/`sell`;
- `price`: parsed from `condition.series[1].value` if a matching
  `alerts_updated` state is available, otherwise best-effort from `message`;
- `bar_time`, `fire_time`, `resolution`, `alert_id`, `fire_id`.

Interpretation:

- price-level popup alerts are confirmed on `private_feed`/`pricealerts`;
- chart WebSocket negative captures are expected because that socket carries
  chart/quote/study data, not this alert bus;
- this is a strong candidate for the free level-alert bridge path, but it is
  still a private authenticated API and should be isolated as an experimental
  extension mode.

What this can support:

- a private quotes bridge that mirrors the currently open TradingView chart;
- diagnostic capture of symbol metadata, active timeframe and bar updates;
- study output research when an indicator is attached to the chart;
- experimental indicator signal capture from `data.alertMessages[]` in study
  `du` frames;
- experimental price-level alert capture from pushstream
  `private_feed`/`pricealerts`.

What is not confirmed yet:

- generic alert delivery over `data.tradingview.com/socket.io/websocket` for
  all alert types;
- whether `alertcondition()` and all non-price alert types use the same
  pushstream wrapper or another channel;
- whether all required messages remain available on free plans and anonymous or
  low-tier sessions.

Possible browser-extension capture strategies:

1. Page-context WebSocket/EventSource probe. Inject at `document_start` into the
   page world, wrap `window.WebSocket` and `window.EventSource`, forward only
   redacted method names and selected metadata to the content script. A normal
   isolated-world content script cannot see frames from TradingView's existing
   sockets.
2. Extension-owned TradingView socket. The extension opens a second socket and
   sends the same subscription frames. This duplicates TradingView sessions,
   needs auth/token handling and is fragile under MV3 service-worker lifetime
   rules.
3. Chrome debugger protocol. `chrome.debugger` can observe WebSocket frames, but
   it requires a very invasive permission prompt and is better suited for a
   developer-only probe than for a user-facing bridge.
4. Private pushstream WebSocket/EventSource. Level popup alerts were confirmed
   on `wss://pushstream.tradingview.com/message-pipe-ws/private_feed`, channel
   `pricealerts`. Existing third-party extensions also used TradingView private
   pushstream channels, so the probe should inspect both WebSocket and
   EventSource traffic.

Recommended handling:

- do not make the native/local bridge read Chrome cookies directly. Chrome
  cookies are protected credentials; exporting them to a local process is a
  bad product boundary and creates a large account-risk surface;
- if private capture becomes necessary, keep it inside the browser extension
  and forward only normalized, redacted events to localhost;
- first build a developer-only probe mode that records method names around a
  manual alert firing, then promote only the minimal proven source to the bridge;
- keep the current `alert_toast_dom` route as the low-risk MVP while the private
  channels are still being verified.

### Email Route

Some services implement "free webhook" by receiving TradingView alert emails and
turning them into webhooks. This is useful as a fallback concept, but it adds a
third party, latency, mailbox verification, more secrets and less local control.
It is not the best first path for a local desktop bridge.

### Native Messaging

Chrome Native Messaging is the official extension-to-native-app channel.
Chrome starts a registered native host process and exchanges length-prefixed
JSON over stdin/stdout. It is robust, but it requires OS-specific registration
of a native host manifest. Content scripts cannot call native messaging
directly; they must message the extension service worker or extension page.

Source:
[Chrome Native Messaging](https://developer.chrome.com/docs/extensions/develop/concepts/native-messaging).

Use Native Messaging later if installation packaging is already solved. For an
MVP, localhost HTTP is simpler.

## Implemented Local HTTP Bridge

The first native receiver is implemented as a header-only bridge:

- config: `include/optionx_cpp/bridges/trading_view/TradingViewExtensionBridgeConfig.hpp`;
- parser/protocol: `include/optionx_cpp/bridges/trading_view/detail/TradingViewExtensionProtocol.hpp`;
- HTTP server: `include/optionx_cpp/bridges/trading_view/TradingViewExtensionBridge.hpp`;
- smoke executable: `examples/tradingview_extension_bridge_smoke.cpp`;
- example config: `examples/tradingview_extension_bridge_smoke.config.json`.

The smoke config is parsed as JSONC by stripping comments first. This keeps the
committed example runnable while still documenting common rule variants inline.

The full HTTP bridge intentionally is not included by `include/optionx_cpp/bridges.hpp`,
because it includes `server_http.hpp` from Simple-Web-Server. The aggregate
header includes only the config. Users that need the server include the bridge
header explicitly.

Default local endpoint:

```text
POST http://127.0.0.1:6560/api/v1/tradingview/signal
GET  http://127.0.0.1:6560/health
```

When `secret` is configured, the browser extension sends it in the HTTP header
`X-OptionX-Secret`. The shared secret is not part of the JSON body by default,
so it does not flow into logs, `TradeSignal::user_data` or downstream storage.
Legacy body-secret parsing exists only behind the explicit
`allow_body_secret_fallback` config flag.

Indicator signal payload:

```json
{
  "source": "tradingview",
  "signal_name": "noisy_rsi_test",
  "action": "buy",
  "symbol": "FX:EURUSD",
  "tickerid": "FX:EURUSD",
  "price": 1.14055,
  "time": 1783476660000,
  "event_id": "indicator:eurusd:1783476660000:buy"
}
```

The parser also accepts the confirmed private `pricealerts` wrapper forwarded by
the extension:

```json
{
  "text": {
    "channel": "pricealerts",
    "content": {
      "m": "alert_fired",
      "p": {
        "fire_id": 53256556946,
        "symbol": "FX:EURUSD",
        "message": "EURUSD Crossing 1.14072"
      }
    }
  }
}
```

Only `alert_fired` can become a local signal. `alerts_created` and
`alerts_updated` are accepted as known TradingView lifecycle/state messages but
are rejected by the parser with `ignored_state_message`.

For visible toast capture, TradingView can show both a configurable alert
title/name and a separate alert message/description. The extension forwards
them as separate fields:

- `alert_name` - the configurable alert title/name, for example `Test99` or
  `BUY Test99`;
- `message` - the alert text/description, for example
  `BTCUSD Crossing BUY 64,119.59`.

The bridge keeps `message` as `TradeSignal::comment`, stores `alert_name` in
`TradeSignal::user_data`, and uses it as a `signal_name` fallback. The action
keyword resolver also inspects `alert_name`, so users can put direction words
in the TradingView title/name without mixing that title into the main comment.

Sizing is configured on our side:

- `fixed_amount` sets `TradeSignal::amount` and marks `mm_type = FIXED`;
- `balance_percent` does not calculate an amount inside the bridge. It marks
  `mm_type = PERCENT` and writes the sizing intent into `TradeSignal::user_data`
  for a downstream money-management/postprocessing layer;
- `none` leaves amount and money-management type unset.

CORS defaults are intentionally convenient for local development:
`allow_cors: true` and `allowed_origin: "*"`. For a packaged extension, set
`allowed_origin` to the concrete extension origin, for example
`chrome-extension://<extension-id>`. The preflight response allows
`Content-Type`, `X-OptionX-Secret` and `Authorization` headers.

Level alerts are accepted only when the bridge can derive a concrete direction:

- explicit payload action, for example `action: "buy"`;
- an ordered level-alert rule;
- an action keyword in the alert text;
- `default_level_action`, if the user explicitly sets it to `buy` or `sell`.

Action keywords are configured separately from level rules:

```json
{
  "action_keywords": {
    "use_defaults": true,
    "buy": ["entry long", "Покупа"],
    "sell": ["entry short", "Селл"]
  }
}
```

With `use_defaults: true`, the custom lists extend built-in English/Russian
terms such as `buy`, `call`, `long`, `up`, `sell`, `put`, `short`, `down`,
`бай`, `селл`, `покуп`, `прода`, `вверх` and `вниз`. Set it to `false` to make
the lists a full override. Keyword mapping is a fallback after matching
level-alert rules, so explicit user rules can still define or reject specific
alert shapes.

The config also contains ordered user rules. Rules without a `symbol` matcher
apply to every pair, for example:

```json
{
  "level_alert_rules": {
    "default_action": "reject",
    "rules": [
      {
        "condition_type": "crossing_up",
        "action": "buy",
        "signal_name": "level_crossing_up"
      },
      {
        "condition_type": "crossing_down",
        "action": "sell",
        "signal_name": "level_crossing_down"
      }
    ]
  }
}
```

This keeps the important product boundary explicit: TradingView tells us that a
level alert fired; only explicit text or user config decides whether that event
means buy, sell or reject. Plain `Crossing` alerts remain direction-ambiguous
unless the alert message includes a keyword such as `BUY`/`SELL` or the user
adds a generic `crossing` rule.

Smoke test:

```powershell
cmake --build <build-dir> --target tradingview_extension_bridge_smoke
<build-dir>\examples\tradingview_extension_bridge_smoke.exe --self-test
```

## GitHub References

These repositories are examples to study. Do not copy code blindly.

### `QGB/tradingview_connector`

Repository: <https://github.com/QGB/tradingview_connector>

Observed files:

- `2.6.5_0/manifest.json`
- `2.6.5_0/js/TradingConnector.js`
- `2.6.5_0/js/TradingView.js`
- `2.6.5_0/background.js`
- `2.6.5_0/content_helper.js`

What it does:

- Chrome Manifest V2 extension named `TradingView Alerts to MT4/MT5`.
- Injects a content helper into `https://*.tradingview.com/*`.
- Reads `window.user.private_channel` from TradingView page context.
- Opens `EventSource` to TradingView `pushstream` private channel.
- Uses `chrome.webRequest.onBeforeSendHeaders` to alter `Origin` for that
  stream request.
- Opens local WebSocket `ws://127.0.0.1:6560`.
- Sends a compact JSON signal shaped like `{ id, symbol, command, time }`.

Good ideas:

- local WebSocket receiver with visible connection status;
- small normalized message from extension to desktop app;
- extension badge/status feedback for "connected/disconnected".

Do not copy for our MVP:

- private TradingView pushstream dependency;
- `Origin` header modification;
- Manifest V2 and `webRequestBlocking`;
- OAuth/Web Store/subscription glue;
- code itself: no repository license was found in the cloned tree.

### `niiisho/TradingView-MT5-Bridge`

Repository: <https://github.com/niiisho/TradingView-MT5-Bridge>

License observed: MIT.

Observed files:

- `Tradingview Trade Detector (Extension)/manifest.json`
- `Tradingview Trade Detector (Extension)/content.js`
- `Tradingview Trade Detector (Extension)/logger.js`
- `README.md`

What it does:

- Chrome MV3 extension.
- Content script runs on `*://*.tradingview.com/*`.
- Uses `MutationObserver` on the visible Strategy Tester trade list.
- Seeds existing rows so old trades are not emitted as new signals.
- Detects list refreshes and suppresses false replays during refresh.
- Extracts `BUY`/`SELL`, optionally `SL=... TP=... LOT=...`, from row text.
- Sends `POST http://localhost:8080/signal`.
- Popup checks `GET http://localhost:8080/health` and stores recent logs in
  `chrome.storage.local`.

Good ideas:

- MV3, minimal permissions and localhost host permissions;
- DOM observer with seeding and refresh guard;
- duplicate suppression;
- local HTTP receiver for easy C++/curl testing;
- popup logger and health check.

Limitations:

- requires the TradingView tab and Strategy Tester list to be visible enough for
  DOM observation;
- depends on TradingView CSS/classes/text that can change;
- extracts commands from free text rather than a typed protocol;
- no shared secret in the observed extension-to-local request;
- MT5-specific assumptions do not map directly to `TradeSignal`.

This is the closest algorithmic template for our free bridge, but the protocol
should be ours.

### `nileio/TV_strategynotifier`

Repository: <https://github.com/nileio/TV_strategynotifier>

License observed: AGPL-3.0.

What it does:

- userscript for Tampermonkey/Violentmonkey style runners;
- inserts an enable/disable control into TradingView UI;
- opens a user-configured WebSocket;
- observes Strategy Tester/report DOM changes;
- builds entry/exit JSON objects with trade numbers, entry/exit IDs, price,
  comments and strategy metadata.

Good ideas:

- explicit user-controlled enable switch;
- richer entry/exit state than a plain `BUY`/`SELL` signal;
- stable local unique IDs derived from strategy/trade/time.

Limitations:

- old and brittle TradingView selectors;
- userscript install path is less product-friendly than a packaged extension;
- AGPL license makes code copying unsuitable for this project.

### `akumidv/tradingview-assistant-chrome-extension`

Repository: <https://github.com/akumidv/tradingview-assistant-chrome-extension>

License observed: GPL-3.0.

This is not a trading execution bridge. It is useful as an architecture example
for a TradingView extension:

- MV3 extension;
- clear split between popup UI, content scripts, page-context injection,
  storage, file import/export and TradingView DOM integration;
- explicit documentation that TradingView UI automation is fragile and may be
  treated as bot-like behavior;
- all TradingView DOM work is isolated in dedicated modules.

Good ideas:

- keep TradingView selectors and UI adaptation in one boundary;
- keep protocol normalization separate from DOM scraping;
- persist logs/intermediate state locally;
- document terms-of-use and account-risk boundaries prominently.

Do not copy code without checking GPL compatibility.

## Recommended Free MVP

There are two different free products hiding under one name:

1. Strategy bridge: reads visible Strategy Tester orders. This is easier and
   less invasive, but it only supports strategy-style scripts.
2. Alert bridge: captures TradingView alert events. This is what we need for
   generic signal indicators. It may require alert-log/notification scraping or
   an internal pushstream-style integration.

Current local prototype:
`browser_extensions/tradingview-alert-extension` implements the first
alert-bridge slice. It is a Chrome/Edge MV3 extension that observes visible
alert toast DOM and sends normalized JSON to a local HTTP bridge endpoint.

Prefer local HTTP first for both modes:

```text
TradingView signal source
    -> MV3 content script
    -> chrome.runtime message or direct fetch
    -> http://127.0.0.1:<port>/api/v1/tradingview/signal
    -> TradingViewExtensionProtocol::parse()
    -> TradeSignal
```

Why HTTP before WebSocket:

- easier to implement and test with `curl`;
- fits MV3 service-worker lifetime better than a permanent background socket;
- local app can expose `/health`, `/signal`, `/signal/<id>` and diagnostics;
- WebSocket can be added later for richer status and streaming logs.

WebSocket remains useful if the bridge needs live status in the extension badge.
If used in MV3, do not assume the service worker is a permanent process. Keep
the connection in a visible extension page/offscreen document or make the
content script reconnect while the TradingView tab is open.

Extension responsibilities:

- support an explicit signal-source mode:
  `strategy_tester_trades`, `alert_toast_dom`, `alert_log`,
  `browser_notification`, `private_alert_stream`,
  `private_chart_socket_probe`, `private_chart_study_alert_messages`,
  `private_pricealerts_ws`;
- for `strategy_tester_trades`, seed existing rows and emit only new rows;
- for alert-based modes, parse the final alert text rather than strategy report
  rows;
- detect list refreshes and suppress replay bursts;
- normalize visible text into an internal alert object;
- send the alert object to localhost with a shared secret in
  `X-OptionX-Secret`;
- keep a small local log and health status;
- provide an explicit enable/disable switch.

Local bridge responsibilities:

- bind only to loopback by default;
- require a shared secret/token;
- reject missing `event_id` or generate a deterministic `unique_hash`;
- deduplicate on `unique_hash`;
- normalize symbol via config map;
- map only `buy`/`sell`/known close actions to `OrderType`;
- set `TradeSignal::bridge_id`, `signal_id`, `symbol`, `order_type`,
  `signal_name`, `user_data`, `comment`, `unique_hash`;
- never accept `amount` from the extension unless a future platform-specific
  mode explicitly enables that behavior.

Suggested extension-to-local payload:

```json
{
  "version": 1,
  "source": "tradingview_extension",
  "event_id": "tv:list-of-trades:strategy-name:order-id:time:action",
  "fingerprint": "fnv1a-content-hash",
  "symbol": "OANDA:EURUSD",
  "action": "buy",
  "price": 1.08453,
  "time": "2026-07-08T00:15:00Z",
  "signal_name": "strategy-name",
  "strategy": {
    "order_id": "Long",
    "order_comment": "optional",
    "market_position": "long",
    "prev_market_position": "flat",
    "contracts": "1"
  },
  "raw": {
    "source_kind": "strategy_tester_list_of_trades"
  }
}
```

Local response:

```json
{
  "ok": true,
  "accepted": true,
  "duplicate": false,
  "unique_hash": "..."
}
```

## `optionx_cpp` Fit

Current bridge contract already matches this direction:

- `bridges::BaseBridge::on_trade_signal()` publishes `std::unique_ptr<TradeSignal>`.
- `TradeSignal::to_trade_request()` converts signal intent into executable
  `TradeRequest`.
- `bridges::named_pipe::detail::parse_contract()` is a close parser precedent:
  external JSON in, normalized `TradeSignal` out.

Possible implementation split:

- `include/optionx_cpp/bridges/trading_view/detail/TradingViewExtensionProtocol.hpp`
  for parser/formatter helpers;
- `include/optionx_cpp/bridges/trading_view/TradingViewExtensionBridgeConfig.hpp`
  for config DTO;
- optional `TradingViewExtensionBridge` only if we choose to include a local
  HTTP/WebSocket server in the library surface;
- otherwise keep the server in an application/tool and expose only protocol
  parsing in `optionx_cpp`.

Open design question: `optionx_cpp` is header-only and currently has HTTP client
helpers, not a public HTTP server abstraction. A local receiver may belong in an
example/tool layer rather than the reusable library core unless we add a small
server dependency intentionally.

## Safety Rules

- Demo account first.
- Default bridge state is disabled until the user explicitly enables it.
- No real tokens, cookies or webhook URLs in docs, tests or logs.
- Never forward TradingView cookies or auth tokens to the local bridge.
- Redact `secret`, full raw alert text and local auth material in logs.
- Keep a duplicate guard on the local side even if the extension suppresses
  duplicates.
- Treat browser-extension and DOM-based approaches as best-effort integrations:
  they can break when TradingView changes UI.
- Do not rely on private APIs for the default MVP. If a working free bridge
  requires them, isolate that mode, label it experimental and keep capture
  inside the browser extension.
- Document that account risk remains with the user.

## Next Steps

1. Decide first free signal source by target script type. Strategy Tester
   `List of Trades` is easiest to prototype for `strategy()` scripts, but it
   will not cover plain signal indicators. If generic indicators are required
   first, validate alert log/browser notification/private stream capture before
   building the strategy-trades scraper.
2. Add parser tests for a `TradingViewExtensionProtocol` that produces
   `TradeSignal` and rejects malformed/unauthorized payloads.
3. Prototype a local HTTP receiver outside the public include surface.
4. Scaffold a minimal MV3 extension with:
   `manifest.json`, `content_scripts/tradingview.js`, `service_worker.js`,
   `popup/`, and `protocol/`.
5. Add a manual smoke checklist: start local receiver, load unpacked extension,
   open TradingView Strategy Tester, enable bridge, trigger one demo signal,
   verify duplicate suppression.
6. Add a developer-only `private_chart_socket_probe` mode to the extension that
   patches WebSocket/EventSource in the page context, redacts credentials and
   records method names around one manually triggered alert. Use that capture to
   confirm whether non-price alerts arrive on `data.tradingview.com`,
   pushstream, DOM only or a different channel.
7. Prototype extraction of `du.p[1].<study-id>.ns.d.data.alertMessages[]` from
   captured WebSocket frames and feed the parsed inner `msg` through the same
   local TradingView extension protocol as DOM toasts.
8. Prototype `private_pricealerts_ws` extraction from
   `message-pipe-ws/private_feed`: consume only `pricealerts/alert_fired` as
   events, treat `alerts_updated` as state/diagnostics, and deduplicate by
   `fire_id`.
