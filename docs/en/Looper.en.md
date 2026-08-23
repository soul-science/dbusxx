# Looper — Event Loop

> Header: `library/include/Looper.hpp` · public namespace: `Dbusxx`

## Overview

`Looper` drives the event loop of a D-Bus `Session` (backed by sd-event). It dispatches incoming messages and runs tasks submitted via `post()` on its **owning thread**. A `Session` needs an event loop to dispatch registered methods and signals.

## Class: `Looper`

```cpp
class Looper {
public:
    Looper() = default;
    explicit Looper(Session& aSession);

    void run();
    void stop();
    void post(std::function<void()> aTask);

    template<typename Callback>
    void onReady(Callback&& aCallback);

    [[nodiscard]] bool isOwnerThread() const;
    [[nodiscard]] Status status() const;
    Session* session() const;
};
```

### Constructors

| Constructor | Description |
| --- | --- |
| `Looper()` | Constructs an empty (unbound) event loop |
| `Looper(Session&)` | Constructs an event loop driving the given session |

### Members

| Method | Description |
| --- | --- |
| `run()` | Runs the event loop (blocks the calling thread) |
| `stop()` | Gracefully requests the event loop to stop |
| `post(task)` | Submits a task to run on the loop's thread (can be called across threads) |
| `onReady(callback)` | Registers a callback run once the loop is ready (or a peer is accepted); the callback should return `void` or `Status` (a compile-time error otherwise) |
| `isOwnerThread()` | Whether the caller is the thread that owns the loop |
| `status()` | Current status of the loop |
| `session()` | Pointer to the session this loop drives (may be null) |

## Per-API Examples

```cpp
#include <dbusxx/Session.hpp>
#include <dbusxx/Looper.hpp>

using namespace Dbusxx;

Session sess = Session::userSession("com.example.Calc");
(void)sess.registerMethod("/com/example/calc", "com.example.Calc", "add",
    [](int32_t a, int32_t b) -> int32_t { return a + b; });

// (1) Looper(Session&) — construct and bind a session
Looper looper(sess);

// (2) Looper() — empty construction (unbound; not normally used directly)
[[maybe_unused]] Looper empty;

// (3) onReady(callback) — run once when the loop is ready (callback must return void or Status)
looper.onReady([]() -> Status {
    std::cout << "loop ready" << std::endl;
    return Status(StatusCode::SUCCESS);
});

// (4) run() — run the event loop (blocks the calling thread; usually on a separate thread)
std::thread t([&looper] { looper.run(); });

// (5) isOwnerThread() — whether the caller owns the loop's thread
std::cout << looper.isOwnerThread();   // usually false on the main thread

// (6) post(task) — submit a task to the loop thread (cross-thread safe)
looper.post([]() {
    std::cout << "posted task" << std::endl;
});

// (7) status() — current loop status
Status st = looper.status();
std::cout << st.message();

// (8) session() — pointer to the driven session (null if unbound)
Session* s = looper.session();

// (9) stop() — gracefully stop the loop
looper.stop();
t.join();
```

## Related

- `Looper` is a core component of `Client` (external-loop mode) and `Server` — see [Client.en.md](Client.en.md), [Server.en.md](Server.en.md).
- `onReady` is used by `Server::run()` for initialization (interface registration).

## Notes

- `run()` is a blocking call; run it on a dedicated thread or the main thread.
- `post()` is thread-safe; tasks are executed serially on the loop thread.
- Callbacks and tasks run on the loop thread; avoid long-running blocking work inside them.
