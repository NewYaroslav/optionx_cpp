# API And Header Contracts

Этот документ фиксирует публичный контракт `optionx_cpp`: как подключаются
заголовки, где проходит граница public API, как устроены typed broker responses
и какие DTO использовать для проверки результата и выгрузки истории сделок.

Открывай его перед изменениями в `include/optionx_cpp/*.hpp`,
`platforms/common/BaseTradingPlatform.hpp`, `data/trading/*`,
`platforms/*/RequestManager.hpp` и broker history/result flows.

## Public Include Contract

Для внешнего пользователя публичный include contract остается классическим:

```cpp
#include <optionx_cpp/optionx.hpp>
#include <optionx_cpp/data.hpp>
#include <optionx_cpp/platforms.hpp>
#include <optionx_cpp/platforms/IntradeBarPlatform.hpp>
```

Внутри самого дерева `include/optionx_cpp` используется локальный путь от
`include/optionx_cpp`, без публичного префикса `optionx_cpp/`:

```cpp
#include "data/trading.hpp"
#include "utils/response_parse_utils.hpp"
#include "storages/TradeRecordDB/TradeRecordFilterMatcher.hpp"
```

Правила:

- Не используй `../` в `#include`.
- Не используй `"optionx_cpp/..."` внутри `include/optionx_cpp`.
- Public aggregate headers (`optionx.hpp`, `data.hpp`, `platforms.hpp`,
  `storages.hpp`, `modules.hpp`, `utils.hpp`, `bridges.hpp`) задают публичные
  точки подключения.
- Domain aggregates, например `data/trading.hpp`, задают include context для
  связанных leaf headers.
- Leaf DTO headers не должны вручную восстанавливать весь порядок зависимостей
  соседнего домена. Если зависимость общая для домена, держи ее в ближайшем
  aggregate.
- Direct leaf includes допустимы в white-box/internal tests, но они не должны
  случайно становиться новым пользовательским include contract.

Текущая CMake-сборка tests/examples добавляет два include-root:
`include` и `include/optionx_cpp`. Первый нужен для внешнего стиля
`<optionx_cpp/...>`, второй - для внутренних локальных путей.

## Header-Only Ownership

`optionx_cpp` - header-only C++17 библиотека. Большая часть публичной
поверхности живет в headers и компилируется в translation units пользователя.

Правила ownership:

- Новый public header добавляй в ближайший aggregate header.
- Internal helper не расширяет public surface без причины.
- Template-visible implementation должна быть видна из header.
- Свободные функции в headers должны быть `inline`, если header может
  включаться в несколько translation units.
- Не добавляй broad dependency в leaf header, если эта зависимость нужна только
  implementation detail соседнего manager.
- Публичные headers должны сохранять Doxygen `\file` и краткий `\brief`.

## Public Platform API Contract

`platforms::BaseTradingPlatform` - user-facing facade. Внешний код работает
через facade и DTO, а не через platform managers.

Основные public entry points:

- `configure_auth(std::unique_ptr<IAuthData>)`
- `connect(callback)` / `disconnect(callback)`
- `place_trade(std::unique_ptr<TradeRequest>)`
- `fetch_trade_result(TradeResultQuery, trade_result_callback_t)`
- `fetch_trade_history(const TradeHistoryRequest&, trade_history_callback_t)`
- `fetch_trade_history(trade_history_callback_t)`
- `fetch_candle_data(...)`
- `fetch_symbol_list(...)`
- `on_trade_result()`, `on_candle_data()`, `on_tick_data()`
- `get_info<T>(AccountInfoType)`

Managers (`AuthManager`, `RequestManager`, `TradeManager`, `BalanceManager`,
etc.) являются implementation detail конкретной платформы. Новый user-facing
метод сначала должен появиться на facade/base contract, а затем делегироваться
в manager.

## Typed Broker Result Pattern

Broker HTTP adapters используют typed result wrappers, чтобы не смешивать
транспортную ошибку, parser failure и успешный payload.

Общая форма: `platforms::ApiResult<T>`.

Инварианты:

- `success` говорит о результате операции.
- `status_code` хранит HTTP status или один из sentinel values:
  `NO_HTTP_STATUS`, `NO_RESPONSE_STATUS`.
- `error_desc` хранит диагностический текст failure.
- `value` хранит typed payload только для успешной операции.
- Ошибка не должна маскироваться пустым payload, если caller должен отличать
  "пустой успешный ответ" от "запрос не удался".

Для Intrade Bar новые `RequestManager::*_result` methods оборачивают старые
request/parser flows в typed results. Они не являются поводом менять parser
literals или HTML constants без live evidence: broker API нестабилен, поэтому
сохраняй то, что уже проверено.

## Trade Result Query Contract

`fetch_trade_result` проверяет результат одной сделки. Он предназначен для
recovery-сценария: бот мог открыть сделку, сохранить промежуточное состояние,
перезапуститься и позже восстановить финальный результат.

DTO:

- `TradeResultQuery` - входной запрос.
- `TradeResult` - заполняемый результат одной сделки.

Контракт:

- Broker trade identity может быть числом, строкой или hash у разных брокеров.
  Текущий Intrade Bar использует `broker_option_id`.
- Не передавай весь `TradeRequest`, если для проверки результата достаточно
  broker id и минимального контекста результата.
- `TradeResult` сохраняет local `trade_id`, account/currency, amount и open
  timing/price, если caller их знает.
- Если платформе не хватает входных данных для корректной классификации
  результата, она должна вернуть failure/diagnostic, а не молча угадать.

## Trade History Contract

`fetch_trade_history` выгружает историю закрытых сделок аккаунта как массив
`TradeRecord`.

DTO:

- `TradeHistoryRequest` - time range, selected time field, range mode, optional
  comment.
- `TradeHistoryResult` - `success`, `status_code`, `error_desc`, `records`.
- `TradeRecord` - нормализованная запись сделки для storage/statistics.

Почему `TradeRecord`, а не `TradeResult`:

- История аккаунта является экспортом/статистикой, а не callback-результатом
  одной активной сделки.
- История должна совпадать с моделью storage и аналитики.
- `TradeRecord.comment` можно использовать, чтобы пометить происхождение
  экспортированных сделок, например `account-history-export`.

Range rules:

- `TradeHistoryRequest::all()` отключает client-side filtering и означает
  "выгрузить все доступное через broker source".
- Ranged request по умолчанию использует `TradeRecordTimeField::CLOSE_DATE`,
  потому что это ближе всего к closed-trade statistics.
- Caller может выбрать другой `TradeRecordTimeField`, если нужна другая
  временная ось.
- Записи без выбранного timestamp исключаются из ranged results.
- `TimeRangeMode::CLOSED` включает обе границы.
- `TimeRangeMode::HALF_OPEN` включает start и исключает stop.

`TradeRecord::close_date` хранит planned or known option close timestamp. Для
classic options это фиксированное `TradeRequest::expiry_time`, известное до
открытия сделки. Для sprint options close time зависит от фактического
`open_date`, поэтому обычно становится известным после открытия как
`open_date + duration`.

## Intrade Bar History Sources

Source выбирается через `platforms::intrade_bar::TradeHistorySource` в
`AuthData::trade_history_source`.

Режимы:

- `CSV` - использует `/stat_trade_export.php`; обычно дает лучшее финансовое
  покрытие и более дальнюю историю.
- `HTML` - читает authenticated main page `trade_close`, затем пагинацию через
  `/trade_load_more2.php`; ближе к UI и содержит broker row identity, но
  покрытие ограничено доступной HTML-пагинацией.
- `HTML_CSV` - требует успеха обоих источников и возвращает только записи,
  найденные в обоих. Это осознанный strict режим, а не "максимальная история".

Особенности Intrade Bar:

- HTML load-more endpoint следует текущему account type в broker session; он
  не принимает независимый `account_type` в request body.
- Перед HTML/HTML_CSV history платформа должна быть подключена к нужному
  account type.
- Intrade Bar closed-history sources provide a close timestamp; the adapter
  stores it in `TradeRecord::close_date`.

## New HTTP Broker Checklist

Когда добавляется брокер с похожей HTTP-механикой:

1. Добавь concrete platform facade на базе `BaseTradingPlatform`.
2. Раздели auth/request/trade/balance/history managers по ответственности,
   ориентируясь на Intrade Bar, но не копируя broker-specific quirks.
3. Parser literals держи в platform-specific parser файле.
4. Broker-independent raw-response helpers выноси в `utils`.
5. RequestManager должен возвращать typed results для новых workflows.
6. Историю аккаунта возвращай как `TradeHistoryResult` с `TradeRecord`.
7. Проверку одной сделки возвращай через `fetch_trade_result`.
8. Для online smoke сделай отдельную подпапку в `tests/<broker>_api`.
9. Credentials/proxy держи только в untracked `*.local.env`.
10. Negative-auth tests оставляй manual, если broker может заблокировать
    аккаунт после failed login attempts.

## Documentation And Evidence Rules

Для broker behavior отделяй факты от предположений:

- Факт: подтвержден кодом, fixture-тестом, live smoke или broker response.
- Предположение: разумная гипотеза, но без подтверждения.
- Не меняй parser constants только потому, что HTML/API "должен" быть другим.
- Если reviewer сообщает потенциальный edge case, сначала классифицируй:
  regression, real bug, low-risk hardening, или false positive.
- Документируй intentional trade-offs рядом с кодом и в guide, если они могут
  выглядеть как ошибка при следующем review.
