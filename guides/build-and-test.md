# Build And Test

Краткая инструкция по сборке, зависимостям и проверкам. Код библиотеки в
основном header-only, но tests/examples линкуются с зависимостями из `external`.

## CMake Options

Опорный файл: `CMakeLists.txt`.

| Option / cache var | Default | Назначение |
|---|---:|---|
| `BUILD_DEPS` | `OFF` | Собрать зависимости из `external/` |
| `BUILD_EXAMPLES` | `OFF` | Включить examples, если они поддержаны CMake |
| `BUILD_TESTS` | `OFF` | Собрать tests из `tests/*.cpp` |
| `DEPS_BUILD_DIR` | empty | Путь к уже собранным зависимостям, когда `BUILD_DEPS=OFF` |
| `LOGIT_BASE_PATH` | source dir | Base path для LOGIT logs |

Если `BUILD_TESTS=ON` и `BUILD_DEPS=OFF`, `DEPS_BUILD_DIR` обязателен.

## Baseline Commands

Полная локальная сборка с зависимостями:

```bash
cmake -S . -B build -DBUILD_DEPS=ON -DBUILD_TESTS=ON
cmake --build build
ctest --test-dir build
```

С уже готовыми зависимостями:

```bash
cmake -S . -B build -DBUILD_TESTS=ON -DDEPS_BUILD_DIR=/path/to/deps-build
cmake --build build
ctest --test-dir build
```

Code::Blocks / MinGW пример указан в комментарии `CMakeLists.txt`:

```bash
cmake -G "CodeBlocks - MinGW Makefiles" -S . -B build-cb
```

## Tests

CMake берет `tests/**/*.cpp` через `GLOB_RECURSE CONFIGURE_DEPENDS` и создает
отдельный executable для каждого test source.

Target naming:

- Если basename уникален, target name равен имени файла без расширения:
  `tests/trade_result_query_test.cpp` -> `trade_result_query_test`.
- Если в разных подпапках есть одинаковый basename, для этого basename
  используется path-derived target:
  `tests/platforms/intrade_bar/api_response_test.cpp` ->
  `tests_platforms_intrade_bar_api_response_test`.
- После нормализации `MAKE_C_IDENTIFIER` CMake дополнительно проверяет
  target-name collisions и показывает оба конфликтующих source path.

Для нового теста достаточно добавить `.cpp` в `tests` или подпапку `tests/*`,
если он использует существующую схему include/link.

Полезные текущие targets:

- `intrade_bar_api_response_test` - parser/typed-result fixtures Intrade Bar.
- `intrade_bar_auth_smoke_test` - guarded online auth smoke; требует proxy.
- `intrade_bar_smoke_cli` - ручная CLI для broker workflows.
- `trade_result_query_test` - DTO/query behavior.
- `trade_record_storage_test`, `trade_record_db_test`,
  `trade_record_stats_test` - storage/statistics behavior.
- `trade_manager_test` - trade execution lifecycle.
- `tradeup_ws_invalid_token_probe` - TradeUp WebSocket probe.

Линкуемые libs для tests в `CMakeLists.txt`: `ws2_32`, `wsock32`, `crypt32`,
`ssl`, `crypto`, `curl`, `mdbx`, `ntdll`, `bcrypt`, `AES`, `gtest`.

Compile definition: `ASIO_STANDALONE`. Для tests также задается
`LOGIT_BASE_PATH`.

## Intrade Bar Smoke CLI

Живые broker workflows вынесены в `tests/intrade_bar_api`.

Документы:

- `tests/intrade_bar_api/README.md` - обзор smoke-подхода.
- `tests/intrade_bar_api/CLI.md` - команды `intrade_bar_smoke_cli`.
- `tests/intrade_bar_api/WORKFLOWS.md` - фактические HTTP sequences.

Правила:

- Любой автоматический credentialed broker request должен идти через proxy.
- Credentials/proxy хранятся только в untracked `*.local.env`.
- `OPTIONX_INTRADE_BAR_CONFIG_FILE` указывает на локальный env-файл.
- Negative-auth сценарии не автоматизировать: broker может заблокировать вход
  на несколько часов после failed login attempts.
- Guarded trade commands требуют `--confirm` или
  `OPTIONX_INTRADE_BAR_ALLOW_TRADE=1`.

Пример сборки CLI:

```bash
cmake --build build-codex --target intrade_bar_smoke_cli -- -j1
```

Пример запуска с локальным config file:

```powershell
$env:OPTIONX_INTRADE_BAR_CONFIG_FILE="tests\intrade_bar_api\intrade_bar_api.local.env"
.\build-codex\intrade_bar_smoke_cli.exe show-account
```

## Include-Contract Checks

При изменении публичных aggregate headers или include policy добавляй или
обновляй тест, который подключает intended public entry point:

```cpp
#include <optionx_cpp/data.hpp>
#include <optionx_cpp/platforms/IntradeBarPlatform.hpp>
```

Direct leaf includes допустимы для white-box tests, но они не должны заменять
проверку публичного include contract.

## Examples

Текущие examples:

- `examples/event_mediator_test.cpp`
- `examples/intrade_bar_api_example.cpp`
- `examples/task_manager_example.cpp`
- `examples/test_encryption.cpp`
- `examples/test_intrade_bar_http_client.cpp`
- `examples/test_intrade_bar_http_client_module.cpp`
- `examples/test_service_session_db.cpp`
- `examples/test_trade_manager_module.cpp`

Перед изменением user-facing API проверь ближайший example и обнови его, если
старый сценарий стал недействительным.

## Dependency Layout

| Путь | Роль |
|---|---|
| `external/` | Third-party submodules и CMake scripts |
| `external/cmake/*.cmake` | Сборка/подключение зависимостей |
| `external/mdbx-containers` | Storage dependency для `ServiceSessionDB` |
| `external/libmdbx` | MDBX dependency |

Не меняй submodules или dependency scripts при обычных правках API/доков.

## Existing Clone Migration

После переименования `libs/` в `external/` существующим клонам стоит
синхронизировать submodule paths:

```bash
git submodule sync --recursive
git submodule update --init --recursive
```

Если после обновления на диске осталась старая неотслеживаемая папка `libs/`,
сначала убедись, что в ней нет локальных файлов, затем удали только этот путь:

```bash
git clean -fd -- libs/
```

Не запускай общий `git clean -fd` без проверки: он удаляет все untracked файлы
и каталоги в рабочем дереве.

## Generated Output

Обычно generated и не редактируется вручную:

- `build/`
- `build-*`
- CMake generated files
- copied DLLs рядом с test executables
- dependency build/install output

Если build/test создали такие файлы, не включай их в commit без отдельного
запроса.

## Documentation-Only Checks

Для изменений только в `guides/*.md` и `AGENTS.md` достаточно:

1. Проверить `git diff -- AGENTS.md guides`.
2. Проверить, что ссылки из `AGENTS.md` ведут на существующие файлы.
3. Выполнить smoke-check по ключевым темам:
   - есть build/test инструкция;
   - есть platform/class API guide;
   - есть architecture/codebase orientation;
   - есть implementation notes;
   - есть coding style.

Кодовые tests для documentation-only изменений не обязательны.
