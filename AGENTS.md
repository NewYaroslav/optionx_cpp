# AGENTS.md

Этот файл намеренно короткий. Это индекс для AI coding agents, которые
работают в `optionx_cpp`; открывай только те документы, которые нужны для
текущей задачи.

## Read First

- [Coding agent workflow](guides/coding-agent-workflow.md) - базовый процесс для
  задач с изменением файлов.
- [Project overview](guides/project-overview.md) - назначение библиотеки,
  публичная поверхность, основные домены и поток выполнения.
- [Platform API guide](guides/platform-api-guide.md) - практический справочник
  по платформам, модулям, событиям, DTO, storage и bridge API.
- [API and header contracts](guides/api-and-header-contracts.md) - публичный
  API, include-модель, typed broker results, trade result/history contracts.
- [Codebase orientation](guides/codebase-orientation.md) - карта проекта,
  DDD-слои, зависимости, расширение и безопасные точки входа.
- [Build and test](guides/build-and-test.md) - CMake options, зависимости,
  локальные проверки, примеры и generated output.
- [Implementation notes](guides/implementation-notes.md) - lifecycle, pub-sub,
  task scheduling, HTTP/WebSocket, trade queue, session DB и ограничения.
- [Coding style](guides/coding-style.md) - naming, namespace, Doxygen,
  обработка ошибок, ownership и header-only правила.
- [Git workflow](guides/git-workflow.md) - branch policy, PR-only workflow,
  branch naming и проверки перед PR.
- [Commit conventions](guides/commit-conventions.md) - формат коммитов, если
  пользователь просит создать commit.

## Critical Defaults

- Перед правками проверь `git status --short` и не перетирай чужие изменения.
- Не коммить напрямую в `main`; создай отдельную ветку и PR, если пользователь
  явно не попросил прямой commit в `main`.
- Для поиска по репозиторию используй `rg` / `rg --files`.
- Держи библиотеку header-only, если задача явно не требует нового `.cpp`.
- Для публичных data/module/platform domains используй ближайший aggregate
  include point вместо ручного восстановления порядка зависимостей в leaf headers.
- Внутри `include/optionx_cpp` не подключай headers через `"optionx_cpp/..."`;
  используй локальные пути от `include/optionx_cpp`, например `"data/..."`.
- Проект ориентирован на C++17 и CMake `>= 3.18`.
- Переиспользуй `utils::EventBus`, `utils::EventMediator`, `utils::TaskManager`
  и `Base*Module` вместо локальных аналогов pub-sub, loop и task queue.
- Не меняй account/trade/session state напрямую, если для этого уже есть event,
  manager или provider.
- В async/HTTP/WebSocket коде сохраняй lifecycle: `run()` -> periodic
  `process()` -> `shutdown()`/drain/cancel.
- Для публичных headers сохраняй Doxygen `///` с `\file`, `\class`, `\brief`.
- Для code changes запускай самые узкие релевантные tests/examples; для
  documentation-only изменений достаточно Markdown/link smoke-check.
