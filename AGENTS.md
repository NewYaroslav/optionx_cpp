# AGENTS.md

## Overview

**optionx_cpp** is a modular C++ library for connecting trading systems and
broker APIs.  It provides building blocks for automated trading strategies and
bridges between MetaTrader, binary options platforms and other services.  The
library is organised as a set of header-only modules that communicate through an
internal publish–subscribe bus and share utility helpers for cryptography,
networking and task scheduling.

## Features

- Publish–subscribe event system (`utils/pubsub.hpp`) for decoupled modules.
- Base modules for trade execution, HTTP clients and account information
  handling (`modules/Base*`).
- Task scheduling utilities (`utils/tasks`) with support for delayed and
  periodic jobs.
- AES‑encrypted session storage built on top of mdbx
  (`storages/ServiceSessionDB.hpp`).
- Common trading data structures for accounts, symbols, tick/bars and
  trade requests/results (`data/`).
- Bridge interface for integrating external platforms (`bridges/BaseBridge.hpp`).
- Helper utilities for cryptography, fixed‑point math, strings, paths and time
  operations (`utils/`).

## Directory Layout

| Directory | Purpose |
|-----------|---------|
| `include/optionx_cpp` | Library headers organised by module. |
| `libs/` | Third‑party dependencies and CMake scripts to install them. |
| `examples/` | Sample programs illustrating usage. |
| `tests/` | GoogleTest‑based tests. |

## Installation & Build

The project uses **CMake** (≥3.18).

- **Build dependencies** (OpenSSL, curl, gtest, etc.) using the scripts in
  `libs/` or pass an existing build through `DEPS_BUILD_DIR`.
- Configure and build the library and tests:
  ```bash
  cmake -S . -B build -DBUILD_DEPS=ON -DBUILD_TESTS=ON
  cmake --build build
  ctest --test-dir build
  ```
- Examples can be enabled with `-DBUILD_EXAMPLES=ON` and built with
  `cmake --build build --target <example_name>`.

## Testing & Documentation

- Run `ctest --test-dir build` after building to execute the test suite.
- Many headers include Doxygen comments.  Documentation can be generated with
  `doxygen` if a configuration file is available.

## Code Style

- Use `///` Doxygen comments with `\file`, `\class`, `\brief` and related
  tags for public headers.
- Prefer C++17 features and keep modules header‑only when possible.
- Follow the [Conventional Commits](https://www.conventionalcommits.org/)
  style for commit messages:

  | Type | Description |
  |------|-------------|
  | `feat:` | new features |
  | `fix:` | bug fixes |
  | `docs:` | documentation changes |
  | `refactor:` | refactoring without behaviour changes |
  | `test:` | adding or modifying tests |

  Format: `type(scope): short description` where the scope is optional and the
  message is written in the imperative mood.

