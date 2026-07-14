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

Точный root должен настраиваться, потому что terminals, portable installations
и VPS images могут располагать common files directory по-разному. После выбора
root протокол использует относительные subdirectories.

### C++ MVP Surface

Первый C++ implementation slice намеренно открывает только reusable building
blocks file transport:

- `bridges/metatrader_file.hpp` как umbrella include;
- `MetaTraderFileBridgeConfig` для MetaQuotes Common Files root, client
  directory identity, polling/lease/retention settings и file limits;
- helpers в `metatrader_file::detail` для path-safe IDs, filenames,
  event filenames совместимых с queue-state, JSON-RPC request/response/
  notification documents, atomic temp-to-ready publishing,
  ready-to-processing claim и bounded JSON reads;
- небольшие helpers для notification payloads `balance.updated` и
  `trade.updated`.

Этот slice ещё не задаёт long-running `BaseBridge` polling loop, MQL advisor
code или broker execution adapter. Эти части должны использовать helpers в
следующих implementation PR.

### Future MetaTrader Discovery Utility

Поиск путей MetaTrader стоит реализовать как переиспользуемую utility в
следующем PR, а не как ad-hoc логику внутри file bridge. Эту utility смогут
использовать file transport, quote translators, MQL sample tooling и будущие
MT4/MT5 adapters.

Ожидаемая ответственность:

- находить default MetaQuotes roaming directory на Windows через OS
  known-folder API, используя `%APPDATA%` только как fallback;
- отдавать default Common Files root:
  `%APPDATA%\MetaQuotes\Terminal\Common\Files`;
- перечислять известные terminal data directories в
  `%APPDATA%\MetaQuotes\Terminal\<terminal-hash>\`;
- классифицировать terminals по наличию директорий `MQL4` или `MQL5`;
- возвращать per-terminal директории `MQL4\Files` / `MQL5\Files`, если они
  существуют;
- принимать явно настроенные terminal или Common Files roots и не пытаться
  угадывать поверх них;
- держать path confinement и reserved-name checks отдельно от discovery, чтобы
  bridge одинаково валидировал настроенные и найденные roots.

В старом `mega-connector` есть полезный prior art в
`tools/mt/common/utils.hpp` (`SHGetKnownFolderPath`, terminal enumeration и
history-folder discovery), но OptionX utility лучше спроектировать вокруг
маленьких тестируемых helpers, а не копировать старый application-specific API.

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

File transport должен поддерживать те же business methods, что и остальные
transports, но MVP стоит начинать с методов, которые делают MQL-интеграцию
полезной без HTTP:

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

Такой набор делает file API не слишком бедным: через него можно слать signals,
открывать trades, узнавать завершение operation или trade, читать текущий
balance, смотреть active/history snapshots и узнавать точный список methods,
limits и event topics, которые поддерживает урезанный file-based bridge.

### Atomicity И Cleanup

Writers не должны показывать consumers недописанные файлы:

1. Писать во временное имя в той же директории, например
   `<unix_ms>_<file_uuid>.json.tmp`.
2. Flush и close file.
3. Atomic rename в `ready\` directory с финальным именем, например
   `<unix_ms>_<file_uuid>.json`.

Consumers должны claim ready file через atomic rename в processing name или
processing directory. Если файл locked, consumer должен повторить позже, а не
считать файл malformed.

Recommended filename pattern:

```text
<unix_ms>_<file_uuid>.json
```

Rules:

- File names должны быть ASCII и path-safe.
- `file_uuid` это только transport-level identifier. JSON-RPC correlation всё
  равно делается по полю `id` внутри file content.
- `id`, `method` и client identity берутся из file content и bridge config, а
  не из filename.
- Writers должны создавать final filenames с no-overwrite semantics.
- File content это UTF-8 JSON. BOM не должен быть обязательным.
- Multiline JSON допустим для file messages, но compact JSON проще читать в
  небольших MQL clients.
- `context.idempotency_key` всё равно обязателен для trade-affecting commands.
- `context.valid_until_ms` strongly recommended, потому что file polling может
  добавлять задержку.
- Claimed request files старше `processing_lease_ms` надо восстанавливать,
  перенося назад в `requests\ready\`, или обрабатывать повторно. Повторная
  обработка безопасна только если соблюдаются idempotency records: bridge
  должен вернуть исходный или текущий operation result, а не создать ещё одну
  сделку.
- Consumers responses и events должны claim files в `responses\processing\` или
  `events\processing\`, сохранять локальный checkpoint, затем удалять или
  архивировать claimed files. Stale client-owned processing files можно
  восстанавливать после lease timeout.
- Bridge должен хранить processed files настраиваемое время перед удалением или
  archive.
- Bridge не должен сразу удалять чужие или unknown files. Unknown files лучше
  игнорировать или переносить в `errors\` только если watched directory
  выделена под OptionX.
- Duplicate files обрабатываются через idempotency и domain deduplication.
- Cleanup worker может удалять старые `responses\`, `events\`, `archive\` и
  `errors\` files после окончания retention.

Event delivery files требуют отдельный transport-level порядок, потому что
порядок enumeration в filesystem не portable. Файлы, которые bridge пишет в
`events\ready\`, должны использовать:

```text
<delivery_queue_id>_<delivery_seq:020>_<file_uuid>.json
```

`delivery_queue_id` это path-safe transport identifier для одной client event
delivery queue. `delivery_seq` монотонно растёт внутри этой queue. Он охватывает
replayed domain events, `replay.completed` control notification и live domain
events. Он не заменяет domain event position `stream_id + seq`, а только
говорит file consumer, какой ready file надо claim следующим. Consumers должны
claim минимальный доступный `delivery_seq` для активного `delivery_queue_id`.

`delivery_queue_id` и `file_uuid` должны быть ASCII и не должны содержать `_`,
path separators, whitespace или shell metacharacters. Консервативный allowed
character set:

```text
[A-Za-z0-9.-]+
```

Это сохраняет однозначный parsing filename с underscore separators. Segment
`delivery_seq` содержит ровно 20 decimal digits.

Активная queue задаётся out-of-band файлом `events\queue-state.json`. Этот файл
надо обновлять атомарно: записать temporary file в той же directory и rename
поверх предыдущего state:

```json
{
  "delivery_queue_id": "dq-019c",
  "first_delivery_seq": 1,
  "previous_delivery_queue_id": "dq-old",
  "transition": "abandon_previous",
  "activated_at_ms": 1783920000000
}
```

Consumers должны читать `queue-state.json` перед polling `events\ready\`.
Consumer checkpoints keyed by `(delivery_queue_id, delivery_seq)`, а не только
по `delivery_seq`. Files, чей `delivery_queue_id` не совпадает с active queue,
нельзя обрабатывать как live deliveries для active queue.

Текущий draft поддерживает только такую transition semantics:

- `initial`: создаёт первую active queue.
- `abandon_previous`: закрывает `previous_delivery_queue_id` как больше не
  active и запускает `delivery_queue_id` с `first_delivery_seq`.

При `transition = "abandon_previous"` unclaimed ready files из
`previous_delivery_queue_id` надо игнорировать для live processing и можно
архивировать или переносить в quarantine, когда это позволяют retention rules.
Bridge не должен публиковать новые files в abandoned queue. Future
`drain_previous` transition reserved, но не является частью этого file transport
draft.

Atomic claim из `events\ready\` в `events\processing\` является delivery
acceptance boundary. File, который был успешно claimed до того, как consumer
увидел `abandon_previous`, является in-flight delivery и может быть processed до
completion под своим исходным checkpoint `(delivery_queue_id, delivery_seq)`.
`abandon_previous` не отзывает уже claimed files; он только запрещает новые
claims из abandoned queue после того, как новый `queue-state.json` стал видимым.
Consumers должны читать `queue-state.json` перед выбором ready file. Если позже
они видят, что claimed file относится к abandoned queue, они могут завершить его
как уже accepted in-flight delivery или перенести в archive/quarantine согласно
local retention policy, но не должны записывать его как часть новой active queue.

Rules для `delivery_seq`:

- `delivery_seq` нельзя переиспользовать внутри одного `delivery_queue_id`.
- Для одного `delivery_queue_id` каждый `delivery_seq` имеет ровно один
  terminal record.
- Bridge должен сохранять следующий `delivery_seq` или последний выданный
  `delivery_seq` как durable queue state до того, как соответствующий file
  станет видимым.
- При старте следующее значение должно восстанавливаться из durable queue state
  и сверяться как значение больше любого видимого номера в `events\ready\`,
  `events\processing\` и retained archives, которые относятся к той же queue.
- Если durable queue state потерян и следующее значение нельзя восстановить,
  bridge не должен продолжать старую queue, переиспользуя меньшие номера. Он
  должен fail closed до тех пор, пока operator атомарно не reset
  `queue-state.json` на новый `delivery_queue_id` с
  `transition = "abandon_previous"`.
- Ready files должны публиковаться по возрастанию `delivery_seq`. Bridge не
  должен делать `N + 1` видимым раньше `N`.
- Consumers должны сохранять последний completed `delivery_seq` и не должны
  перескакивать через unexplained gap. Gap закрывается только missing delivery
  file с ровно ожидаемым `delivery_seq`, например восстановленным processing
  file или `delivery.gap` control notification. Более поздний file с большим
  `delivery_seq` не должен использоваться для объяснения более раннего gap.

Когда missing delivery нельзя восстановить, bridge должен опубликовать
`delivery.gap` control notification с номером missing sequence:

```json
{
  "jsonrpc": "2.0",
  "method": "delivery.gap",
  "params": {
    "delivery_queue_id": "dq-019c...",
    "delivery_seq": 41,
    "reason": "expired_by_retention",
    "recoverable": false
  }
}
```

Сам файл `delivery.gap` должен называться с тем же `delivery_queue_id` и
`delivery_seq`, например `dq-019c_00000000000000000041_<file_uuid>.json`.

`delivery.gap` это terminal replacement для этого delivery slot, а не вторая
delivery с тем же номером. Перед публикацией `delivery.gap(N)` bridge должен
атомарно пометить slot `N` как tombstoned или replaced в durable queue state.
Если original delivery file для `N` позже восстановлен из `processing\`, его
надо перенести в quarantine или archive и нельзя возвращать в `ready\`. Если
consumer видит два ready files с одним `delivery_queue_id` и `delivery_seq`, он
должен считать это protocol violation и не должен продвигать checkpoint для
этого slot.

### Authentication И Client Identity

File transport обычно полагается на local OS permissions и per-client
directories. Он не должен требовать API keys внутри JSON-RPC `params`.

Recommended model:

- Directory identity аутентифицирует клиента только если OS permissions
  изолируют writer. Иначе file transport это trusted-local-client profile в
  границах одного OS user.
- Bridge config сопоставляет watched directory с configured client id и
  permission scopes.
- Если нужен shared API key, он задаётся как transport metadata для этой
  директории, а не как business field.
- На Windows по возможности надо использовать directory ACLs для ограничения
  writers.
- Каждый client должен получать отдельную directory, если на одной машине
  работают несколько advisors, terminals или tools.
- Для недоверенных same-user processes лучше использовать named pipes с ACLs
  или другой transport с credential authentication.

### Polling И Replay

File transport не имеет live socket. Clients опрашивают `responses\ready\`,
`events\queue-state.json` и `events\ready\` с заданным interval.

Recommendations:

- `events.subscribe`, `events.unsubscribe` и `events.subscriptions.list`
  являются валидными file-transport requests. File subscriptions durable и
  client-scoped, потому что живой socket session отсутствует.
- File subscription idempotency scoped так:

  ```text
  configured client + method + effective_subscription_key
  ```

- `effective_subscription_key` это `client_subscription_key`, если он задан,
  иначе `context.idempotency_key`. Запрос, который передаёт только
  `context.idempotency_key`, всё равно является валидным durable subscription
  request. Если заданы оба ключа, они должны разрешаться в одну durable
  subscription: тот же normalized request возвращает существующую подписку, а
  другой normalized request для того же effective key является conflict.
  Bridge должен сохранять оба переданных ключа как aliases, чтобы последующий
  retry только с `context.idempotency_key` нашёл подписку, которая изначально
  была создана с обоими fields.
- `events.subscribe` задаёт topics, filters и `replay.mode` так же, как в общем
  event contract. Bridge пишет в `events\ready\` клиента только subscribed
  topics.
- MQL clients должны читать `events\queue-state.json`, затем claim минимальный
  доступный `delivery_seq` для active `delivery_queue_id` из `events\ready\`.
  Успешный claim является acceptance boundary для этой delivery. Более позднее
  изменение `queue-state.json` не переносит claimed file в новую active queue и
  не требует rollback со стороны consumer.
- MQL clients должны хранить event checkpoint как
  `(delivery_queue_id, delivery_seq)`. Domain `stream_id + seq` остаётся
  полезным для deduplication и event-log replay, но это не file delivery
  checkpoint.
- Bridges должны писать event delivery files по возрастанию `delivery_seq`
  внутри active `delivery_queue_id`; clients также должны дедуплицировать по
  `event_id`.
- Durable replay представляется записью retained event notifications в
  `events\ready\`, затем `replay.completed` JSON-RPC control notification, затем
  новых live event notifications. Все они используют один порядок delivery
  queue.

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

- Точные retention defaults для request, response, event, archive и error
  files.
- Должен ли первый production file transport быть только JSON или ещё
  поддерживать compact line-oriented format для очень маленьких MQL scripts.
- Точный MQL4/MQL5 sample code для atomic write, polling и event dedupe.
- Можно ли получить BotBinary balance/result из machine-readable source или
  только из UI/report screen.
- Можно ли безопасно декодировать MT2Trading `mt2trade_*` payloads настолько,
  чтобы поддерживать generation, а не только observation.
