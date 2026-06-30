# Examples

The examples directory contains small programs that are intended to build
against the current public aggregate headers.

Currently maintained examples:

- `trade_record_db_example.cpp` demonstrates basic `TradeRecordDB` usage.

Old exploratory broker and module probes were removed because they referenced
pre-refactor include paths and contained local credentials. Broker smoke flows
live under `tests/intrade_bar_api/` and read credentials from local environment
files instead.
