# optionx_cpp — Claude Code instructions

## Principles

1. **Think before code.** Voice assumptions, show alternatives, push back on ambiguity. Vague request -> ask what exactly to change.
2. **Simplicity first.** Minimum code that solves the task. No speculative abstractions, no future-proofing, no handling impossible scenarios. Would a senior say this is over-engineered? If yes, simplify.
3. **Surgical edits.** Only touch what the task requires. Follow existing style. Clean up your own orphans (unused imports, variables). Dead code you didn't create -> mention, don't delete.
4. **Goal-driven.** State declarative goals with verifiable success criteria. For fixes: write a test that reproduces the bug, then make it pass. Multi-step tasks: `step -> check: [what to verify]`.

## Build & Test

```bash
# Full build with deps + tests
cmake -S . -B build -DBUILD_DEPS=ON -DBUILD_TESTS=ON
cmake --build build
ctest --test-dir build

# Build a single test target
cmake --build build --target trade_manager_test
```

## Code Style

- C++17, header-only modules when possible
- Doxygen: `/// \brief`, `/// \file`, `/// \class` — never multi-paragraph docstrings
- Naming: `CamelCase` for classes/files-with-one-class, `snake_case` for methods/multi-class-files
- Member fields: `m_` prefix; getters omit `get_`
- No `../` in `#include`; cross-module deps through umbrella headers
- Comments only for non-obvious WHY, never for WHAT

## Security

No real tokens, cookies, proxy URIs, .env, or private endpoints in code, docs, tests, or commits.

## Reference

- Full workflow with practice steps: `agents/coding-agent-workflow.md`
- Commit conventions: `agents/commit-conventions.md`
- Project overview: `AGENTS.md`
