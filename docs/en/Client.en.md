# Client — Remote Service Proxy

> Header: `library/include/Client.hpp` · public namespace: `Dbusxx`

## Overview

`Client` encapsulates a session plus an event loop and exposes type-safe calls, signals and property access for one fixed remote target (service name, path, interface).

It has two usage modes:

- **Self-managed mode** (four-argument constructor): `Client` owns its own `Session` and event loop (including a dedicated thread pool) — ready to use out of the box;
- **External-loop mode** (two-argument constructor): reuses an externally provided `Looper`.

All `Client` methods are thread-safe: internally it checks whether it is on the loop thread and, when needed, posts to the loop thread via `post()` and synchronously waits for the result.

## Class: `Client`

```cpp
class Client {
public:
    Client() = default;
    explicit Client(SessionType aType, std::string aService,
        std::string aPath, std::string aInterface);
    explicit Client(Looper& aLooper, std::string aService,
        std::string aPath, std::string aInterface);

    ~Client() = default;
    Client(Client&&) noexcept;
    Client& operator=(Client&&) noexcept;
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    template<typename Ret=void, uint64_t TimeoutUsec=0, typename... Args>
    [[nodiscard]] Reply<Ret> callSync(std::string_view aMethod, const Args&... aArgs);

    template<typename Ret=void, uint64_t TimeoutUsec=0, typename... Args>
    [[nodiscard]] PendingReply<Ret> callAsync(std::string_view aMethod, const Args&... aArgs);

    template<typename Ret=void, uint64_t TimeoutUsec=0, typename Callback, typename... Args>
    [[nodiscard]] Status callAsync(std::string_view aMethod, Callback&& aCallback, const Args&... aArgs);

    template<typename Callback>
    [[nodiscard]] Status listenSignal(std::string_view aSignal, Callback&& aCallback);
    template<typename Cls, typename Ret, typename... Args>
    [[nodiscard]] Status listenSignal(std::string_view aSignal, Cls* aCls, Ret(Cls::*aFunc)(Args...));

    template<typename Ret>
    [[nodiscard]] Reply<Ret> getProperty(std::string_view aProp);
    template<typename T>
    [[nodiscard]] Status setProperty(std::string_view aProp, const T& aValue);
    template<typename Callback>
    [[nodiscard]] Status onPropertyChanged(std::string_view aProp, Callback&& aCallback);
};
```

## Constructors

| Constructor | Description |
| --- | --- |
| `Client()` | Constructs an empty (invalid) client |
| `Client(SessionType, service, path, interface)` | **Self-managed mode**: creates its own session and event-loop thread |
| `Client(Looper&, service, path, interface)` | **External-loop mode**: reuses an existing `Looper` (and its `Session`) |

`Client` is non-copyable (copy deleted) and movable.

## Method calls

### Synchronous call

```cpp
template<typename Ret=void, uint64_t TimeoutUsec=0, typename... Args>
[[nodiscard]] Reply<Ret> callSync(std::string_view aMethod, const Args&... aArgs);
```

Blocks until the reply arrives. `Ret` is the return type (default `void`); `TimeoutUsec` is an optional timeout in microseconds (0 = default).

### Async call (returns a handle)

```cpp
template<typename Ret=void, uint64_t TimeoutUsec=0, typename... Args>
[[nodiscard]] PendingReply<Ret> callAsync(std::string_view aMethod, const Args&... aArgs);
```

Returns a `PendingReply<Ret>` that you can `wait()` on or `setCallback()` on.

### Async call (with callback)

```cpp
template<typename Ret=void, uint64_t TimeoutUsec=0, typename Callback, typename... Args>
[[nodiscard]] Status callAsync(std::string_view aMethod, Callback&& aCallback, const Args&... aArgs);
```

The callback signature is `void(Reply<Ret>)`; it comes first among the arguments.

## Signals

```cpp
template<typename Callback>
[[nodiscard]] Status listenSignal(std::string_view aSignal, Callback&& aCallback);
template<typename Cls, typename Ret, typename... Args>
[[nodiscard]] Status listenSignal(std::string_view aSignal, Cls* aCls, Ret(Cls::*aFunc)(Args...));
```

Subscribe to a remote signal: callback overload (callback argument types are the signal argument types) or member-function overload.

## Properties

```cpp
template<typename Ret>
[[nodiscard]] Reply<Ret> getProperty(std::string_view aProp);
template<typename T>
[[nodiscard]] Status setProperty(std::string_view aProp, const T& aValue);
template<typename Callback>
[[nodiscard]] Status onPropertyChanged(std::string_view aProp, Callback&& aCallback);
```

- `getProperty`: reads a remote property via `Properties.Get`.
- `setProperty`: writes a remote property via `Properties.Set`.
- `onPropertyChanged`: listens for `PropertiesChanged` of a remote property.

## Per-API Examples

```cpp
#include <dbusxx/Client.hpp>
#include <dbusxx/Looper.hpp>

using namespace Dbusxx;

// (1) Client() — empty construction (invalid; not usable)
[[maybe_unused]] Client empty;

// (2) Client(SessionType, service, path, iface) — self-managed mode
Client c(SessionType::USER, "com.example.Calc",
         "/com/example/calc", "com.example.Calc");

// (3) callSync<Ret, TimeoutUsec>(method, args...) — synchronous call
auto r = c.callSync<int32_t>("add", 20, 22);
if (!r.isError()) {
    std::cout << r.value();               // 42
}

// (4) callAsync<Ret>(method, args...) — async, returns a handle
auto pend = c.callAsync<int32_t>("add", 1, 2);
pend.wait();
std::cout << pend.reply().value();        // 3

// (5) callAsync<Ret>(method, cb, args...) — async, with callback
Status st5 = c.callAsync<int32_t>("add", [](Reply<int32_t> rep) {
    std::cout << rep.value();
}, 1, 2);

// (6) getProperty<Ret>(prop) — read a remote property
auto ver = c.getProperty<std::string>("version");
std::cout << ver.value();

// (7) setProperty<T>(prop, value) — write a remote property
Status st7 = c.setProperty<int32_t>("counter", 10);

// (8) onPropertyChanged(prop, cb) — listen for remote property changes
Status st8 = c.onPropertyChanged("counter", [](const int32_t& v) {
    std::cout << v;
});

// (9) listenSignal(signal, cb) — subscribe to a signal (callback overload)
Status st9 = c.listenSignal("valueChanged", [](int32_t oldV, int32_t newV) {
    std::cout << oldV << " -> " << newV;
});

// (10) listenSignal(signal, cls, memfn) — subscribe to a signal (member-function overload)
struct Sink { void onChanged(int32_t a, int32_t b) {} } sink;
Status st10 = c.listenSignal("valueChanged", &sink, &Sink::onChanged);

// (11) Client(Looper&, service, path, iface) — external-loop mode
Session sess = Session::userSession();
Looper looper(sess);
Client c2(looper, "com.example.Calc", "/com/example/calc", "com.example.Calc");
looper.run();   // driven by the external loop
```

## Notes

- All public `Client` methods are thread-safe and may be called from any thread.
- Callbacks (async calls, signals, property changes) run on the event-loop thread; avoid blocking inside them.
- In self-managed mode `Client` has its own async thread pool; it is usable immediately after construction — no manual driving needed.
