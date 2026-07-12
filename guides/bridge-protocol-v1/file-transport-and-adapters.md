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
requests\ready\          client -> bridge JSON-RPC commands ready to claim
requests\processing\     bridge-owned claimed request files
requests\archive\        optional processed request retention
requests\errors\         malformed/unprocessable request retention
responses\ready\         bridge -> client JSON-RPC responses ready to claim
responses\processing\    client-owned claimed response files
events\ready\            bridge -> client JSON-RPC notifications ready to claim
events\processing\       client-owned claimed event files
archive\                 optional shared retention area
errors\                  optional shared malformed/unprocessable-file retention
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
  "method": "balance.updated",
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
- `events.subscribe`
- `events.unsubscribe`
- `events.subscriptions.list`

This keeps the file API useful enough for real integrations: it can submit
signals, open trades, ask whether an operation or trade is complete, read the
current balance, inspect active/history snapshots and discover the exact
methods, limits and event topics supported by a reduced file-based bridge.

### Atomicity And Cleanup

Writers must avoid exposing half-written files:

1. Write to a temporary name in the same directory, for example
   `<unix_ms>_<file_uuid>.json.tmp`.
2. Flush and close the file.
3. Atomically rename it into the `ready\` directory with its final name, for
   example `<unix_ms>_<file_uuid>.json`.

Consumers should claim a ready file by atomic rename into a processing name or
processing directory. If a file is locked, the consumer should retry later
instead of treating it as malformed.

Recommended filename pattern:

```text
<unix_ms>_<file_uuid>.json
```

Rules:

- File names should be ASCII and path-safe.
- `file_uuid` is a transport-level identifier only. JSON-RPC correlation still
  uses the `id` field inside the file content.
- `id`, `method` and client identity are read from file content and bridge
  configuration, not from the filename.
- Writers should create final filenames with no-overwrite semantics.
- File content is UTF-8 JSON. A BOM should not be required.
- Multiline JSON is allowed for file messages, but compact JSON is easier for
  simple MQL readers.
- `context.idempotency_key` is still required for trade-affecting commands.
- `context.valid_until_ms` is strongly recommended because file polling can
  introduce delay.
- Claimed request files older than `processing_lease_ms` should be recovered by
  moving them back to `requests\ready\` or by processing them again. Reprocessing
  is safe only when idempotency records are honored: the bridge must return the
  original or current operation result instead of creating another trade.
- Response and event consumers should claim files into `responses\processing\`
  or `events\processing\`, store their local checkpoint, then delete or archive
  the claimed files. Stale client-owned processing files may be restored after a
  lease timeout.
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

File transport has no live socket. Clients poll `responses\` and `events\` at a
configured interval.

Recommendations:

- `events.subscribe`, `events.unsubscribe` and `events.subscriptions.list` are
  valid file-transport requests. File subscriptions are durable and
  client-scoped because there is no live socket session.
- File subscription idempotency is scoped as:

  ```text
  configured client + method + client_subscription_key
  ```

- `events.subscribe` defines topics, filters and `replay.mode`, exactly as in
  the general event contract. The bridge writes only the subscribed topics into
  the client's `events\ready\` directory.
- MQL clients should keep the last processed event filename or event
  `stream_id + seq`.
- Bridges may emit event files in sequence order, but clients must deduplicate
  by `event_id`.
- Durable replay can be represented by writing retained event notifications
  into `events\` before new live events.
- `replay.completed` can also be written as a JSON-RPC control notification
  file after retained replay events and before live events when the client
  requested replay.

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

- Exact retention defaults for request, response, event, archive and error
  files.
- Whether the first production file transport should use JSON only or also
  support a compact line-oriented format for very small MQL scripts.
- Exact MQL4/MQL5 sample code for atomic write, polling and event dedupe.
- Whether BotBinary balance/result can be obtained from a machine-readable
  source or only from the UI/report screen.
- Whether MT2Trading `mt2trade_*` payloads can be decoded safely enough to
  support generation, not only observation.
