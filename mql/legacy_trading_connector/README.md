# Legacy Trading Connector

This package contains the MetaTrader 4 and MetaTrader 5 source files for the
legacy OptionX named-pipe trading bridge.

The connector speaks the historical MegaConnector V1 JSON protocol:

- sends `contract` messages to `optionx::bridges::named_pipe::LegacyTradingBridge`;
- receives `update_bet` trade-result messages;
- receives balance, account, connection, and ping messages.

The default pipe name is intentionally still `intrade_bar_console_bot` for
compatibility with existing legacy setups. The C++ bridge name is broker-neutral;
the old protocol was originally used with Intrade Bar, but the wire format does
not depend on a specific broker.

## Layout

- `MQL4/Include` and `MQL5/Include` contain the legacy `MegaConnector/v1`
  support headers.
- `MQL4/Indicators/.../LegacyTradingConnector` contains the MT4 indicator entry
  points.
- `MQL5/Indicators/.../LegacyTradingConnector` contains the MT5 indicator entry
  points.

The MQL sources are imported with minimal changes from the old connector
release. Internal `MegaConnector/v1` names and some UI strings are preserved so
the package remains close to the code that was used in terminals. A future
non-legacy bridge should use a new package and protocol instead of extending
this one.

## Install

Copy the contents of either `MQL4` or `MQL5` into the corresponding terminal
data folder, preserving the `Include` and `Indicators` directory structure.

Compile `LegacyTradingConnector.mq4` or `LegacyTradingConnector.mq5` in
MetaEditor. The C++ CI does not compile MQL files.
