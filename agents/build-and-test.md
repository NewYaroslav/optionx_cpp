# Build And Test

Краткая инструкция по сборке, зависимостям и проверкам. Код библиотеки в
основном header-only, но tests/examples линкуются с зависимостями из `libs`.

## CMake Options

Опорный файл: `CMakeLists.txt`.

| Option / cache var | Default | Назначение |
|---|---:|---|
| `BUILD_DEPS` | `OFF` | Собрать зависимости из `libs/` |
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

Текущие test files:

- `tests/intrade_bar_platform_test.cpp`
- `tests/intrade_ping_sweep.cpp`
- `tests/trade_manager_test.cpp`
- `tests/tradeup_ws_invalid_token_probe.cpp`

CMake берет `tests/*.cpp` и создает executable с именем файла без расширения.
Для нового теста достаточно добавить `.cpp` в `tests`, если он использует
существующую схему include/link.

Линкуемые libs для tests в `CMakeLists.txt`: `ws2_32`, `wsock32`, `crypt32`,
`ssl`, `crypto`, `curl`, `mdbx`, `ntdll`, `bcrypt`, `AES`, `gtest`.

Compile definition: `ASIO_STANDALONE`. Для tests также задается
`LOGIT_BASE_PATH`.

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
| `libs/` | Third-party submodules и CMake scripts |
| `libs/cmake/*.cmake` | Сборка/подключение зависимостей |
| `libs/mdbx-containers` | Storage dependency для `ServiceSessionDB` |
| `libs/libmdbx` | MDBX dependency |

Не меняй submodules или dependency scripts при обычных правках API/доков.

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

Для изменений только в `agents/*.md` и `AGENTS.md` достаточно:

1. Проверить `git diff -- AGENTS.md agents`.
2. Проверить, что ссылки из `AGENTS.md` ведут на существующие файлы.
3. Выполнить smoke-check по ключевым темам:
   - есть build/test инструкция;
   - есть platform/class API guide;
   - есть architecture/codebase orientation;
   - есть implementation notes;
   - есть coding style.

Кодовые tests для documentation-only изменений не обязательны.
