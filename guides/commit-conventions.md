# Commit Conventions

Operational rule for AI agents that create commits in this repository.

## Format

- Commit messages must be in English and use Conventional Commits.
- Use the form `type(scope): short summary`.
- Use an imperative, concise summary.
- Include a descriptive body when the change is not obvious from the summary.
- Prefer a scope that names the touched subsystem: `docs(guides)`,
  `feat(intrade-bar)`, `fix(storage)`, `test(history)`, `build(cmake)`.

Use:

```bash
git commit -m "type(scope): short summary" -m "Detailed description of changes."
```

Before committing:

- Check `git status --short`.
- Check `git diff` or staged diff.
- Include only files related to the requested change.
- Do not commit generated build output, local env files, credentials, broker
  session databases, logs, or unrelated agent/tool folders.

## Allowed types

- `feat` - new functionality.
- `fix` - bug fix.
- `refactor` - refactoring without behavior changes.
- `perf` - performance improvements.
- `test` - add or modify tests.
- `docs` - documentation changes.
- `build` - build system or dependency updates.
- `ci` - CI/CD configuration changes.
- `chore` - maintenance tasks that do not affect production code behavior.
