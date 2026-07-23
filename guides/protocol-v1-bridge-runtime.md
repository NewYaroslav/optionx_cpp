# Bridge Protocol v1 Runtime Quickstart

This guide shows how to embed the native OptionX Bridge Protocol v1
HTTP/WebSocket server bridge in an application. The wire contract is specified
in `guides/bridge-protocol-v1.md`; this page is only about the C++ runtime
adapter.

## Public Headers

Use the family umbrella:

```cpp
#include <optionx_cpp/bridges/protocol_v1.hpp>
```

The runtime types live in `optionx::bridges::protocol_v1`:

- `BridgeProtocolServerConfig` for HTTP/WebSocket.
- `BridgeProtocolServerBridge` for the runtime bridge.
- `BridgeProtocolNamedPipeConfig` and `BridgeProtocolNamedPipeBridge` for the
  same protocol over a local named pipe.

## HTTP/WebSocket Runtime

Minimal application flow:

```cpp
using optionx::bridges::protocol_v1::BridgeProtocolServerBridge;
using optionx::bridges::protocol_v1::BridgeProtocolServerConfig;

BridgeProtocolServerConfig config;
config.bridge_id = 2;
config.address = "127.0.0.1";
config.http_port = 6562;
config.websocket_port = 6563;
config.secret = "local-dev-secret";

BridgeProtocolServerBridge bridge;
bridge.configure(std::make_unique<BridgeProtocolServerConfig>(config));

bridge.on_signal_id() = [] {
    static std::atomic<optionx::SignalId> next{1};
    return next.fetch_add(1);
};

bridge.on_trade_signal() = [](std::unique_ptr<optionx::TradeSignal> signal) {
    // Route to risk checks, broker/platform execution, or a test harness.
    (void)signal;
};

// Optional but recommended: publish the latest account snapshot before clients
// can connect. The third argument is the internal OptionX account_id; broker
// USER_ID remains inside the account snapshot as user_id in protocol results.
bridge.update_account_info(optionx::AccountInfoUpdate(
    current_account_snapshot,
    optionx::AccountUpdateStatus::CONNECTED,
    1));

bridge.run();
// Periodically publish account/trade updates with update_account_info() and
// update_trade_result(), then call shutdown() during application teardown.
```

`examples/protocol_v1_bridge_smoke.cpp` is the executable version of this
flow. It starts both transports, publishes a demo account snapshot, and in
`--self-test` mode sends:

- HTTP `POST /api/v1/bridge/command` with a `trade.open` JSON-RPC command.
- WebSocket `/api/v1/bridge/ws` with an `account.balance.get` JSON-RPC command.

The sample configuration is
`examples/protocol_v1_bridge_smoke.config.json`.

## Local Commands

Build the smoke target:

```powershell
cmake --build build --target protocol_v1_bridge_smoke
```

Run with ephemeral ports and self-test traffic:

```powershell
.\build\protocol_v1_bridge_smoke.exe --self-test
```

Run with the checked-in local config:

```powershell
.\build\protocol_v1_bridge_smoke.exe --config examples\protocol_v1_bridge_smoke.config.json
```

The configured local endpoints are:

- HTTP command endpoint:
  `http://127.0.0.1:6562/api/v1/bridge/command`
- HTTP health endpoint:
  `http://127.0.0.1:6562/api/v1/bridge/health`
- WebSocket endpoint:
  `ws://127.0.0.1:6563/api/v1/bridge/ws`
- WebSocket subprotocol:
  `optionx.bridge.v1`

HTTP clients authenticate with either `Authorization: Bearer <secret>` or
`X-OptionX-Secret: <secret>`. The WebSocket smoke uses the same
`X-OptionX-Secret` header during handshake.

## Transport Boundaries

HTTP and WebSocket are two transports for the same native protocol family. They
share one bridge instance, one lifecycle state machine, one idempotency cache,
and one set of application callbacks.

Compatibility adapters such as TradingView and BotBinary intentionally do not
need to expose this native JSON-RPC surface. They keep their external wire
contract and convert incoming payloads into the same OptionX DTO callbacks.
