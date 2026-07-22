# Bridge Taxonomy

This guide fixes the bridge layout decision used by the current bridge work.
It is intentionally about ownership boundaries, not wire details.

## Two Axes

Bridge code has two independent axes:

- **Protocol or adapter family**: the external contract being understood, such
  as OptionX Bridge Protocol v1, MetaTrader Common Files commands,
  TradingView extension payloads, BinaryBot/BotBinary command strings, or the
  legacy trading pipe protocol.
- **Transport**: how bytes move, such as HTTP, WebSocket, files, or named
  pipes.

The repository layout is owned by the first axis. Transport-specific code lives
inside the family that owns the wire contract. This keeps compatibility adapters
close to the schemas they emulate and keeps all transports for the native
OptionX protocol together.

## Current Families

| Family | Public include | Role | Transports |
|---|---|---|---|
| Native OptionX API | `optionx_cpp/bridges/protocol_v1.hpp` | JSON-RPC Bridge Protocol v1 for clients that can speak OptionX directly. | HTTP, WebSocket, named pipe. |
| MetaTrader Common Files | `optionx_cpp/bridges/metatrader_file.hpp` | MT4/MT5 file-command bridge and command writer for `Common\Files`. | Files. |
| TradingView extension | `optionx_cpp/bridges/trading_view.hpp` | Adapter for payloads emitted by `browser_extensions/tradingview-alert-extension`. | HTTP. |
| BinaryBot/BotBinary | `optionx_cpp/bridges/bot_binary.hpp` | Formatter/parser helpers for observed BinaryBot-compatible command strings. | HTTP query value, file-signal name. |
| Legacy trading pipe | `optionx_cpp/bridges/legacy_trading.hpp` | Compatibility bridge for the older named-pipe JSON trading protocol. | Named pipe. |

All families converge internally on OptionX DTOs such as `TradeSignal`,
`TradeRequest`, account snapshots and bridge callbacks. The public wire format
does not need to be the same for every family.

## Layout Rules

- Add new native OptionX transports under `bridges/protocol_v1/`.
  For example, the named-pipe transport for Bridge Protocol v1 is
  `bridges/protocol_v1/BridgeProtocolNamedPipeBridge.hpp`.
- Keep legacy or platform-specific adapters under their adapter family even
  when their transport overlaps a native transport. For example, the legacy
  named-pipe protocol is not the same contract as Bridge Protocol v1 over a
  named pipe.
- Keep transport utility code in `detail/` or a clearly named shared helper
  only after there are at least two real users.
- Prefer public umbrella headers (`bridges/<family>.hpp`) over direct leaf
  includes in examples and tests.

## Planned Cleanup

The legacy named-pipe family lives under `bridges/legacy_trading`.
`bridges/named_pipe.hpp` remains as a compatibility umbrella for existing
users that included the old transport-named entry point.

`BridgeProtocolServerBridge` currently exposes one runtime for HTTP and
WebSocket because both endpoints share lifecycle, idempotency state, request
handling and event broadcasting. Split internal HTTP/WebSocket transport code
only when it reduces complexity without creating separate public state machines.
