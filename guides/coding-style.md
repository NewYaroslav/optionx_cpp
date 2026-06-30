# Coding Style

Стиль проекта выводится из headers в `include/optionx_cpp`. Не добавляй общие
правила C++, если они не совпадают с локальным стилем.

## Files And Headers

- Header-only by default.
- Расширение: `.hpp`.
- Public headers используют `#pragma once` и macro guard:
  `_OPTIONX_<AREA>_<NAME>_HPP_INCLUDED`.
- Public aggregate headers лежат в `include/optionx_cpp/*.hpp`.
- External consumers include public headers with the installed prefix, for
  example `<optionx_cpp/data.hpp>` or
  `<optionx_cpp/platforms/IntradeBarPlatform.hpp>`.
- Internal headers under `include/optionx_cpp` include other project headers
  with local paths from `include/optionx_cpp`, for example
  `"data/trading.hpp"`, `"utils/http_utils.hpp"`, and
  `"platforms/common/ApiResult.hpp"`.
- Do not use `"optionx_cpp/..."` inside `include/optionx_cpp`.
- Prefer the nearest aggregate header for public-domain and cross-domain
  dependencies; do not rebuild aggregate include order inside leaf DTO headers.
- Do not use `../` in `#include` directives.
- Avoid broad or implementation-only includes in leaf headers. Direct leaf
  includes are acceptable for white-box/internal tests, not the main public
  include contract.
- Platform-specific files лежат в папке платформы и подключаются относительными
  includes из facade header.

Пример: `include/optionx_cpp/platforms/IntradeBarPlatform.hpp`.

## Namespace

Основные namespace:

- `optionx` - общие DTO/enums trading/account/market data.
- `optionx::events` - event classes.
- `optionx::utils` - infrastructure helpers.
- `optionx::components` - base components/managers.
- `optionx::platforms` - platform facades.
- `optionx::platforms::intrade_bar` - Intrade Bar internals.
- `optionx::platforms::tradeup` - TradeUp internals.
- `optionx::storage` - storage services.
- `optionx::bridges` - bridge contracts.

Для нового platform-specific manager используй subnamespace платформы, а не
общий `optionx::platforms`.

## Naming

| Сущность | Стиль | Пример |
|---|---|---|
| Классы/structs | PascalCase | `TradeRequest`, `BaseTradingPlatform` |
| Base classes | `Base<Name>` | `BaseComponent`, `BaseBridge` |
| Managers/components | PascalCase + `Manager`/`Component` | `AuthManager`, `HttpClientComponent` |
| Methods | snake_case | `place_trade`, `configure_auth`, `get_info` |
| Fields | `m_` + snake_case для private/protected | `m_event_bus`, `m_task_manager` |
| Public DTO fields | snake_case без `m_` | `account_type`, `expiry_time` |
| Type aliases | snake_case + `_t` где уже есть стиль | `trade_request_t`, `callback_t` |
| Enum types | PascalCase | `PlatformType`, `TradeState` |
| Enum values | UPPER_SNAKE_CASE | `INTRADE_BAR`, `WAITING_OPEN` |
| Files | PascalCase для классов, snake_case для helpers | `TradeRequest.hpp`, `http_utils.hpp` |

## Doxygen

Публичные headers обычно имеют:

```cpp
/// \file MyHeader.hpp
/// \brief Short description.
```

Публичные классы:

```cpp
/// \class MyClass
/// \brief Short class description.
```

Методы документируются через `/// \brief`, `\param`, `\return`, если они часть
public API. Для private helper comments добавляй только если логика неочевидна.

Не копируй mojibake из старых комментариев; новые комментарии должны быть
читаемыми.

## Error Handling

Локальные паттерны:

- Public operations часто возвращают `bool` для success/failure.
- Optional read возвращает `std::optional<T>`, например session value.
- Trade errors отражаются в `TradeResult`/`TradeErrorCode`.
- HTTP failures остаются в `kurlyk::HttpResponsePtr`.
- Storage ловит `mdbxc::MdbxException` и `std::exception`, логирует и
  возвращает failure value.
- String-to-enum имеет два слоя: non-throwing `to_enum(str, value)` и throwing
  `to_enum<T>(str)`.

Не бросай исключения из component `process()`/callbacks без явной обработки:
platform loop не должен падать из-за обычной сетевой или validation ошибки.

## Logging

Проект использует LOGIT macros:

- `LOGIT_TRACE0()`
- `LOGIT_TRACE(...)`
- `LOGIT_WARN(...)`
- `LOGIT_ERROR(...)`
- `LOGIT_PRINT_ERROR(...)`

Смотри примеры в `BaseTradingPlatform.hpp`, `TaskManager.hpp`,
`BaseHttpClientComponent.hpp`, `ServiceSessionDB.hpp`.

Правила:

- Логируй boundary failures: network/storage/async exception/shutdown misuse.
- Не логируй secrets: auth token, password, raw session, AES key.
- Для повторяющихся periodic paths избегай noisy logs без причины.

## Const, References, Pointers

Локальный ownership стиль:

- `std::unique_ptr<T>` - передача владения через public API/events, например
  `configure_auth`, `place_trade`, bridge callbacks.
- `std::shared_ptr<T>` - shared domain state/events, например account info,
  `TradeRequestEvent`.
- Raw pointers - non-owning registered components/listeners; lifetime должен быть
  обеспечен owner platform.
- `const T&` - входные DTO без владения, например `BarHistoryRequest`.
- Callback references возвращаются как `callback_t&`, чтобы пользователь мог
  назначить handler.

Do:

- Принимая ownership, используй `std::move`.
- Для nullable ownership возвращай/принимай smart pointer, не raw owning pointer.
- Для event payload не сохраняй raw pointer дольше callback.

Avoid:

- Не делай `shared_ptr` там, где поток владения единственный.
- Не отдавай mutable reference на internal state без существующего проекта
  pattern.
- Не регистрируй raw component pointer, если объект может умереть раньше platform.

## C++ Standard

Проект CMake объявляет C++ project, а agent instructions фиксируют C++17.
В коде уже используются:

- nested namespace syntax `namespace optionx::utils`
- `std::optional`
- `std::unique_ptr`, `std::shared_ptr`
- `std::atomic`, `std::thread`, `std::future`
- `nlohmann::json`

Не добавляй зависимости на более новый стандарт без явного запроса.

## Formatting

Локальный стиль неоднороден, но для нового кода придерживайся:

- 4 spaces indent.
- Opening brace на той же строке для классов/методов.
- Короткие inline methods допустимы в headers.
- Группируй public/protected/private явно.
- В таблицах enum conversion сохраняй существующий стиль `to_str`/`to_enum`.

## Compatibility Checklist

Перед изменением public header проверь:

- Не сломался ли aggregate include.
- Не изменились ли JSON field names.
- Не изменились ли enum string values.
- Не изменилась ли callback signature.
- Не появился ли новый required include у пользователей.
- Не добавлен ли platform-specific dependency в common/utils/data layer.
