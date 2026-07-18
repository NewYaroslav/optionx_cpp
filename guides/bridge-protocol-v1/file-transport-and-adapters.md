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
%APPDATA%\MetaQuotes\Terminal\Common\Files\OptionX\Bridge\v1\<bridge_id>\<client_id>\
```

Recommended layout:

```text
commands.ndjson          MT4/MT5 -> bridge append-only command log
events.ndjson            bridge -> MT4/MT5 append-only event/result log
state.json               bridge -> MT4/MT5 latest state snapshot
commands.checkpoint.json bridge-owned reader checkpoint, optional
events.checkpoint.json   MT-owned reader checkpoint, optional
```

The exact root may be configured because terminals, portable installations and
VPS images can place the common files directory differently. The protocol uses
relative subdirectories once a root is configured.

The canonical MetaTrader file transport is intentionally line-oriented rather
than one-file-per-message. Each append log has one writer:

- the MQL side is the only writer of `commands.ndjson`;
- the bridge is the only writer of `events.ndjson`;
- the bridge is the only writer of `state.json`.

This keeps the format easy to implement in MQL4/MQL5. Readers keep their own
checkpoint and never rewrite another side's append log. When several runtime
objects or processes can write the same logical log, they must share a per-log
exclusive lock.

### C++ MVP Surface

The first C++ implementation slice intentionally exposes only reusable
file-transport building blocks:

- `bridges/metatrader_file.hpp` as the umbrella include;
- `MetaTraderFileBridgeConfig` for the MetaQuotes Common Files root, client
  directory identity, polling interval and NDJSON line limits;
- `MetaTraderFileCommandWriter` as the C++ client-side counterpart to the MQL
  `COptionXFileBridge` header; it appends canonical `signal.submit`,
  `trade.open` and `account.balance.get` requests;
- `metatrader_file::detail` helpers for path-safe IDs, NDJSON append/read,
  owner-side log cleanup, JSON-RPC request/response/notification documents,
  atomic state snapshots and bounded JSON reads;
- `utils::metatrader` helpers for resolving the default MetaQuotes roaming
  location, Common Files root and terminal data directories;
- small helpers for `balance.updated` and `trade.updated` notification payloads.

The bridge side is implemented by `MetaTraderFileBridge`. The C++ client side is
intentionally smaller: `MetaTraderFileCommandWriter` is a reusable command
writer, analogous to the MQL `COptionXFileBridge` header. It is not a background
transport loop and does not read events or state snapshots. It should be used by
smoke tools, tests and C++ applications that need to create
MetaTrader-compatible command logs.

### Writer Helpers And Smoke Generator

The C++ client-side writer appends one compact JSON-RPC request per line to
`commands.ndjson`. It assigns `file_seq` and adds `context.valid_until_ms` for
trade-affecting commands. The caller must provide a stable
`context.idempotency_key` for `signal.submit` and `trade.open` and persist it
before retrying the same logical operation; `file_seq` is a transport sequence
and must not be used as the retry identity of a trade-affecting operation.
One-shot query commands such as `account.balance.get` may use a writer-generated
JSON-RPC `id`.

Supported convenience methods:

- `MetaTraderFileCommandWriter::signal_submit(...)`;
- `MetaTraderFileCommandWriter::trade_open(...)`;
- `MetaTraderFileCommandWriter::account_balance_get(...)`.

The example `examples/metatrader_file_command_writer_smoke.cpp` writes one
`account.balance.get`, one `signal.submit` and one `trade.open` command. In
self-test mode it uses a temporary Common Files root and verifies that the
generated log contains `file_seq`, JSON-RPC `id`, caller-provided
`context.idempotency_key` and `context.valid_until_ms`.

Owner-side cleanup is explicit: the writer may clear `commands.ndjson` only
after `commands.checkpoint.json.last_file_seq` is greater than or equal to the
greatest visible `file_seq` in the command log. Clearing before that checkpoint
can drop commands that the bridge has not processed yet.

### MQL4/MQL5 Client Header

The sample MQL client is laid out like a real terminal data folder. The header
lives in:

```text
mql/MQL4/Include/OptionX/OptionXFileBridge.mqh
mql/MQL5/Include/OptionX/OptionXFileBridge.mqh
```

It exposes a small `COptionXFileBridge` class with the same three command
helpers:

- `AccountBalanceGet(...)`;
- `SignalSubmit(...)`;
- `TradeOpen(...)`.

Copy the matching `mql/MQL4` or `mql/MQL5` tree into the terminal data folder,
or copy only the `OptionX` subfolders into existing `MQL4\Include` /
`MQL4\Indicators` or `MQL5\Include` / `MQL5\Indicators` directories. Include
the header from MQL as:

```mql
#include <OptionX/OptionXFileBridge.mqh>
```

Ready-to-run examples are provided as:

- `mql/MQL4/Indicators/OptionX/OptionXFileBridgeSignalExample.mq4`;
- `mql/MQL5/Indicators/OptionX/OptionXFileBridgeSignalExample.mq5`.

The examples print the resolved client root and command log path with `Print`,
send an `account.balance.get` request and submit a simple signal for the current
chart symbol. `trade.open` is available behind an input flag so the example does
not open a direct trade accidentally.

`SignalSubmit(...)` and `TradeOpen(...)` require an explicit `operation_key`.
The MQL caller should create that key before appending the command, persist it
as part of its own retry state, and reuse the same value when retrying the same
logical signal or trade. The header uses this key as JSON-RPC `id`,
`context.idempotency_key` and, when no separate domain identity is available,
`identity.unique_hash`.

`AccountBalanceGet(...)` remains a one-shot query helper. If the caller does not
provide an `operation_key`, the MQL header generates a transport-local request
id after reserving `file_seq` under the command-log lock:

```text
mql:<bridge_id>:<client_id>:<base36(file_seq)>
```

The MQL header opens text files with `CP_UTF8` and shared read/write flags. It
also repairs an incomplete tail before appending the first new command after a
restart. If `commands.checkpoint.json` exists but is unreadable or malformed, or
if `commands.ndjson` cannot be scanned safely, the helper fails closed and does
not write a new command with a reset `file_seq`.

### MetaTrader Discovery Utility

MetaTrader path discovery is a reusable utility, not ad-hoc logic inside the
file bridge. The utility is intended for the file transport, quote translators,
MQL sample tooling and future MT4/MT5 adapters.

Responsibilities:

- resolve the default MetaQuotes roaming directory on Windows through the OS
  known-folder API, with `%APPDATA%` only as a fallback;
- expose the default Common Files root:
  `%APPDATA%\MetaQuotes\Terminal\Common\Files`;
- enumerate known terminal data directories under
  `%APPDATA%\MetaQuotes\Terminal\<terminal-hash>\`;
- classify terminals by the presence of `MQL4` or `MQL5` directories;
- return per-terminal `MQL4\Files` / `MQL5\Files` directories when they exist;
- accept explicitly configured terminal or Common Files roots without trying to
  guess over them;
- keep path confinement and reserved-name checks separate from discovery so the
  bridge can validate configured and discovered roots the same way.

C++ entry points live under `optionx::utils::metatrader` and are exposed through
`optionx_cpp/utils.hpp` and `optionx_cpp/bridges/metatrader_file.hpp`.

The older `mega-connector` code has useful prior art in
`tools/mt/common/utils.hpp` (`SHGetKnownFolderPath`, terminal enumeration and
history-folder discovery), but the OptionX utility should be redesigned around
small testable helpers instead of copying the old application-specific API.

### File Message Shape

`commands.ndjson` and `events.ndjson` are UTF-8 NDJSON files. One complete
line is one compact JSON document. A line without a trailing `\n` is
incomplete and must not be processed yet.

Each log record has a monotonic `file_seq` assigned by that log's writer. The
sequence is transport-local and exists to let the reader skip records already
processed before cleanup. It does not replace JSON-RPC `id`,
`context.idempotency_key`, domain trade IDs or event `stream_id + seq`.

Example command line in `commands.ndjson`:

```json
{"file_seq":1,"jsonrpc":"2.0","id":"mql5-ea-0001","method":"trade.open","params":{"context":{"idempotency_key":"mql5:terminal-01:bar-1783476720:buy","client_created_at_ms":1783476720120,"valid_until_ms":1783476729000},"routing":{"selector":{"kind":"default"}},"trade":{"symbol":"EURUSD","order_type":"BUY","option_type":"SPRINT","amount":{"value":"1.00","currency":"USD"},"expiry":{"kind":"duration","duration_ms":60000}}}}
```

The bridge may answer through events instead of a separate response log. For a
trade request, `trade.accepted`, `trade.rejected` and later `trade.updated` /
`trade.closed` events can all reference the original JSON-RPC `id`.
Implementations that want a strict JSON-RPC response may append a response-like
event to `events.ndjson`, but there is no separate `responses` directory in the
canonical MetaTrader file binding.

Example bridge event line in `events.ndjson`:

```json
{"file_seq":101,"jsonrpc":"2.0","method":"trade.accepted","params":{"request_id":"mql5-ea-0001","operation_id":"op-019c...","trade_id":"784512"}}
```

Domain event notifications can still use the normal bridge event envelope:

```json
{"file_seq":102,"jsonrpc":"2.0","method":"balance.updated","params":{"event_id":"evt-019c...","source":"optionx://installation-019c/bridge/file","stream_id":"file-bridge-instance-019c...","seq":42,"occurred_at_ms":1783476725000,"emitted_at_ms":1783476725010,"subject":{"account_id":"1"},"revision":3,"payload":{"account_id":"1","balance":{"value":"1024.50","currency":"USD"}}}}
```

`state.json` is an atomically replaced snapshot, not an append log:

```json
{
  "version": 56,
  "updated_at_ms": 1784030400000,
  "connection": "connected",
  "accounts": [
    {
      "account_id": "1",
      "balance": {
        "value": "1018.50",
        "currency": "USD"
      }
    }
  ],
  "open_trades": [
    {
      "trade_id": "784513",
      "symbol": "EURUSD",
      "order_type": "BUY"
    }
  ]
}
```

### Supported Commands

The file transport should support the same business methods as other
transports, but the MVP should prioritize the methods that make MQL integration
useful without HTTP:

- `protocol.hello`
- `protocol.capabilities.get`
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
current balance, inspect active/history snapshots and discover the exact
methods and limits supported by a reduced file-based bridge. Explicit event
subscription commands can be added later if a file client needs something more
selective than the default `events.ndjson` stream.

### Atomicity And Cleanup

Append-log writers must avoid exposing half-written records:

1. Serialize the whole JSON document in memory.
2. Write compact JSON plus one trailing `\n`.
3. Flush the file.

A reader processes only complete lines. A final line without `\n` is considered
incomplete and must be retried on the next poll.

On startup, and before the first append after startup, a writer must repair its
own append log if the file ends with an incomplete line. The repair truncates
the file to the last complete LF-terminated record, or to an empty file when no
complete record exists. This prevents a crashed half-record from being glued to
the next valid record.

The single-writer rule is per logical log and includes all threads/tasks inside
that writer. Repair, sequence recovery, append and owner-side clear operations
for the same log must be serialized. If more than one runtime object or process
can touch the same path, use a per-log lock file or equivalent OS file lock; an
in-process mutex alone is not sufficient. The low-level append helper is
intentionally unlocked.

Before appending, a writer must serialize the exact UTF-8 bytes that will be
written, including the trailing LF, and check:

```text
current_log_size + encoded_record_bytes <= max_command_log_bytes
```

If there is not enough space, the writer may first clear its own log only when
the reader checkpoint confirms that all currently visible records were consumed.
Otherwise it must fail before append; it must not publish a record that pushes
the log beyond the configured bridge read limit.

Readers must enforce `max_line_bytes` while streaming the file, not after
loading the full tail into memory. A complete malformed line may be skipped and
reported as transport diagnostics; the reader checkpoint may advance past that
complete malformed line. Missing, zero, negative, fractional or non-numeric
`file_seq` values make the complete line malformed at the transport layer. An
incomplete final line is not processed and does not advance the checkpoint.

`state.json` and checkpoint files are not append logs. They must be written via
same-directory temporary file and atomic replacement:

```text
state.json.tmp.<id> -> state.json
commands.checkpoint.json.tmp.<id> -> commands.checkpoint.json
```

Cleanup is owner-driven:

- The MQL side may clear `commands.ndjson` because it is the only writer.
- The bridge may clear `events.ndjson` because it is the only writer.
- A reader must never remove or rewrite the other side's append log.
- A writer should clear a log only after the reader checkpoint confirms that
  all records up to the intended `file_seq` have been processed.

Writers must keep `file_seq` monotonic across cleanup cycles. After
`commands.ndjson` is cleared, the next command must use a greater `file_seq`
than any command written before cleanup. This lets the bridge safely read the
file from the beginning and skip already processed records by `last_file_seq`.
Byte offsets are allowed only as an optimization.

After restart, a writer should initialize its next sequence as:

```text
next_file_seq = max(last file_seq currently visible in the writer-owned log,
                    reader checkpoint last_file_seq) + 1
```

If the writer stores a durable next-sequence value, that value must also be
included in the maximum. The writer must not restart at `1` while the reader
checkpoint still points to a later sequence.

Baseline checkpoint shape:

```json
{
  "last_file_seq": 102
}
```

Persisted byte offsets are not part of the baseline cleanup-safe checkpoint.
A reader may keep a byte offset only as an optimization while it can prove that
the append-log identity has not changed since the offset was recorded, for
example through a future `log_generation` or equivalent file identity. Without
that proof, after restart or cleanup the reader must scan the current file from
the beginning and filter by `last_file_seq`. The reader must still use
idempotency records to avoid duplicate trade execution.

Polling helpers may limit both the number of scanned complete non-empty lines
and the number of returned new records per poll. The scan limit counts accepted,
already-seen and malformed complete lines so malformed input cannot make one
poll accumulate unbounded diagnostics.

Trade-affecting commands still require `context.idempotency_key`. Concrete
polling bridges should also require `context.valid_until_ms` because file
polling can introduce delay and append logs may be retained longer than the
dedupe window. If a command with the same JSON-RPC `id` or idempotency key is
seen again within the configured idempotency retention window, the bridge must
return or re-emit the original/current operation result instead of creating a
second trade. After that retention window, duplicate suppression is no longer
guaranteed; stale retained commands should be rejected by `valid_until_ms`.
For idempotency comparison, `context.valid_until_ms` and
`context.client_created_at_ms` are admission/attempt metadata rather than
business payload. They must not make an otherwise identical retry conflict, but
the bridge still validates `valid_until_ms` before accepting a new operation.

This draft intentionally does not define log rotation. Production
implementations may add owner-side compaction later, but the baseline profile is
append, checkpoint and clear.

Deferred implementation work:

- Higher-level MQL helpers for writing and compacting these logs.
- A runtime writer object or owner queue that serializes append, repair and
  owner-side clear operations per log file.
- Optional `log_generation`/file identity support if persisted byte-offset
  optimization becomes necessary.
- A callback/visitor NDJSON reader if future logs need very large scans without
  accumulating records.

### Authentication And Client Identity

The file transport normally relies on local OS permissions and per-client
directories. It should not require API keys inside JSON-RPC `params`.

Recommended model:

- Directory identity is authenticated only when OS permissions isolate the
  writer. Otherwise file transport is a trusted-local-client profile inside the
  same OS user boundary.
- The bridge config maps a watched directory to a configured client id and
  permission scopes.
- If a shared API key is needed, it is configured as transport metadata for
  that directory, not as a business field.
- On Windows, directory ACLs should be used where possible to restrict writers.
- Each client should get a separate directory when several advisors, terminals
  or tools run on the same machine.
- For untrusted same-user processes, prefer named pipes with ACLs or another
  transport with credential authentication.

### Polling And Replay

File transport has no live socket. The bridge polls `commands.ndjson` and the
MQL side polls `events.ndjson` and `state.json` at configured intervals.

Recommendations:

- For the first MetaTrader bridge implementation, prefer a small always-on
  event stream over explicit `events.subscribe` management. The MQL side can
  ignore event types it does not need.
- A later implementation may accept `events.subscribe` commands in
  `commands.ndjson`, but the result should still be reflected through
  `events.ndjson` and `state.json`, not through per-subscription delivery
  directories.
- `state.json` is authoritative for current connection/account/open-trade
  state. `events.ndjson` is a change log and may be cleaned by the bridge after
  the MQL checkpoint confirms it was read.
- Replay after reconnect is simple: a reader loads its last checkpoint and
  processes complete records with a greater `file_seq`. If the append log was
  cleared, the reader scans from the beginning of the current file and still
  filters by `last_file_seq`.
- Domain event deduplication still uses `event_id` and domain `stream_id + seq`
  when those fields are present.

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
| `expiry.kind = absolute` | `endtime=<unix_seconds>=s` |
| `context.idempotency_key` | persisted adapter mapping to the exact BotBinary request or filename |
| `identity.unique_hash` | external stable signal identity only when the suffix is known to represent one |

The adapter must not generate a new BotBinary filename for a retry of the same
OptionX `context.idempotency_key`. It should persist:

```text
OptionX idempotency_key -> exact BotBinary request/filename
```

The BotBinary filename suffix may be copied to `identity.unique_hash` only when
it is confirmed to be a stable domain identity of the signal or trade. A suffix
that merely makes one file unique is transport identity, not domain
deduplication.

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

#### Non-Normative Research Notes

The following MT2Trading notes are black-box research observations, not the
OptionX wire contract. They must not be treated as normative behavior for live
broker dispatch.

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
- A controlled no-martingale fixture with amount `1.0` and expiration `5m`
  produced two 224-byte payloads, one from the `3B 30 31 D5 ...` family and one
  from the `67 27 75 95 ...` family. Repeating the test later produced the same
  two payloads, so this fixture does not show a changing timestamp inside the
  payload. The two payloads differ in every 16-byte block, so this fixture
  identifies two stable direction templates, but not the BUY/SELL mapping unless
  the exact button-click order is recorded.
- A reported controlled comparison from expiration `5m` to `7m` changed exactly
  one 16-byte block in both direction families: block 5, byte offset
  `0x50..0x5F` / decimal bytes `80..95`. All other 208 bytes in the 224-byte
  payload stayed identical. This strongly suggests independently encoded
  parameter blocks rather than one CBC-like stream over the whole payload.
- A reported equal-length signal-name comparison `Manual -> Test00` changed
  block 4 (`0x40..0x4F`) plus dynamic blocks 6 and 7, while keeping expiration
  block 5 and the tail configuration blocks unchanged. This makes block 4 the
  current best candidate for the serialized `signal_name` field when the name
  length is six bytes. A shorter-name test such as `Manual -> Test` may shift
  later serialized fields and therefore changes many following blocks.
- Reported BUY fixtures with a six-byte signal name currently suggest this
  block map: block 0 = direction/template or command type, block 2 = amount,
  block 4 = signal name, block 5 = expiration, blocks 6-7 = dynamic data, and
  blocks 8-13 = martingale configuration plus runtime chain state. This is a
  working map, not a stable contract.
- Reported tail blocks 8-13 have at least two byte-exact states. The sequence
  `Manual #1 -> state A`, `Manual #2 -> state B`, `Test00 #1 -> state A`
  suggests that the tail may include martingale/runtime state keyed by
  `signal_name`, not only static martingale settings.
- The current high-probability implementation hypothesis is MQL-native
  `CryptEncode(CRYPT_AES128/AES256)` followed by an `ArrayToHex`-style loop.
  This matches the uppercase two-hex-characters-per-byte output and explains why
  the same MT4/MT5 code path would be portable without a custom DLL.
- The observed behavior is functionally ECB-like: the same logical plaintext
  block appears to produce the same ciphertext block, and changing one logical
  field changes only the corresponding 16-byte block. MQL `CryptEncode()` does
  not expose an IV argument, so a normal CBC mode with transmitted random IV is
  unlikely through that API. This does not prove the exact mode name, but it
  makes DES and unencrypted binary serialization much less likely.
- AES-128 and AES-256 cannot be distinguished from the ciphertext alone because
  both use a 16-byte block size. The difference is only the key length.
- Block lookup tables, if ever built, are research/tester-only artifacts. They
  must never be enabled for live broker dispatch unless the connector version,
  plaintext serialization, key derivation or key material, encryption mode,
  padding, dynamic fields, integrity behavior and BUY/SELL mapping are fully
  confirmed.

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
- The opaque payload hash should be stored as metadata, for example
  `metadata.opaque_payload_sha256`. Do not populate `identity.unique_hash` from
  the opaque payload until fixtures prove that the same logical signal retry
  maps to the same domain identity and different logical signals map to
  different identities.
- Result and balance support should be reported as unsupported unless a stable
  MT2Trading-readable source is identified.
- Decoding or generating opaque payloads is a research task and should have
  fixture-based tests from captured samples before being enabled for trading.
- If the adapter ever needs to generate MT2Trading payloads, the practical
  research paths are controlled black-box fixtures, vendor documentation, or
  authorized source-level instrumentation of code the integrator is allowed to
  inspect. Without that, generation remains out of scope for live trading.
- Useful fixture labels for the next research pass: target prefix, direction,
  symbol, amount, expiration, martingale mode, terminal version and connector
  version.
- Useful next fixtures: repeat `5m`, `7m` and another duration with all other
  parameters fixed; repeat the same BUY twice and the same SELL twice with file
  cleanup between clicks; then change amount and martingale mode one at a time.
- To test chain state, use fresh six-byte signal names such as `AAAAAA` and
  `BBBBBB`: send `AAAAAA` twice, then `BBBBBB` once. If the hypothesis is
  correct, the expected tail sequence is `A`, `B`, `A`.

## Open Questions

- Exact cleanup policy for `commands.ndjson` and `events.ndjson` once the peer
  checkpoint confirms records were processed.
- Exact MQL4/MQL5 sample code for appending NDJSON, polling complete lines,
  atomically replacing `state.json` and deduplicating event IDs.
- Whether BotBinary balance/result can be obtained from a machine-readable
  source or only from the UI/report screen.
- Whether MT2Trading `mt2trade_*` payloads can be decoded safely enough to
  support generation, not only observation.
