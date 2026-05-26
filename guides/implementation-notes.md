# Implementation Notes

Внутренние правила и инварианты `optionx_cpp`. Открывай этот документ перед
изменениями в lifecycle, async, HTTP/WebSocket, trade queue или storage.

## Header-Only И Include Model

- Основной код живет в `include/optionx_cpp`.
- Headers используют `#pragma once` и macro include guard.
- Aggregate headers (`optionx.hpp`, `utils.hpp`, `data.hpp`, `modules.hpp`,
  `platforms.hpp`) формируют публичную поверхность.
- Public domains use an aggregate-first include model: users include the nearest
  aggregate header first, and that header prepares shared std/third-party/domain
  prerequisites in the intended order.
- Domain aggregates such as `data/trading.hpp` own common include context for
  their leaf DTO headers. Leaf DTO headers should not recreate sibling include
  order or pull broad dependencies unless that leaf truly owns the dependency.
- При добавлении public header обновляй ближайший aggregate header; при
  внутреннем helper не расширяй public surface.

Осторожно: non-`inline` свободные функции в header могут дать ODR-проблемы,
если header включается в несколько translation units. В существующем коде есть
старые места с такими функциями; не копируй этот стиль в новый код.

## Platform Lifecycle

Опорный файл: `platforms/common/BaseTradingPlatform.hpp`.

Lifecycle:

1. Constructor concrete platform создает account data и managers.
2. `run()` добавляет single task `initialize`.
3. Periodic task `loop` вызывает `m_event_bus.process()`,
   `module->process()`, `on_loop()`.
4. `shutdown()` останавливает `TaskManager`, вызывает `module->shutdown()`,
   затем `m_event_bus.drain()`.

Инварианты:

- `shutdown()` idempotent через атомики `m_stopping`/`m_stopped`.
- После shutdown `run()` логирует warning и не стартует lifecycle заново.
- Если используется `run(false)`, caller обязан регулярно вызывать
  `process()`.
- Registered modules хранятся как raw pointers; concrete platform должна
  владеть ими как fields и гарантировать lifetime.

## Pub-Sub

Опорные файлы: `utils/pubsub/EventBus.hpp`, `EventMediator.hpp`,
`EventAwaiter.hpp`, `data/events/*.hpp`.

Правила:

- Modules общаются через typed events, не через прямые вызовы друг друга.
- `EventMediator` в destructor отменяет awaiters и отписывает listener.
- `await_once` создает self-owned awaiter и выполняет callback один раз.
- Async events должны проходить через `notify_async(std::unique_ptr<Event>)` и
  обрабатываются на `EventBus::process()`.

Осторожно:

- Не сохраняй raw pointer на event после callback.
- Predicate/callback в `await_once` исполняются на event loop thread.
- Если module может быть destroyed раньше event delivery, подписки должны
  принадлежать `EventMediator`, а не ручным raw callbacks без unsubscribe.

## Task Scheduling

Опорные файлы: `utils/tasks/Task.hpp`, `utils/tasks/TaskManager.hpp`.

`TaskManager` поддерживает single, delayed, periodic, on-date tasks и named
overloads. `run()` запускает worker thread, `process()` выполняет ready tasks в
текущем thread.

Инварианты:

- `add_*` возвращает `false`, если manager в shutdown state.
- `shutdown()` notify/join worker thread или делает финальный `process()` без
  worker.
- `force_execute()` выставляет atomic flag; callbacks должны быть готовы к
  forced execution.
- Task callback получает `std::shared_ptr<Task>` и может проверить
  `is_shutdown()`.

Не добавляй отдельный thread loop в manager, если можно вписаться в platform
`TaskManager` или `BaseModule::process()`.

## HTTP Requests

Опорный файл: `modules/BaseHttpClientModule.hpp`.

Модель:

- Derived HTTP module запускает request через `kurlyk::HttpClient`.
- Возвращенный `std::future<kurlyk::HttpResponsePtr>` регистрируется через
  `add_http_request_task(future, callback)`.
- На каждом `process()` готовые futures читаются, callback получает response.
- В destructor/shutdown requests отменяются, rate limits удаляются.

Инварианты:

- `process()` и `shutdown()` final: derived class не должен перекрывать этот
  lifecycle.
- Exceptions из future ловятся и переводятся в error response.
- Rate limit ids хранятся в `m_rate_limits`; используй enum class
  `RateLimitType` внутри конкретной платформы.

## WebSocket

Опорный файл: `platforms/TradeUpPlatform/WebSocketManager.hpp`.

Текущий WebSocket код platform-specific и не включен в публичную композицию
`TradeUpPlatform`. Перед расширением проверь, должен ли manager стать частью
facade lifecycle или остаться probe/internal component.

Правила:

- Auth/application events должны идти через `data/events`.
- Сетевой lifecycle должен иметь явный cancel/shutdown.
- Не смешивай WebSocket state с HTTP request queue без понятного owner.

## Trade Queue And State

Опорные файлы:

- `modules/BaseTradeExecutionModule.hpp`
- `modules/BaseTradeExecutionModule/TradeQueueManager.hpp`
- `modules/BaseTradeExecutionModule/TradeStateManager.hpp`

Модель:

- Public `place_trade()` принимает unique request.
- Queue manager создает/ведет transaction event/result.
- State manager отвечает за допустимые переходы и проверки.
- Trade result callbacks вызываются через queue/result flow.
- Persistent trade storage uses TradeRequest::trade_id, not unique_id.

Инварианты:

- Не создавай trade result callback вручную в обход queue manager.
- Для сохраняемых сделок резервируй trade_id до отправки брокеру.
- Не используй TradeRequest::unique_id как DB identity.
- Не строй primary ID сделки из даты или timestamp bucket.
- Сохраняй propagation TradeRequest::trade_id -> TradeResult::trade_id.
- Не меняй active/pending trades напрямую из platform manager.
- Preprocess hook должен вернуть `false` и заполнить result error, если request
  невалиден.
- On shutdown все pending/active trades должны финализироваться.

## Account Info

Опорные файлы:

- `data/account/BaseAccountInfoData.hpp`
- `modules/BaseAccountInfoHandler.hpp`
- `modules/BaseTradeExecutionModule/AccountInfoProvider.hpp`

Модель:

- `BaseTradingPlatform` хранит shared `BaseAccountInfoData`.
- `AccountInfoProvider` дает typed read API.
- `BaseAccountInfoHandler` прокидывает account info callback.
- Updates идут через account/events/managers.

Не меняй account info напрямую из application code. Для user-facing чтения
используй `BaseTradingPlatform::get_info<T>()`.

## Session Storage

Опорный файл: `storages/ServiceSessionDB.hpp`.

Модель:

- Singleton `ServiceSessionDB::get_instance()`.
- Key = `platform + ":" + email`, затем Base64.
- Value шифруется `crypto::AESCrypt` и хранится как Base64.
- Backend: `mdbxc::KeyValueTable<std::string, std::string>`.
- Методы protected mutex.

Инварианты:

- Перед production use задай key через `set_key`; default key в constructor
  удобен для разработки, но не должен считаться безопасной политикой.
- Не обходи сервис прямым доступом к mdbx table.
- `shutdown()` disconnects DB и clears AES key.
- Path управляется macros: `OPTIONX_DATA_PATH`, `OPTIONX_DB_PATH`,
  `OPTIONX_SESSION_DB_FILE`.

## Backward Compatibility

Сохраняй совместимость в:

- string values enums и JSON conversion;
- public aggregate includes;
- DTO field names в `NLOHMANN_DEFINE_TYPE_INTRUSIVE`;
- callback signatures с `std::unique_ptr<TradeRequest>` /
  `std::unique_ptr<TradeResult>`;
- event names/type identity.

Если compatibility нужно нарушить, документируй это в PR/commit и добавляй
минимальный migration note.

## Старые Или Временные Решения

- В некоторых headers есть mojibake в комментариях и старые закомментированные
  include blocks. Не копируй это в новые файлы.
- `TradeUpPlatform` выглядит частичной реализацией: не используй ее как полный
  образец trade execution.
- `ServiceSessionDB` Doxygen говорит "SQLite", но код использует
  `mdbx_containers::KeyValueTable`; ориентируйся на код, не на устаревший
  комментарий.
