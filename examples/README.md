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
- `trade_record_db_example.cpp` demonstrates basic `TradeRecordDB` usage.

Old exploratory broker and component probes were removed because they referenced
pre-refactor include paths and contained local credentials. Broker smoke flows
live under `tests/intrade_bar_api/` and read credentials from local environment
files instead.
