# Bridge Protocol v1 Draft - File Transport И Legacy Adapters

## Область

Этот раздел описывает две связанные, но разные вещи:

- OptionX file-drop transport, то есть binding протокола для клиентов, которые
  не могут использовать HTTP, WebSocket или named pipes;
- legacy adapter profiles для сторонних программ, у которых публичная или
  наблюдаемая интеграция сделана через файлы или HTTP-like команды, но не через
  OptionX protocol.

Английская версия каноническая. Русский файл синхронизируется с английским
после изменения английского текста.

## OptionX File-Drop Transport

File-drop transport нужен в первую очередь для MT4/MT5 indicators, advisors и
marketplace packages. В правилах MetaQuotes marketplace и в пользовательских
окружениях raw sockets, local HTTP servers или WebSocket clients часто сложнее
поставлять, чем обычный file I/O. File transport позволяет MQL-стороне писать
requests в MetaQuotes common files sandbox, а bridge может писать назад
responses, events, balances и trade snapshots.

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

Точный root должен настраиваться, потому что terminals, portable installations
и VPS images могут располагать common files directory по-разному. После выбора
root протокол использует относительные subdirectories.

### File Message Shape

Каждый request file содержит ровно один UTF-8 JSON-RPC request document:

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

Responses являются обычными JSON-RPC responses, пишутся в `responses\` и
коррелируются по исходному request `id`:

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

Events являются обычными JSON-RPC notifications, пишутся в `events\`. Сюда
относятся trade updates, balance updates и reports:

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

File transport должен поддерживать те же business methods, что и остальные
transports, но MVP стоит начинать с методов, которые делают MQL-интеграцию
полезной без HTTP:

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

Такой набор делает file API не слишком бедным: через него можно слать signals,
открывать trades, узнавать завершение operation или trade, читать текущий
balance и смотреть active/history snapshots.

### Atomicity И Cleanup

Writers не должны показывать consumers недописанные файлы:

1. Писать во временное имя в той же директории, например
   `request-id.json.tmp`.
2. Flush и close file.
3. Atomic rename в ready name, например `request-id.json`.

Consumers должны claim ready file через atomic rename в processing name или
processing directory. Если файл locked, consumer должен повторить позже, а не
считать файл malformed.

Recommended filename pattern:

```text
<unix_ms>_<client_id>_<request_id>_<method>.json
```

Rules:

- File names должны быть ASCII и path-safe.
- File content это UTF-8 JSON. BOM не должен быть обязательным.
- Multiline JSON допустим для file messages, но compact JSON проще читать в
  небольших MQL clients.
- `context.idempotency_key` всё равно обязателен для trade-affecting commands.
- `context.valid_until_ms` strongly recommended, потому что file polling может
  добавлять задержку.
- Bridge должен хранить processed files настраиваемое время перед удалением или
  archive.
- Bridge не должен сразу удалять чужие или unknown files. Unknown files лучше
  игнорировать или переносить в `errors\` только если watched directory
  выделена под OptionX.
- Duplicate files обрабатываются через idempotency и domain deduplication.
- Cleanup worker может удалять старые `responses\`, `events\`, `archive\` и
  `errors\` files после окончания retention.

### Authentication И Client Identity

File transport обычно полагается на local OS permissions и per-client
directories. Он не должен требовать API keys внутри JSON-RPC `params`.

Recommended model:

- Bridge config сопоставляет watched directory с authenticated client id и
  permission scopes.
- Если нужен shared API key, он задаётся как transport metadata для этой
  директории, а не как business field.
- На Windows по возможности надо использовать directory ACLs для ограничения
  writers.
- Каждый client должен получать отдельную directory, если на одной машине
  работают несколько advisors, terminals или tools.

### Polling И Replay

File transport не имеет live socket. Clients опрашивают `responses\` и
`events\` с заданным interval.

Recommendations:

- MQL clients должны хранить последний processed event filename или event
  `stream_id + seq`.
- Bridges могут писать event files в sequence order, но clients всё равно
  должны дедуплицировать по `event_id`.
- Durable replay можно представить как запись retained event notifications в
  `events\` перед новыми live events.
- `replay.completed` тоже может быть записан как JSON-RPC control notification
  file, если client запросил replay.

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
| `expiry.kind = absolute` | `endtime=<unix_seconds>=s/m/h` |
| `unique_hash` | unique filename suffix или generated request id |

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
- `unique_hash` можно получать из полной строки или из stable hash opaque
  payload.
- Result и balance support должны отмечаться как unsupported, пока не найден
  stable MT2Trading-readable source.
- Decoding или generation opaque payloads это research task; перед включением
  для trading нужны fixture-based tests на captured samples.

## Open Questions

- Точные retention defaults для request, response, event, archive и error
  files.
- Должен ли первый production file transport быть только JSON или ещё
  поддерживать compact line-oriented format для очень маленьких MQL scripts.
- Точный MQL4/MQL5 sample code для atomic write, polling и event dedupe.
- Можно ли получить BotBinary balance/result из machine-readable source или
  только из UI/report screen.
- Можно ли безопасно декодировать MT2Trading `mt2trade_*` payloads настолько,
  чтобы поддерживать generation, а не только observation.
