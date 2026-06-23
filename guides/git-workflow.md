# Git Workflow

Этот документ фиксирует рабочий процесс для изменений в `optionx_cpp`.

## Branch Policy

Все изменения кода и документации проходят через отдельную ветку и pull
request. Не коммить напрямую в `main`, если пользователь явно не попросил
именно это для конкретной задачи.

## Перед Началом Работы

1. Проверь состояние:

   ```bash
   git status --short
   git branch --show-current
   ```

2. Если работа начинается с `main`, обнови его:

   ```bash
   git fetch origin
   git switch main
   git merge --ff-only origin/main
   ```

3. Создай ветку с подходящим prefix:

   ```bash
   git switch -c docs/api-contracts
   ```

## Branch Naming

| Prefix | Когда использовать |
|---|---|
| `feat/` | Новая функциональность |
| `fix/` | Исправление bug/regression |
| `refactor/` | Изменение структуры без поведения |
| `test/` | Только тесты |
| `docs/` | Документация |
| `build/` | CMake/dependency/build changes |
| `ci/` | CI/CD |
| `chore/` | Maintenance без production behavior changes |
| `perf/` | Производительность |

## Commit Rules

- Делай focused commits: один понятный смысл на commit.
- Используй Conventional Commits на английском.
- Формат: `type(scope): short summary`.
- Добавляй body, если commit не очевиден из одной строки.
- Перед commit проверь `git status --short` и `git diff`.
- Не добавляй unrelated generated output, local env files, credentials, logs,
  build trees.

Смотри подробности в `guides/commit-conventions.md`.

## PR Checklist

Перед push/PR:

1. Убедись, что diff относится к задаче.
2. Запусти релевантные проверки:
   - docs-only: Markdown/link smoke-check;
   - code: самые узкие test targets;
   - public include changes: include-contract test/build.
3. Зафиксируй в PR description:
   - что изменено;
   - какие проверки выполнены;
   - что не проверялось и почему.

## После Merge

После принятия PR:

```bash
git fetch origin
git switch main
git merge --ff-only origin/main
```

Локальные build directories и untracked agent/tool folders не трогай, если они
не относятся к задаче.
