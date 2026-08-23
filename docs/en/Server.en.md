# Server — Server Wrapper

> Header: `library/include/Server.hpp` · public namespace: `Dbusxx`

## Overview

`Server<Derived>` (CRTP) bundles a `Session`, a `Looper` and reflection (`MetaObject`) into an out-of-the-box server. On `run()`, it registers the interfaces annotated on `Derived` with the `DBUSXX_*` macros, then serves them until stopped.

It is the preferred entry point for most server scenarios: just derive from `Server<Derived>`, annotate members with the macros, and call `run()`.

## Template class: `Server<Derived>`

```cpp
template<typename Derived>
class Server : public MetaObject<Derived> {
public:
    Server() = delete;
    explicit Server(SessionType aType, std::string_view aServiceName);
    explicit Server(std::string_view aServiceName);

    Server(Server&&) noexcept;
    Server& operator=(Server&&) noexcept;
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    void run();
    void stop();
    void forceStop();
    void post(std::function<void()> aTask);

    template<typename... Args>
    [[nodiscard]] Status emit(std::string_view aPath, std::string_view aIface,
        std::string_view aSignal, Args&&... aArgs);

    template<typename T>
    [[nodiscard]] Status getProperty(std::string_view aPath, std::string_view aIface,
        std::string_view aName, T& aValue);
    template<typename T>
    [[nodiscard]] Status setProperty(std::string_view aPath, std::string_view aIface,
        std::string_view aName, const T& aValue);
    template<typename T>
    [[nodiscard]] Status onPropertyChanged(std::string_view aPath, std::string_view aIface,
        std::string_view aName, std::function<void(const T&)>&& aCallback);

    [[nodiscard]] Status status() const;
    [[nodiscard]] SessionType type() const;

protected:
    Session& session();
    Looper& looper();
};
```

### Template parameters

| Parameter | Description |
| --- | --- |
| `Derived` | The derived class (CRTP self-type) whose members are annotated with `DBUSXX_*` macros for exposure |

### Constructors

| Constructor | Description |
| --- | --- |
| `Server()` | Deleted — a session type and name must be given |
| `Server(SessionType, serviceName)` | Creates a server by type (system/user/peer); establishes the connection in the **server role** (the accepting end in peer mode) |
| `Server(serviceName)` | Convenience constructor using the user session (`SessionType::USER`) |

`Server` is non-copyable (copy deleted) and movable.

## Lifecycle

| Method | Description |
| --- | --- |
| `run()` | Registers the annotated interfaces and runs the event loop (**blocking**; registration happens in `onReady`) |
| `stop()` | Graceful stop (no-op if already errored) |
| `forceStop()` | Stops the event loop unconditionally |
| `post(task)` | Posts a task to run on the server's loop thread |

## Signals & properties

All `Server` methods are **thread-safe**: internally it checks whether it is on the loop thread and posts across threads when needed.

| Method | Description |
| --- | --- |
| `emit(path, iface, signal, args...)` | Emits a signal (may be called across threads) |
| `getProperty(path, iface, name, value&)` | Reads a locally registered property |
| `setProperty(path, iface, name, value)` | Writes a local property |
| `onPropertyChanged(path, iface, name, cb)` | Registers a local property change callback |

## Queries

| Method | Description |
| --- | --- |
| `status()` | Current server status (error takes precedence) |
| `type()` | Bound session type |

## Protected accessors

| Method | Description |
| --- | --- |
| `session()` | Accesses the underlying `Session` (for direct registration) |
| `looper()` | Accesses the underlying `Looper` |

## Per-API Examples

```cpp
#include <dbusxx/Server.hpp>

using namespace Dbusxx;

class CalcServer : public Server<CalcServer> {
public:
    // (1) Server(name) — convenience constructor (user session, requests a unique name)
    CalcServer() : Server("com.example.Calc") {}

    DBUSXX_PATH("/com/example/calc")
    DBUSXX_IFACE("com.example.Calc")
    int32_t add(int32_t a, int32_t b) { return a + b; }
    DBUSXX_METHOD(add)
    DBUSXX_PROPERTY_RW(counter, int32_t, 0)
    DBUSXX_SIGNAL(valueChanged, int32_t, int32_t)
};

// (2) Server(SessionType, name) — explicit session type
class SysServer : public Server<SysServer> {
public:
    SysServer() : Server(SessionType::SYSTEM, "com.example.Sys") {}
    DBUSXX_PATH("/com/example/sys")
    DBUSXX_IFACE("com.example.Sys")
    void ping() {}
    DBUSXX_METHOD(ping)
};

int main() {
    CalcServer server;             // (1) construct (user bus, requests a unique name)

    // (3) run() — register interfaces and run the event loop (blocking)
    std::thread t([&server] { server.run(); });

    // (4) status() — current status (error takes precedence)
    Status st = server.status();
    // (5) type() — session type
    SessionType tp = server.type();

    // (6) post(task) — post a task to the server's loop thread (cross-thread safe)
    server.post([]() { std::cout << "task on loop thread"; });

    // (7) emit(path, iface, signal, args...) — emit a signal (cross-thread safe)
    Status st7 = server.emit("/com/example/calc", "com.example.Calc",
        "valueChanged", 1, 2);

    // (8) getProperty(path, iface, name, out&) — read a local property
    int32_t c = 0;
    Status st8 = server.getProperty("/com/example/calc", "com.example.Calc",
        "counter", c);

    // (9) setProperty(path, iface, name, value) — write a local property
    Status st9 = server.setProperty("/com/example/calc", "com.example.Calc",
        "counter", 42);

    // (10) onPropertyChanged<T>(path, iface, name, cb) — local property change callback
    Status st10 = server.onPropertyChanged<int32_t>(
        "/com/example/calc", "com.example.Calc", "counter",
        [](const int32_t& v) { std::cout << v; });

    // (11) stop() — graceful stop (no-op if already errored)
    server.stop();
    // (12) forceStop() — unconditional stop (uncomment when needed)
    // server.forceStop();
    t.join();

    return 0;
}
```

## Notes

- `run()` blocks the current thread; to exit, call `stop()` / `forceStop()` from another thread.
- Interface registration happens in the `onReady` callback (grouped by `DBUSXX_PATH`/`DBUSXX_IFACE`).
- `emit`, `getProperty`, `setProperty`, `onPropertyChanged` are safe to call across threads.
