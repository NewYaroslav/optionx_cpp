# Refactor Backlog

This file tracks remaining follow-up work after the 2026 refactor-audit PR
series. Keep it short and remove items once they are handled.

## Next PR Candidates

- Add a fuller CMake package/export story for consumers that do not use the
  project as a direct submodule. The current `optionx_cpp::optionx_cpp`
  interface target covers build-tree/submodule consumption.
- Run a fresh audit pass against current `main`. The original
  `tmp/refactor-audit-findings.md` is now stale because most findings have been
  closed by PRs #15-#39.

## Explicitly Deferred

- `TradeUpPlatform` remains a partial implementation. Do not refactor it as
  part of generic cleanup PRs unless the task is specifically about TradeUp.
