# Bridge Protocol v1 Draft - File Transport And Legacy Adapters

## Область

Этот раздел описывает две связанные, но разные вещи:

- OptionX file transport для клиентов, которые не могут использовать HTTP,
  WebSocket или named pipes;
- legacy adapter profiles для сторонних инструментов с файловым или HTTP-like
  API, который не является OptionX protocol.

Английская версия каноническая. Русский файл синхронизируется с английским
после изменения английского текста.

## OptionX File-Drop Transport

File transport в первую очередь нужен для MT4/MT5 indicators, advisors и
MetaQuotes marketplace packages. В таких окружениях обычный file I/O проще
поставлять и поддерживать, чем raw sockets, local HTTP server или WebSocket
client.

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

Точный root настраивается, потому что portable terminals и VPS images могут
располагать Common Files по-разному.

Canonical MetaTrader file transport является line-oriented, а не
one-file-per-message. У каждого append-log только один writer:

- MQL сторона пишет только `commands.ndjson`;
- bridge пишет только `events.ndjson`;
- bridge пишет `state.json` через atomic replace.

Reader хранит свой checkpoint и не переписывает append-log другой стороны.

Если несколько runtime-объектов или процессов могут писать в один логический
лог, они обязаны использовать общий per-log exclusive lock. Reader всё равно
хранит свой checkpoint и не переписывает append-log другой стороны.

### C++ MVP Surface

Первый C++ implementation slice открывает только reusable building blocks:

- `bridges/metatrader_file.hpp` как umbrella include;
- `MetaTraderFileBridgeConfig` для MetaQuotes Common Files root, client
  directory identity, polling interval и NDJSON line limits;
- `MetaTraderFileCommandWriter` для C++/tooling clients, которым нужно писать
  canonical `signal.submit`, `trade.open` и `account.balance.get` requests;
- helpers в `metatrader_file::detail` для path-safe IDs, NDJSON append/read,
  owner-side cleanup, JSON-RPC request/response/notification documents,
  atomic state snapshots и bounded JSON reads;
- helpers `utils::metatrader` для default MetaQuotes roaming location,
  Common Files root и terminal data directories;
- helpers для `balance.updated`, `trade.updated` и `state.json` payloads.

Bridge-side реализован через `MetaTraderFileBridge`. Writer-side намеренно
меньше: `MetaTraderFileCommandWriter` это reusable command generator, а не
background transport loop. Его можно использовать в smoke tools, tests и C++
applications, которые формируют MetaTrader-compatible command logs.

### Writer Helpers And Smoke Generator

C++ writer helper добавляет в `commands.ndjson` один compact JSON-RPC request
на строку. Он назначает `file_seq`, генерирует compact Base36 operation keys,
если caller не передал `id` / `context.idempotency_key`, и добавляет
`context.valid_until_ms` для commands, влияющих на торговлю.

Supported convenience methods:

- `MetaTraderFileCommandWriter::signal_submit(...)`;
- `MetaTraderFileCommandWriter::trade_open(...)`;
- `MetaTraderFileCommandWriter::account_balance_get(...)`.

Example `examples/metatrader_file_command_writer_smoke.cpp` пишет один
`account.balance.get`, один `signal.submit` и один `trade.open` command. В
self-test mode используется temporary Common Files root и проверяется, что в log
есть `file_seq`, JSON-RPC `id`, `context.idempotency_key` и
`context.valid_until_ms`.

Owner-side cleanup должен быть явным: writer может очищать `commands.ndjson`
только после того, как `commands.checkpoint.json.last_file_seq` стал больше или
равен максимальному visible `file_seq` в command log. Очистка раньше checkpoint
может удалить commands, которые bridge еще не обработал.

### MQL4/MQL5 Client Header

Sample MQL client разложен как реальный terminal data folder. Header лежит в:

```text
mql/MQL4/Include/OptionX/OptionXFileBridge.mqh
mql/MQL5/Include/OptionX/OptionXFileBridge.mqh
```

Он дает небольшой class `COptionXFileBridge` с теми же тремя helpers:

- `AccountBalanceGet(...)`;
- `SignalSubmit(...)`;
- `TradeOpen(...)`.

Скопируй подходящее дерево `mql/MQL4` или `mql/MQL5` в terminal data folder
или перенеси только subfolders `OptionX` в существующие `MQL4\Include` /
`MQL4\Indicators` или `MQL5\Include` / `MQL5\Indicators`. Header подключается
так:

```mql
#include <OptionX/OptionXFileBridge.mqh>
```

Ready-to-run examples:

- `mql/MQL4/Indicators/OptionX/OptionXFileBridgeSignalExample.mq4`;
- `mql/MQL5/Indicators/OptionX/OptionXFileBridgeSignalExample.mq5`.

Examples пишут через `Print` resolved client root и command log path,
отправляют `account.balance.get` и simple signal для текущего chart symbol.
`trade.open` доступен через input flag, чтобы example случайно не открывал
direct trade.

Если caller не передал explicit `operation_key`, MQL header генерирует его
после резервирования `file_seq` под command-log lock:

```text
mql:<bridge_id>:<client_id>:<base36(file_seq)>
```

Это делает automatically generated JSON-RPC `id`,
`context.idempotency_key` и `identity.unique_hash` детерминированными для
конкретной transport record и не зависит от MetaTrader process-local
`MathRand()` sequence.

MQL header открывает text files с `CP_UTF8` и shared read/write flags. Он также
ремонтирует incomplete tail перед первым новым append после restart. Если
`commands.checkpoint.json` существует, но не читается или malformed, либо если
`commands.ndjson` нельзя безопасно просканировать, helper fails closed и не
пишет новую command со сброшенным `file_seq`.

### MetaTrader Discovery Utility

Поиск путей MetaTrader реализован отдельной reusable utility, а не ad-hoc
логикой внутри file bridge. Utility предназначена для file transport, quote
translators, MQL sample tooling и будущих MT4/MT5 adapters.

Responsibilities:

- находить default MetaQuotes roaming directory на Windows через OS
  known-folder API, используя `%APPDATA%` только как fallback;
- отдавать default Common Files root:
  `%APPDATA%\MetaQuotes\Terminal\Common\Files`;
- перечислять known terminal data directories в
  `%APPDATA%\MetaQuotes\Terminal\<terminal-hash>\`;
- классифицировать terminals по наличию `MQL4` или `MQL5` directories;
- возвращать per-terminal `MQL4\Files` / `MQL5\Files`, если они существуют;
- принимать явно настроенные terminal или Common Files roots без guesswork;
- держать path confinement и reserved-name checks отдельно от discovery.

C++ entry points находятся в `optionx::utils::metatrader` и доступны через
`optionx_cpp/utils.hpp` и `optionx_cpp/bridges/metatrader_file.hpp`.

В старом `mega-connector` есть useful prior art в `tools/mt/common/utils.hpp`,
но OptionX utility лучше спроектировать как набор маленьких testable helpers.

### File Message Shape

`commands.ndjson` и `events.ndjson` это UTF-8 NDJSON files. Одна complete line
это один compact JSON document. Строка без trailing `\n` считается incomplete и
пока не обрабатывается.

Каждая log record имеет monotonic `file_seq`, который назначает writer этого
лога. Это transport-local sequence для checkpoint/cleanup. Он не заменяет
JSON-RPC `id`, `context.idempotency_key`, trade IDs или domain event
`stream_id + seq`.

Example command line in `commands.ndjson`:

```json
{"file_seq":1,"jsonrpc":"2.0","id":"mql5-ea-0001","method":"trade.open","params":{"context":{"idempotency_key":"mql5:terminal-01:bar-1783476720:buy","client_created_at_ms":1783476720120,"valid_until_ms":1783476729000},"routing":{"selector":{"kind":"default"}},"trade":{"symbol":"EURUSD","order_type":"BUY","option_type":"SPRINT","amount":{"value":"1.00","currency":"USD"},"expiry":{"kind":"duration","duration_ms":60000}}}}
```

Bridge может отвечать событиями в `events.ndjson`, без отдельного
`responses` directory. Например:

```json
{"file_seq":101,"jsonrpc":"2.0","method":"trade.accepted","params":{"request_id":"mql5-ea-0001","operation_id":"op-019c...","trade_id":"784512"}}
```

Domain event notifications могут использовать обычный bridge event envelope:

```json
{"file_seq":102,"jsonrpc":"2.0","method":"balance.updated","params":{"event_id":"evt-019c...","source":"optionx://installation-019c/bridge/file","stream_id":"file-bridge-instance-019c...","seq":42,"occurred_at_ms":1783476725000,"emitted_at_ms":1783476725010,"subject":{"account_id":"1"},"revision":3,"payload":{"account_id":"1","balance":{"value":"1024.50","currency":"USD"}}}}
```

`state.json` это atomically replaced snapshot, а не append-log:

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

File transport should support the same business methods as other transports,
but MVP should prioritize:

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

Explicit event subscription commands can be added later if a file client needs
something more selective than the default `events.ndjson` stream.

### Atomicity И Cleanup

Append-log writer:

1. serializes the whole JSON document in memory;
2. writes compact JSON plus one trailing `\n`;
3. flushes the file.

Reader processes only complete lines. Final line without `\n` is incomplete and
is retried on the next poll.

Before first append after startup, writer repairs its own append-log when the
file ends with an incomplete line: truncate to the last complete LF-terminated
record, or to an empty file when no complete record exists. This prevents a
crashed half-record from being glued to the next valid record.

Single-writer rule действует на уровне логического лога и включает все
threads/tasks внутри writer'а. Repair, sequence recovery, append и owner-side
clear для одного и того же лога должны быть сериализованы. Если один и тот же
path может трогать больше одного runtime-объекта или процесса, нужен per-log
lock file или эквивалентный OS file lock; одного in-process mutex
недостаточно. Low-level append helper намеренно остаётся unlocked.

Перед append writer обязан сериализовать точные UTF-8 bytes, включая trailing
LF, и проверить:

```text
current_log_size + encoded_record_bytes <= max_command_log_bytes
```

Если места не хватает, writer может сначала очистить свой лог только когда
reader checkpoint подтверждает, что все текущие видимые records уже consumed.
Иначе он должен fail before append и не публиковать record, который выведет лог
за настроенный bridge read limit.

Reader must enforce `max_line_bytes` while streaming the file, not after loading
the full tail into memory. A complete malformed line may be skipped and reported
as transport diagnostics; checkpoint may advance past that complete malformed
line. Missing, zero, negative, fractional or non-numeric `file_seq` values make
the complete line malformed at transport layer. Incomplete final line is not
processed and does not advance checkpoint.

`state.json` and checkpoint files are written through same-directory temp file
and atomic replacement:

```text
state.json.tmp.<id> -> state.json
commands.checkpoint.json.tmp.<id> -> commands.checkpoint.json
```

Cleanup owner-driven:

- MQL side may clear `commands.ndjson`, because it is the only writer.
- Bridge may clear `events.ndjson`, because it is the only writer.
- Reader must never remove or rewrite the other side append-log.
- Writer should clear a log only after reader checkpoint confirms all records up
  to the intended `file_seq` were processed.

Writers must keep `file_seq` monotonic across cleanup cycles. After
`commands.ndjson` is cleared, the next command must use a greater `file_seq`
than any command before cleanup. This lets reader scan from the beginning and
skip old records by `last_file_seq`. Byte offsets are only an optimization.

After restart, writer should initialize its next sequence as:

```text
next_file_seq = max(last file_seq currently visible in the writer-owned log,
                    reader checkpoint last_file_seq) + 1
```

If writer stores a durable next-sequence value, that value is included in the
maximum too. Writer must not restart at `1` while reader checkpoint still points
to a later sequence.

Baseline checkpoint shape:

```json
{
  "last_file_seq": 102
}
```

Persisted byte offsets are not part of the baseline cleanup-safe checkpoint.
Reader may keep byte offset only as optimization while it can prove append-log
identity did not change since offset was recorded, for example through future
`log_generation` or equivalent file identity. Without that proof, after restart
or cleanup reader scans current file from beginning and filters by
`last_file_seq`. Reader still uses idempotency records to avoid duplicate trade
execution.

Polling helpers may limit both scanned complete non-empty lines and returned new
records per poll. Scan limit counts accepted, already-seen and malformed
complete lines so malformed input cannot make one poll accumulate unbounded
diagnostics.

Trade-affecting commands still require `context.idempotency_key`. Concrete
polling bridges should also require `context.valid_until_ms`, because file
polling can add latency and append logs may be retained longer than dedupe
window. Repeated command with the same JSON-RPC `id` or idempotency key within
configured idempotency retention window must return or re-emit the
original/current operation result, not create a second trade. After that
retention window duplicate suppression is no longer guaranteed; stale retained
commands should be rejected by `valid_until_ms`.

This draft intentionally does not define log rotation. Baseline profile is
append, checkpoint and clear.

Deferred implementation work:

- Higher-level MQL helpers for writing and compacting these logs.
- Runtime writer object or owner queue that serializes append, repair and
  owner-side clear operations per log file.
- Optional `log_generation`/file identity support if persisted byte-offset
  optimization becomes necessary.
- Callback/visitor NDJSON reader if future logs need very large scans without
  accumulating records.

### Authentication И Client Identity

File transport normally relies on local OS permissions and per-client
directories. It should not require API keys inside JSON-RPC `params`.

Recommended model:

- Directory identity is authenticated only when OS permissions isolate writer.
  Otherwise file transport is trusted-local-client profile inside one OS user.
- Bridge config maps a watched directory to configured client id and permission
  scopes.
- If a shared API key is needed, it is transport metadata for the directory, not
  a business field.
- On Windows, directory ACLs should restrict writers where possible.
- Each client should get a separate directory when several advisors, terminals
  or tools run on the same machine.
- For untrusted same-user processes, prefer named pipes with ACLs or another
  transport with credential authentication.

### Polling И Replay

File transport has no live socket. Bridge polls `commands.ndjson`; MQL side
polls `events.ndjson` and `state.json`.

Recommendations:

- For the first MetaTrader bridge implementation, prefer a small always-on event
  stream over explicit `events.subscribe` management. MQL can ignore event types
  it does not need.
- Later implementation may accept `events.subscribe` commands in
  `commands.ndjson`, but results still go through `events.ndjson` and
  `state.json`, not through per-subscription delivery directories.
- `state.json` is authoritative for current connection/account/open-trade state.
  `events.ndjson` is a change log and may be cleaned by the bridge after MQL
  checkpoint confirms it was read.
- Replay after reconnect: reader loads last checkpoint and processes complete
  records with greater `file_seq`. If append-log was cleared, reader scans from
  the beginning of current file and still filters by `last_file_seq`.
- Domain event deduplication still uses `event_id` and domain `stream_id + seq`
  when those fields are present.
## Legacy Adapter Profiles

Legacy adapter profiles не являются новой surface OptionX protocol. Это
реализации bridge, которые переводят сторонний формат в OptionX domain model.

### BotBinary

По локальному manual BotBinary имеет две automation surfaces:

- HTTP/WebRequest command с query field `request`;
- file signal directory, которую bot watches.

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

- `duration` использует seconds, minutes или hours в зависимости от time unit.
- `endtime` использует Unix time in seconds.
- Default file signal directory находится в MetaQuotes common files sandbox,
  обычно `Common\Files\Signal\`, но BotBinary может настраивать custom
  `SignalPath`.
- BotBinary можно моделировать как одно bridge family с двумя delivery
  strategies: `http` и `file`. Domain mapping одинаковый, отличается только
  transport writer.
- Внутри всё равно разумно разделить transport implementations, например
  `BotBinaryHttpTransport` и `BotBinaryFileTransport`, за одним
  `BotBinaryBridge` facade.
- Account selection, balance reading и result history зависят от observable API
  surface BotBinary. Если программа не отдаёт machine-readable balance или
  result files, bridge должен сообщать это через capabilities и
  `report.created` diagnostics, а не делать вид, что данные доступны.

Mapping draft:

| OptionX field | BotBinary field |
| --- | --- |
| `symbol` | first token, например `frxEURAUD` или `R_25` |
| `order_type = BUY` | `CALL` |
| `order_type = SELL` | `PUT` |
| `amount` | stake token |
| `expiry.kind = duration` | `duration=<value>=s/m/h` |
| `expiry.kind = absolute` | `endtime=<unix_seconds>=s` |
| `context.idempotency_key` | persisted adapter mapping на точный BotBinary request или filename |
| `identity.unique_hash` | external stable signal identity только если suffix действительно обозначает его |

Adapter не должен генерировать новый BotBinary filename при retry того же
OptionX `context.idempotency_key`. Он должен сохранять:

```text
OptionX idempotency_key -> exact BotBinary request/filename
```

BotBinary filename suffix можно копировать в `identity.unique_hash` только если
подтверждено, что это stable domain identity сигнала или сделки. Suffix,
который просто делает один файл уникальным, это transport identity, а не domain
deduplication.

### MT2Trading File Signals

MT2Trading не публикует stable public file API в наблюдаемых материалах.
Сейчас есть observed compatibility file, который пишет MT5 manual connector:

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

Второе поле это opaque encoded payload. Несколько captured samples отличаются
при изменении martingale settings, и тот же payload shape встречается под
разными target prefixes. Payload не надо считать decoded public contract, пока
encoding не понят и не покрыт тестами.

`mt2trade_tester` это observed non-broker target для MT2Trading strategy tester.
В терминах OptionX он ближе к `source.kind = "backtest"` или
`PlatformType::SIMULATOR`, чем к live broker account.

#### Non-Normative Research Notes

Следующие MT2Trading notes являются black-box research observations, а не
OptionX wire contract. Их нельзя считать normative behavior для live broker
dispatch.

Observed payload facts:

- Payload это Base16/hex text, а не Base64. Он использует только `0-9A-F`,
  имеет четную длину, и каждые два символа декодируются в один byte.
- В captured samples встречаются decoded lengths 224, 240 и 256 bytes.
- Первые bytes сейчас образуют две повторяющиеся payload families, например
  `67 27 75 95 ...` и `3B 30 31 D5 ...`. Возможно, они соответствуют разным
  signal/action templates, но текущие логи этого еще не доказывают.
- Внутри одной family и byte length изменения обычно затрагивают 16-byte
  blocks. Это похоже на block-oriented encoding или encryption layer, а не на
  plain text.
- Payload нельзя декодировать как UTF-8 text и нельзя выводить из него trade
  fields, пока controlled fixtures не покажут, какие bytes соответствуют
  direction, amount, expiration, martingale mode и symbol.
- Controlled fixture без martingale, со ставкой `1.0` и expiration `5m`,
  породил два 224-byte payloads: один из family `3B 30 31 D5 ...`, второй из
  family `67 27 75 95 ...`. Повторный тест спустя время дал те же два payloads,
  поэтому этот fixture не показывает меняющийся timestamp внутри payload. Эти
  два payload отличаются в каждом 16-byte block, значит fixture выделяет две
  стабильные direction templates, но не дает BUY/SELL mapping без точной
  фиксации порядка нажатия кнопок.
- Reported controlled comparison при изменении expiration с `5m` на `7m`
  поменял ровно один 16-byte block в обеих direction families: block 5, byte
  offset `0x50..0x5F` / decimal bytes `80..95`. Остальные 208 bytes в 224-byte
  payload остались идентичными. Это сильно похоже на independently encoded
  parameter blocks, а не на один CBC-like stream поверх всего payload.
- Reported equal-length signal-name comparison `Manual -> Test00` поменял
  block 4 (`0x40..0x4F`) плюс dynamic blocks 6 и 7, сохранив expiration block 5
  и tail configuration blocks. Поэтому block 4 сейчас лучший кандидат на
  serialized `signal_name` field, когда name length равен шести bytes. Тест с
  более коротким именем, например `Manual -> Test`, может сдвигать последующие
  serialized fields и поэтому менять много следующих blocks.
- Reported BUY fixtures с six-byte signal name сейчас дают такую рабочую карту:
  block 0 = direction/template или command type, block 2 = amount, block 4 =
  signal name, block 5 = expiration, blocks 6-7 = dynamic data, blocks 8-13 =
  martingale configuration плюс runtime chain state. Это рабочая карта, а не
  stable contract.
- Reported tail blocks 8-13 имеют минимум два byte-exact states. Последовательность
  `Manual #1 -> state A`, `Manual #2 -> state B`, `Test00 #1 -> state A`
  предполагает, что tail может содержать martingale/runtime state, привязанный к
  `signal_name`, а не только static martingale settings.
- Current high-probability implementation hypothesis: MQL-native
  `CryptEncode(CRYPT_AES128/AES256)` с последующим `ArrayToHex`-style loop. Это
  совпадает с uppercase output по два hex-символа на byte и объясняет, почему
  один MT4/MT5 code path мог быть portable без custom DLL.
- Observed behavior функционально похож на ECB: одинаковый logical plaintext
  block, судя по fixtures, дает тот же ciphertext block, а изменение одного
  logical field меняет только соответствующий 16-byte block. MQL `CryptEncode()`
  не имеет IV argument, поэтому normal CBC mode с transmitted random IV через
  этот API маловероятен. Это не доказывает точное mode name, но делает DES и
  unencrypted binary serialization намного менее вероятными.
- AES-128 и AES-256 нельзя различить по ciphertext alone, потому что оба
  используют 16-byte block size. Отличается только key length.
- Block lookup tables, если они когда-нибудь появятся, являются только
  research/tester-only artifacts. Их нельзя включать для live broker dispatch,
  пока полностью не подтверждены connector version, plaintext serialization,
  key derivation или key material, encryption mode, padding, dynamic fields,
  integrity behavior и BUY/SELL mapping.

Implementation notes:

- MT2Trading support должен быть отдельным compatibility adapter, например
  `MT2TradingFileBridge`, а не canonical OptionX file transport.
- Adapter может watch или write known file только в explicit compatibility
  mode.
- Adapter должен моделировать line prefix как observed target selector отдельно
  от opaque payload. Unknown `mt2trade_*` prefixes надо репортить и игнорировать,
  если они явно не включены.
- Target selector надо сохранять в parsed metadata даже если opaque payload
  пока не декодируется.
- Adapter не должен truncate или delete lines immediately. Если cleanup
  включён, надо держать retention window, чтобы не race с MT2Trading, который,
  похоже, тоже чистит consumed signals.
- Hash opaque payload надо хранить как metadata, например
  `metadata.opaque_payload_sha256`. Не надо заполнять `identity.unique_hash` из
  opaque payload, пока fixtures не докажут, что retry одного logical signal
  даёт ту же domain identity, а разные logical signals дают разные identities.
- Result и balance support должны отмечаться как unsupported, пока не найден
  stable MT2Trading-readable source.
- Decoding или generation opaque payloads это research task; перед включением
  для trading нужны fixture-based tests на captured samples.
- Если adapter когда-нибудь должен будет генерировать MT2Trading payloads,
  практические research paths это controlled black-box fixtures, vendor
  documentation или authorized source-level instrumentation кода, который
  integrator имеет право inspect. Без этого generation остаётся out of scope
  для live trading.
- Полезные fixture labels для следующего research pass: target prefix,
  direction, symbol, amount, expiration, martingale mode, terminal version и
  connector version.
- Полезные следующие fixtures: повторить `5m`, `7m` и еще одну duration при
  фиксированных остальных параметрах; повторить тот же BUY дважды и тот же SELL
  дважды с очисткой файла между кликами; затем менять amount и martingale mode
  по одному.
- Для проверки chain state использовать новые six-byte signal names, например
  `AAAAAA` и `BBBBBB`: отправить `AAAAAA` два раза, затем `BBBBBB` один раз.
  Если гипотеза верна, ожидаемый tail sequence будет `A`, `B`, `A`.

## Open Questions

- Точная cleanup policy для `commands.ndjson` и `events.ndjson`, когда peer
  checkpoint подтверждает обработку записей.
- Точный MQL4/MQL5 sample code для append NDJSON, polling complete lines,
  atomic replace `state.json` и event dedupe.
- Можно ли получить BotBinary balance/result из machine-readable source или
  только из UI/report screen.
- Можно ли безопасно декодировать MT2Trading `mt2trade_*` payloads настолько,
  чтобы поддержать generation, а не только observation.
