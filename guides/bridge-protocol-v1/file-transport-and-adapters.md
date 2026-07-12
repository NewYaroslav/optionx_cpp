# Bridge Protocol v1 Draft - File Transport And Legacy Adapters

## Scope

This section describes two related but separate ideas:

- the OptionX file-drop transport, a protocol binding for clients that cannot
  use HTTP, WebSocket or named pipes;
- legacy adapter profiles for third-party tools whose public or observed
  integration surface is file-based or HTTP-like but not the OptionX protocol.

The English version is canonical. The Russian mirror should be updated after
the English text is changed.

## OptionX File-Drop Transport

The file-drop transport is intended mainly for MT4/MT5 indicators, advisors and
marketplace packages. MetaQuotes marketplace rules and user environments often
make raw sockets, local HTTP servers or WebSocket clients harder to ship than
plain file I/O. A file transport lets the MQL side write requests into the
MetaQuotes common files sandbox and lets the bridge write responses, events,
balances and trade snapshots back to files.

Default root:

```text
%APPDATA%\MetaQuotes\Terminal\Common\Files
```

Recommended OptionX subdirectory:

```text
%APPDATA%\MetaQuotes\Terminal\Common\Files\OptionX\Bridge\<bridge_id>\<client_id>\
```

Recommended layout:

```text
requests\      client -> bridge JSON-RPC commands
responses\     bridge -> client JSON-RPC responses
events\        bridge -> client JSON-RPC notifications
archive\       optional processed-file retention
errors\        optional malformed/unprocessable-file retention
```

The exact root may be configured because terminals, portable installations and
VPS images can place the common files directory differently. The protocol uses
relative subdirectories once a root is configured.

### File Message Shape

Each request file contains exactly one UTF-8 JSON-RPC request document:

```json
{
  "jsonrpc": "2.0",
  "id": "mql5-ea-0001",
  "method": "trade.open",
  "params": {
    "context": {
      "idempotency_key": "mql5:terminal-01:bar-1783476720:buy",
      "client_created_at_ms": 1783476720120,
      "valid_until_ms": 1783476729000
    },
    "routing": {
      "selector": {
        "kind": "default"
      }
    },
    "trade": {
      "symbol": "EURUSD",
      "order_type": "BUY",
      "option_type": "SPRINT",
      "amount": "1.00",
      "expiry": {
        "kind": "duration",
        "duration_ms": 60000
      }
    }
  }
}
```

Responses are normal JSON-RPC responses written to `responses\` and correlated
by the original request `id`:

```json
{
  "jsonrpc": "2.0",
  "id": "mql5-ea-0001",
  "result": {
    "status": "accepted",
    "final": false,
    "operation_id": "op-019c..."
  }
}
```

Events are normal JSON-RPC notifications written to `events\`. This includes
trade updates, balance updates and reports:

```json
{
  "jsonrpc": "2.0",
  "method": "account.balance.updated",
  "params": {
    "event_id": "evt-019c...",
    "source": "optionx://installation-019c/bridge/file",
    "stream_id": "file-bridge-instance-019c...",
    "seq": 42,
    "occurred_at_ms": 1783476725000,
    "emitted_at_ms": 1783476725010,
    "subject": {
      "account_id": "1"
    },
    "revision": 3,
    "payload": {
      "account_id": "1",
      "balance": {
        "value": "1024.50",
        "currency": "USD"
      }
    }
  }
}
```

### Supported Commands

The file transport should support the same business methods as other
transports, but the MVP should prioritize the methods that make MQL integration
useful without HTTP:

- `signal.submit`
- `signal.buffers.submit`
- `trade.open`
- `operation.get`
- `trade.result.get`
- `trade.active.query`
- `trade.history.query`
- `account.list`
- `account.balance.get`
- `trading.pause`
- `trading.resume`
- `reports.query`

This keeps the file API useful enough for real integrations: it can submit
signals, open trades, ask whether an operation or trade is complete, read the
current balance and inspect active/history snapshots.

### Atomicity And Cleanup

Writers must avoid exposing half-written files:

1. Write to a temporary name in the same directory, for example
   `request-id.json.tmp`.
2. Flush and close the file.
3. Atomically rename it to a ready name, for example `request-id.json`.

Consumers should claim a ready file by atomic rename into a processing name or
processing directory. If a file is locked, the consumer should retry later
instead of treating it as malformed.

Recommended filename pattern:

```text
<unix_ms>_<client_id>_<request_id>_<method>.json
```

Rules:

- File names should be ASCII and path-safe.
- File content is UTF-8 JSON. A BOM should not be required.
- Multiline JSON is allowed for file messages, but compact JSON is easier for
  simple MQL readers.
- `context.idempotency_key` is still required for trade-affecting commands.
- `context.valid_until_ms` is strongly recommended because file polling can
  introduce delay.
- The bridge should retain processed files for a configurable window before
  deleting or archiving them.
- The bridge must not delete foreign or unknown files immediately. Unknown
  files should be ignored or moved to `errors\` only when the watched
  directory is dedicated to OptionX.
- Duplicate files are handled through idempotency and domain deduplication.
- A cleanup worker may remove old `responses\`, `events\`, `archive\` and
  `errors\` files after retention expires.

### Authentication And Client Identity

The file transport normally relies on local OS permissions and per-client
directories. It should not require API keys inside JSON-RPC `params`.

Recommended model:

- The bridge config maps a watched directory to an authenticated client id and
  permission scopes.
- If a shared API key is needed, it is configured as transport metadata for
  that directory, not as a business field.
- On Windows, directory ACLs should be used where possible to restrict writers.
- Each client should get a separate directory when several advisors, terminals
  or tools run on the same machine.

### Polling And Replay

File transport has no live socket. Clients poll `responses\` and `events\` at a
configured interval.

Recommendations:

- MQL clients should keep the last processed event filename or event
  `stream_id + seq`.
- Bridges may emit event files in sequence order, but clients must deduplicate
  by `event_id`.
- Durable replay can be represented by writing retained event notifications
  into `events\` before new live events.
- `replay.completed` can also be written as a JSON-RPC control notification
  file when the client requested replay.

## Legacy Adapter Profiles

Legacy adapter profiles are not new OptionX protocol surfaces. They are bridge
implementations that translate between a third-party format and the OptionX
domain model.

### BotBinary

BotBinary has two automation surfaces according to the local manual:

- an HTTP/WebRequest command with a `request` query field;
- a file signal directory watched by the bot.

Observed HTTP shape:

```text
http://127.0.0.2/?request=frxEURAUD=CALL=1.00=duration=5=m=
```

Observed file signal naming shape:

```text
R_25=PUT=1=duration=5=m=2018.09.29=1538190215.txt
R_50=CALL=1=endtime=1538264736=s=2018.09.29=1538264736.txt
```

Field meaning:

```text
symbol=PUT/CALL=amount=duration/endtime=value=s/m/h=unique_suffix
```

Notes:

- `duration` uses seconds, minutes or hours depending on the time unit.
- `endtime` uses Unix time in seconds.
- The default file signal directory is under the MetaQuotes common files
  sandbox, typically `Common\Files\Signal\`, but BotBinary can configure a
  custom `SignalPath`.
- BotBinary can be modeled as one bridge family with two delivery strategies:
  `http` and `file`. The domain mapping is the same; only the transport writer
  differs.
- Internally, it is still reasonable to split transport implementations, for
  example `BotBinaryHttpTransport` and `BotBinaryFileTransport`, behind one
  `BotBinaryBridge` facade.
- Account selection, balance reading and result history depend on BotBinary's
  observable API surface. If the program does not expose machine-readable
  balance or result files, the bridge should report that limitation through
  capabilities and `report.created` diagnostics instead of pretending the data
  is available.

Mapping draft:

| OptionX field | BotBinary field |
| --- | --- |
| `symbol` | first token, for example `frxEURAUD` or `R_25` |
| `order_type = BUY` | `CALL` |
| `order_type = SELL` | `PUT` |
| `amount` | stake token |
| `expiry.kind = duration` | `duration=<value>=s/m/h` |
| `expiry.kind = absolute` | `endtime=<unix_seconds>=s/m/h` |
| `unique_hash` | unique filename suffix or generated request id |

### MT2Trading File Signals

MT2Trading does not publish a stable public file API in the observed material.
The current evidence is an observed compatibility file written by the MT5
manual connector:

```text
%APPDATA%\MetaQuotes\Terminal\Common\Files\mt52bot.dat
```

Observed line shape:

```text
mt2trade_<target>,<opaque_hex_payload>
```

Observed target prefixes:

| Prefix | Observed target |
| --- | --- |
| `mt2trade_all` | all configured brokers |
| `mt2trade_iq` | IQ Option |
| `mt2trade_binary` | Binary.com / Deriv |
| `mt2trade_spectre` | Spectre.Ai |
| `mt2trade_alpari` | Alpari |
| `mt2trade_insta` | InstaBinary |
| `mt2trade_clm` | CLM |
| `mt2trade_gc` | GC Option |
| `mt2trade_pocket` | Pocket Option |
| `mt2trade_bitness` | Bitness |
| `mt2trade_tester` | MT2Trading strategy tester / simulator |

The second field is an opaque encoded payload. Several captured samples differ
when martingale settings change, and the same payload shape appears under
different target prefixes. The payload should not be treated as a decoded public
contract until the encoding is understood and tested.

`mt2trade_tester` is an observed non-broker target for MT2Trading's strategy
tester. In OptionX terms this should map closer to `source.kind = "backtest"`
or `PlatformType::SIMULATOR` than to a live broker account.

Observed payload facts:

- The payload is Base16/hex text, not Base64. It uses only `0-9A-F`, has even
  length, and each two characters decode to one byte.
- Captured samples include decoded lengths of 224, 240 and 256 bytes.
- The first bytes currently form two repeated payload families, for example
  `67 27 75 95 ...` and `3B 30 31 D5 ...`. These may correspond to different
  signal/action templates, but the mapping is not proven by the current logs.
- Within the same family and byte length, changes tend to affect 16-byte
  blocks. This suggests a block-oriented encoding or encryption layer rather
  than plain text.
- Do not decode the payload as UTF-8 text and do not infer trade fields from it
  until controlled fixtures identify which bytes correspond to direction,
  amount, expiration, martingale mode and symbol.

Implementation notes:

- Treat MT2Trading support as a separate compatibility adapter, for example
  `MT2TradingFileBridge`, not as the canonical OptionX file transport.
- The adapter can watch or write the known file only in an explicit
  compatibility mode.
- The adapter should model the line prefix as an observed target selector,
  separate from the opaque payload. Unknown `mt2trade_*` prefixes should be
  reported and ignored unless explicitly enabled.
- The target selector should be preserved in parsed metadata even when the
  opaque payload cannot be decoded.
- The adapter should not truncate or delete lines immediately. If cleanup is
  enabled, it should keep a retention window to avoid racing MT2Trading, which
  also appears to clean consumed signals.
- `unique_hash` can be derived from the full line or from a stable hash of the
  opaque payload.
- Result and balance support should be reported as unsupported unless a stable
  MT2Trading-readable source is identified.
- Decoding or generating opaque payloads is a research task and should have
  fixture-based tests from captured samples before being enabled for trading.
- Useful fixture labels for the next research pass: target prefix, direction,
  symbol, amount, expiration, martingale mode, terminal version and connector
  version.

## Open Questions

- Exact retention defaults for request, response, event, archive and error
  files.
- Whether the first production file transport should use JSON only or also
  support a compact line-oriented format for very small MQL scripts.
- Exact MQL4/MQL5 sample code for atomic write, polling and event dedupe.
- Whether BotBinary balance/result can be obtained from a machine-readable
  source or only from the UI/report screen.
- Whether MT2Trading `mt2trade_*` payloads can be decoded safely enough to
  support generation, not only observation.
