# Bridge Lifecycle Guide For Coding Agents

This guide captures the lifecycle patterns learned while hardening the
MetaTrader file bridge and Bridge Protocol v1 HTTP/WebSocket bridge. Use it
before adding or reviewing a bridge that owns external transports, callbacks,
worker threads, or idempotent command dispatch.

## Why Bridges Are Tricky

Platform code usually owns its event loop and component graph. A bridge sits on
the boundary between OptionX, third-party transport runtimes, and user
callbacks. That boundary creates failure modes that are easy to miss:

- HTTP/WebSocket libraries can create internal worker threads.
- User callbacks can call back into `run()`, `shutdown()`, update hooks, or
  destruct the bridge.
- A request handler may still need to write a response after a callback asks for
  shutdown.
- WebSocket clients can disconnect while broadcast is reserving or completing a
  send.
- Two lifecycle calls can race across external threads, transport threads, and
  status callbacks.
- Detached work must not depend on the lifetime of the bridge object.

Treat every bridge as a small concurrent system, not as a thin adapter.

## Core State Machine

Use an explicit runtime phase:

```text
Stopped -> Starting -> Running -> Stopping -> Stopped
```

Required invariants:

- `Stopped` means all transport server objects are stopped and all owned
  transport threads have completed or have been safely joined.
- `Starting` and `Stopping` belong to a specific lifecycle generation.
- Only the owner of a generation can publish its final state.
- `run()` must wait while `Stopping` is in progress, unless it is called from a
  bridge callback where waiting would deadlock.
- `shutdown()` must be idempotent.
- A failed startup must roll back only the generation that failed.

Keep a monotonic `lifecycle_generation`. Pass the expected generation into
startup rollback and generation-specific shutdown paths.

## Shutdown Ownership

Only one caller should own a shutdown transition.

Recommended behavior:

- The first caller that changes `Running` or `Starting` to `Stopping` becomes
  the shutdown owner.
- Concurrent external shutdown callers wait for `Stopped`.
- Reentrant shutdown from a callback returns quickly after recording intent.
- Only the owner for `stopping_generation` can publish `Stopped`.

Avoid this pattern:

```cpp
// Wrong: any shutdown caller can publish Stopped.
state->phase = RuntimePhase::Stopped;
state->lifecycle_cv.notify_all();
```

Use generation checks in finalization:

```cpp
if (state->phase == RuntimePhase::Stopping &&
    state->stopping_generation == stopping_generation) {
    state->phase = RuntimePhase::Stopped;
    state->lifecycle_cv.notify_all();
}
```

## Callback Reentrancy

Callbacks are part of the bridge lifecycle surface. Assume callbacks can call
`shutdown()`, `run()`, update hooks, and diagnostic hooks.

Rules:

- Do not hold the bridge I/O or lifecycle mutex while invoking user callbacks.
- Track callback execution with a guard.
- The guard identity must be instance-specific. A callback in one bridge must
  not affect lifecycle calls on another bridge.
- Prefer a stable runtime identity, such as `RuntimeState*` held by
  `std::shared_ptr<RuntimeState>`, over a raw bridge object pointer.
- `run()` or `shutdown()` called from a callback must not wait for the same
  callback path to unwind.

Do not use a process-wide or type-wide callback depth counter. It creates false
reentrancy across independent bridge instances.

## Callback-Initiated Shutdown

If a callback asks for shutdown while a request handler is still executing, do
not stop the transport immediately.

Problem sequence:

```text
HTTP worker
  -> handle request
    -> user callback
      -> bridge.shutdown()
        -> server.stop()
    -> write JSON response
```

The response can be lost if `server.stop()` wins the race.

Preferred pattern:

1. `shutdown()` inside a callback records a pending shutdown for the current
   generation and closes admission for new transport callbacks.
2. New HTTP/WS handlers that arrive after admission closes must not invoke user
   callbacks. They should return a documented stopping response or close.
3. Already admitted transport handlers write their responses or enqueue their
   WebSocket replies.
4. The last admitted transport scope drains the pending shutdown after all
   active transport scopes for that runtime have unwound.
5. Stop and join happen in a reaper thread or in the external shutdown caller.

Track active transport callback scopes in shared runtime state, not just in
`thread_local`, because HTTP servers can run multiple worker threads. The drain
must check both `active_transport_callbacks == 0` and closed admission while
holding the same mutex; otherwise a new handler can enter between "saw zero"
and the actual drain.

## Starting Callbacks

`SERVER_STARTED` is often emitted from the transport thread that runs
`server->start()`. Shutdown requested from this callback is special:

- Do not stop the server from inside the `server->start()` ready callback.
- Do not detach the server thread and publish `Stopped` before that thread has
  really returned.
- Record pending shutdown while phase is `Starting`.
- Let `run()` observe the pending shutdown after the ready callback returns and
  perform generation-safe shutdown from the caller thread.

Regression tests should restart immediately on the same fixed port after
shutdown from `SERVER_STARTED`. This catches old-generation threads that were
detached instead of joined.

## Thread Joining

Never self-join. But also do not detach a live transport thread just because the
shutdown request originated from that thread.

Use two paths:

- Synchronous shutdown path: if finalization runs on the current thread, avoid
  self-join and detach only when there is no safe owner that can join.
- Async reaper path: finalization runs on a separate thread, so move all
  transport thread handles into the reaper and join them there.

`Stopped` must not be published until the reaper has stopped server objects and
joined the moved thread handles.

## Detached Work And Object Lifetime

Detached lambdas must not capture raw `this`.

Bad:

```cpp
std::thread([this] {
    finalize_shutdown();
}).detach();
```

Good:

```cpp
std::thread([state = m_state, handles = std::move(handles)]() mutable {
    finalize_shutdown(state, std::move(handles));
}).detach();
```

The finalizer should be static or otherwise independent of the bridge object.
Callbacks needed by finalization should be copied from `RuntimeState` under
lock, and callback guards should use the stable runtime identity.

## Status Notifications

`SERVER_STOPPED` is part of lifecycle finalization.

Rules:

- Do not publish `Stopped` before `SERVER_STOPPED` if callbacks from
  `SERVER_STOPPED` are expected to be no-ops for the old generation.
- Do not allow an old generation's `SERVER_STOPPED` callback to stop a new
  generation.
- If lifecycle calls from `SERVER_STOPPED` are supported, they should see a
  callback reentrancy guard and return quickly.

## Dispatch And Idempotency During Shutdown

If a command is already in dispatch when shutdown begins:

- Complete it with a durable or cached terminal result such as
  `server_stopped`.
- Do not leave it permanently `dispatching`.
- If another request retries the same idempotent operation while the first is
  still running, return a stable `operation_in_progress` response instead of
  blocking the transport thread.

Do not call user trade callbacks under lifecycle or I/O locks.

## WebSocket Backpressure

For WebSocket bridges:

- Track pending message count and pending bytes per connection.
- Reserve capacity before sending.
- Complete or release capacity in send callbacks.
- On overflow, close or drop the connection according to the documented
  contract.
- Remove connection state on close, error, and shutdown.

Broadcast must tolerate disconnect racing with send completion.

## Test Checklist

Add tests for these scenarios when implementing a bridge transport:

- `run()` while already running returns quickly.
- Startup failure leaves no running transport and allows a later successful
  `run()`.
- External `run()` waits while `shutdown()` owns `Stopping`.
- Two concurrent external `shutdown()` calls do not publish `Stopped` early.
- `shutdown()` from an HTTP request callback returns without deadlock and still
  lets the HTTP response reach the client.
- New HTTP/WS command handlers cannot enter user callbacks after callback
  shutdown has closed transport admission.
- `run()` and `shutdown()` from a request callback do not deadlock.
- `shutdown()` from `SERVER_STARTED` joins the old generation before immediate
  restart on the same fixed port.
- `shutdown()` and `run()` from `SERVER_STOPPED` callback are safe no-ops for
  the old generation.
- A delayed startup failure callback cannot stop a newer generation.
- WebSocket broadcast handles disconnect, slow clients, and queue overflow.
- Oversized, malformed, partial, and unauthorized requests return documented
  errors without crashing the server loop.

Run the lifecycle tests on both Windows and Linux. Thread scheduling differences
regularly expose transport races on only one platform.

## Review Checklist For Agents

When reviewing bridge code, ask:

- Who owns each transition in the runtime phase machine?
- Which generation is being stopped or rolled back?
- Can a callback call `shutdown()`, `run()`, or destroy the bridge?
- Are callbacks invoked without holding locks?
- Can a callback-initiated shutdown cut off the current HTTP/WebSocket response?
- Does detached work capture `this`?
- Does `Stopped` really mean transport threads have ended?
- Can an old callback affect a new generation?
- Are in-flight operations completed or failed closed during shutdown?
- Do tests prove the lifecycle behavior, or do they only prove that CI was lucky?
