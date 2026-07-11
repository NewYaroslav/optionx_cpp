# TradingView Test Indicators

These Pine Script files are manual test fixtures for the TradingView extension
and private chart WebSocket research.

Official TradingView references:

- <https://www.tradingview.com/pine-script-docs/faq/alerts/>
- <https://www.tradingview.com/pine-script-docs/concepts/alerts/>
- <https://www.tradingview.com/support/solutions/43000531021-how-to-use-a-variable-value-in-alert/>

## `optionx_noisy_test_signals.pine`

Primary test indicator. It uses `alert()` calls to emit dynamic JSON messages
from Pine at runtime.

Manual setup in TradingView:

1. Open a liquid chart, for example `EURUSD`, `BTCUSDT` or another active
   symbol.
2. Set the timeframe to `1m`.
3. Open Pine Editor.
4. Paste the contents of `optionx_noisy_test_signals.pine`.
5. Click Add to chart.
6. Create an alert.
7. In Condition, choose `OptionX Noisy Test Signals` and then
   `Any alert() function call`.

`alert()` is the preferred fixture for the bridge because it can build a dynamic
message while the script executes. The emitted JSON includes fields such as
`symbol`, `tickerid`, `price` and `time` without relying on TradingView's alert
message placeholder substitution.

The script intentionally reacts on the realtime bar by default. Signals can
appear, disappear and appear again while the current bar is still moving. This
is useful for testing duplicate suppression and repaint-sensitive workflows.

For production-style checks, enable the script input `Confirmed bars only`. In
that mode the fixture requires `barstate.isconfirmed` and uses
`alert.freq_once_per_bar_close`, so the Pine alert is emitted only after the bar
closes. This is the recommended setup when the user wants signals that should
not disappear before bar close.

## `optionx_noisy_test_alertcondition.pine`

Comparison fixture for `alertcondition()`. It uses the same RSI crossing logic,
but defines separate `OptionX BUY` and `OptionX SELL` alert conditions.

Manual setup is the same as above, except in Condition choose either
`OptionX BUY` or `OptionX SELL` instead of `Any alert() function call`.

This variant is here to verify whether TradingView sends the same private
WebSocket shape for `alertcondition()` alerts:

```text
du -> <study-id>.ns.d -> data.alertMessages[] -> msg
```

Expected differences:

- `alertcondition()` can only use a constant message in Pine source;
- dynamic values come from TradingView placeholders such as `{{ticker}}`,
  `{{exchange}}`, `{{close}}` and `{{time}}`;
- `time` is expected as an ISO UTC string placeholder, while the `alert()`
  fixture sends Pine's numeric bar timestamp.

Until this variant is captured from the private WebSocket, treat `alert()` as
the confirmed path for indicator signal JSON.
