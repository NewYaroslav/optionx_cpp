# Bridge Protocol v1 Draft - Overview

Status: domain-protocol draft. This document records the shared protocol
direction for new OptionX bridges. It is intentionally expected to change and
must not yet be treated as a stable wire-level v1 contract.

Russian mirror: [overview.ru.md](overview.ru.md).
The English document is canonical; the Russian translation is synchronized from
English, not the other way around.

This draft describes the business commands, events and shared objects that
HTTP, WebSocket and named-pipe bridges should expose. The recommended wire
envelope for commands and responses is JSON-RPC 2.0, because the future
`mgc-platform` integration already uses JSON-RPC surfaces and benefits from the
same request, response and error semantics.

The protocol is transport-independent. HTTP, WebSocket and named-pipe bridges
should expose the same command, response and event payloads. Transport details
such as HTTP headers, WebSocket authentication and pipe framing are outside the
business payload.

## Goals

- Accept explicit trade requests from external tools.
- Accept strategy signals that may later produce zero, one or many trades.
- Support money/risk-management delegation instead of requiring every signal to
  carry a final amount.
- Support MT4/MT5 indicator-buffer input where signal interpretation is moved
  from MQL into the bridge.
- Import historical test results when an external tester already knows the
  result.
- Query trade results, trade history, active trades, accounts, balances and
  platform status.
- Stream reports and state changes for WebSocket and named-pipe clients, while
  still supporting query-style access for HTTP clients.
- Keep the bridge as an adapter between external systems and existing OptionX
  DTOs, not a second trading platform implementation.

## Non-Goals For v1

- Building a full historical simulator.
- Storing or replaying tick/bar history inside the protocol layer.
- Encoding broker-specific authentication flows in command payloads.
- Defining a visual node/blueprint runtime. The protocol should leave room for
  that layer, but not depend on it.
