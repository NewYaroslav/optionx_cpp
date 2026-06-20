# Intrade Bar Smoke CLI

`intrade_bar_smoke_cli` is a manual smoke tool for checking individual Intrade
Bar HTTP workflows through the normal platform modules. It is not a parser
fixture and it does not bypass proxy/auth guards.

## Build

Configure and build tests from the repository root:

```powershell
cmake -G "MinGW Makefiles" -S . -B build-codex -DBUILD_DEPS=ON -DBUILD_TESTS=ON "-DCMAKE_POLICY_VERSION_MINIMUM=3.5"
cmake --build build-codex --target intrade_bar_smoke_cli -- -j4
```

The executable is created here:

```text
build-codex\intrade_bar_smoke_cli.exe
```

## Configuration

Create an untracked local env file from the example:

```powershell
Copy-Item tests\intrade_bar_api\intrade_bar_api.env.example tests\intrade_bar_api\intrade_bar_api.local.env
```

Fill in credentials and proxy in `*.local.env`, then point the CLI to it:

```powershell
$env:OPTIONX_INTRADE_BAR_CONFIG_FILE="E:\_repoz\optionx_cpp\tests\intrade_bar_api\intrade_bar_api.local.env"
```

The CLI also reads normal environment variables. Environment variables override
values from `OPTIONX_INTRADE_BAR_CONFIG_FILE`.

Required for broker access:

```text
OPTIONX_INTRADE_BAR_EMAIL
OPTIONX_INTRADE_BAR_PASSWORD
OPTIONX_INTRADE_BAR_PROXY
```

`OPTIONX_INTRADE_BAR_PROXY` uses this format:

```text
host:port:user:password
```

Alternatively set proxy pieces separately:

```text
OPTIONX_INTRADE_BAR_PROXY_SERVER
OPTIONX_INTRADE_BAR_PROXY_AUTH
OPTIONX_INTRADE_BAR_PROXY_TYPE
```

Automatic CLI broker commands refuse to run with credentials unless proxy
settings are present.

## Commands

Show command list:

```powershell
.\build-codex\intrade_bar_smoke_cli.exe help
```

Check authorization:

```powershell
.\build-codex\intrade_bar_smoke_cli.exe auth
```

Check fresh login vs saved session:

```powershell
.\build-codex\intrade_bar_smoke_cli.exe auth-cache
```

Expected output shape:

```text
fresh_ms=8336 cached_ms=3215 session_saved=1 faster=1
```

Show account state after auth:

```powershell
.\build-codex\intrade_bar_smoke_cli.exe show-account
```

Expected output shape:

```text
auth callback=1 success=1 elapsed_ms=3468
connected=1 account_type=DEMO currency=USD balance=10000 open_trades=0
```

Check automatic domain discovery:

```powershell
.\build-codex\intrade_bar_smoke_cli.exe domain-check --domain-min=0 --domain-max=1000
```

`domain-check` temporarily enables `auto_find_domain`, runs the normal
authorization flow through proxy, and prints the selected host reported by the
platform's `AutoDomainSelectedEvent`.

Expected output shape:

```text
domain_check auto_find_domain=1 domain_min=0 domain_max=1000 timeout_ms=90000
auth callback=1 success=1 elapsed_ms=4200
domain selected=1 success=1 host=https://intrade.bar
connected=1 account_type=DEMO currency=USD balance=10000 open_trades=0
```

The same range can be configured without CLI flags:

```text
OPTIONX_INTRADE_BAR_AUTO_FIND_DOMAIN=1
OPTIONX_INTRADE_BAR_DOMAIN_MIN=0
OPTIONX_INTRADE_BAR_DOMAIN_MAX=1000
```

Use a negative `domain-min` when the primary `https://intrade.bar` domain must
be excluded from discovery:

```powershell
.\build-codex\intrade_bar_smoke_cli.exe domain-check --domain-min=-1 --domain-max=1000
.\build-codex\intrade_bar_smoke_cli.exe domain-check --domain-min=-35 --domain-max=35
```

Get one quote snapshot:

```powershell
.\build-codex\intrade_bar_smoke_cli.exe quotes --symbol=EURUSD
```

`BTCUSD` is accepted as an alias for the broker-side `BTCUSDT` symbol:

```powershell
.\build-codex\intrade_bar_smoke_cli.exe quotes --symbol=BTCUSD
.\build-codex\intrade_bar_smoke_cli.exe quotes --symbol=BTCUSDT
```

Expected output shape:

```text
auth callback=1 success=1 elapsed_ms=3639
EURUSD bid=1.14911 ask=1.14914 mid=1.14913
```

Check automatic account/currency recovery:

```powershell
.\build-codex\intrade_bar_smoke_cli.exe switch-check --confirm --account-type=DEMO --currency=USD
```

`switch-check` first connects with the target settings, then temporarily
switches the broker to opposite settings through a second runtime. The original
runtime must notice the mismatch through its connected balance poll and recover
back to the target account type/currency.

If the broker account refuses switching to REAL, use a currency-only source
state while still verifying the same recovery path:

```powershell
.\build-codex\intrade_bar_smoke_cli.exe switch-check --confirm --account-type=DEMO --currency=USD --from-account-type=DEMO --from-currency=RUB
```

Useful options:

```text
--account-type=DEMO
--currency=USD
--from-account-type=REAL
--from-currency=RUB
--timeout-ms=120000
```

For smoke runs the connected balance poll is intentionally fast:

```text
OPTIONX_INTRADE_BAR_BALANCE_CHECK_PERIOD_MS=15000
OPTIONX_INTRADE_BAR_SETTINGS_SWITCH_TIMEOUT_MS=120000
OPTIONX_INTRADE_BAR_SETTINGS_SWITCH_RETRY_TIMEOUT_MS=600000
OPTIONX_INTRADE_BAR_SETTINGS_SWITCH_RETRY_DELAY_MS=15000
OPTIONX_INTRADE_BAR_SETTINGS_SWITCH_ACTIVE_TRADE_BUFFER_MS=5000
```

When testing switch retries blocked by open trades, make the auth timeout longer
than the longest demo trade:

```powershell
$env:OPTIONX_INTRADE_BAR_AUTH_TIMEOUT_MS="480000"
$env:OPTIONX_INTRADE_BAR_SETTINGS_SWITCH_RETRY_TIMEOUT_MS="480000"
```

Open a guarded demo trade:

```powershell
.\build-codex\intrade_bar_smoke_cli.exe open-trade --confirm --symbol=EURUSD --amount=1 --duration=60 --buy
```

Other trade options:

```powershell
.\build-codex\intrade_bar_smoke_cli.exe open-trade --confirm --symbol=EURUSD --amount=1 --duration=60 --sell
```

`open-trade` will not run unless one of these is true:

```text
--confirm
OPTIONX_INTRADE_BAR_ALLOW_TRADE=1
```

Real-account trades are refused unless both are set:

```text
--allow-real
OPTIONX_INTRADE_BAR_ALLOW_REAL_TRADE=1
```

Keep `OPTIONX_INTRADE_BAR_ACCOUNT_TYPE=DEMO` for routine smoke checks.

## Logs And Data

Because the executable lives in `build-codex`, runtime data is created under:

```text
build-codex\data\
```

Session DB:

```text
build-codex\data\db\session_data\mdbx.dat
build-codex\data\db\session_data\mdbx.lck
```

Logs:

```text
build-codex\data\logs\
build-codex\data\logs\unique_logs\
```

By default CLI stdout stays compact and workflow logs go to log files. To mirror
internal workflow logs to console:

```powershell
$env:OPTIONX_INTRADE_BAR_CLI_CONSOLE_LOG="1"
```

Turn it off again:

```powershell
Remove-Item Env:OPTIONX_INTRADE_BAR_CLI_CONSOLE_LOG -ErrorAction SilentlyContinue
```

## Exit Codes

```text
0  command succeeded
1  broker workflow failed or timed out
2  CLI usage/configuration guard refused to run
3  auth-cache succeeded but cached login was not faster
```

## Safety Notes

Do not use automated negative-auth checks with the real broker account. Failed
login attempts can lock the account for hours.

Do not commit `*.local.env`. The repository ignores
`tests/intrade_bar_api/*.local.env` for this reason.
