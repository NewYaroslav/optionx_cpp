# Examples

The examples directory contains small programs that are intended to build
against the current public aggregate headers.

Currently maintained examples:

- `account_info_hub_example.cpp` demonstrates routing account information
  updates through `AccountInfoHub` into an `IAccountInfoSubscriber`
  implementation.
- `intrade_market_data_example.cpp` demonstrates Intrade Bar market-data
  subscriptions, stream status callbacks, direct bar history, and history routed
  through `MarketDataContinuityService`. Live authenticated lifecycle reads
  `OPTIONX_INTRADE_EMAIL` and `OPTIONX_INTRADE_PASSWORD` from the environment.
- `market_data_hub_example.cpp` demonstrates routing provider callbacks through
  `MarketDataHub` into an `IMarketDataSubscriber` implementation.
- `trading_condition_hub_example.cpp` demonstrates routing payout, session and
  expiration-limit changes through `TradingConditionHub`, plus querying the
  merged current condition snapshot for a concrete symbol.
- `tradingview_extension_bridge_smoke.cpp` starts the local TradingView
  browser-extension HTTP bridge and can run `--self-test` to POST a sample
  indicator signal to itself.
- `metatrader_file_bridge_smoke.cpp` runs the C++ side of the MetaTrader
  Common\Files bridge against a temporary command/event layout.
- `metatrader_file_command_writer_smoke.cpp` demonstrates the C++ command-writer
  companion used by tests and non-MQL clients to append file commands.
- `trade_record_db_example.cpp` demonstrates basic `TradeRecordDB` usage.

MetaTrader-facing examples live under `mql/` in terminal-like layouts:

- `mql/MQL5/Include/OptionX/OptionXFileBridge.mqh`
- `mql/MQL5/Indicators/OptionX/OptionXFileBridgeSignalExample.mq5`
- `mql/MQL4/Include/OptionX/OptionXFileBridge.mqh`
- `mql/MQL4/Indicators/OptionX/OptionXFileBridgeSignalExample.mq4`

Use `scripts/install-metatrader-mql.ps1` to copy the matching `OptionX` include
and indicator trees into a terminal data folder. Portable terminals are
supported by passing the path from MetaTrader's `File -> Open Data Folder` via
`-TargetPath`.

Use `scripts/compile-metatrader-mql.ps1` to compile those MQL examples with a
local MetaEditor installation.

Old exploratory broker and component probes were removed because they referenced
pre-refactor include paths and contained local credentials. Broker smoke flows
live under `tests/intrade_bar_api/` and read credentials from local environment
files instead.
