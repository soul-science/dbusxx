# Session — D-Bus Session

> Header: `library/include/Session.hpp` · public namespace: `Dbusxx`

## Overview

`Session` is the core type of the library: it manages a single D-Bus connection, supports registering methods/signals/properties (via `RegisterBuilder`), making synchronous and asynchronous remote calls, and emitting/listening to signals.

**Important threading model**: `Session` is single-threaded. Registering methods requires an event loop for dispatch, and calls need to receive replies — therefore the **server and the client usually need two independent connections** (one to serve, one to call).

## Static factory methods

```cpp
static Session systemSession();
static Session systemSession(std::string_view aServiceName);
static Session userSession();
static Session userSession(std::string_view aServiceName);
static Session peerSession(std::string_view aServiceName, bool aIsServer = false);
static Session createSession(SessionType aType = SessionType::USER,
    std::string_view aServiceName = "", bool aIsServer = false);
```

| Method | Description |
| --- | --- |
| `systemSession()` | Connects to the system bus |
| `systemSession(name)` | Connects to the system bus and requests a service name |
| `userSession()` | Connects to the user/session bus |
| `userSession(name)` | Connects to the user/session bus and requests a service name |
| `peerSession(name, isServer)` | Establishes a peer-to-peer connection over a socket (no bus daemon) |
| `createSession(type, name, isServer)` | Generic factory creating a session by `SessionType` |

## Basic info

| Method | Description |
| --- | --- |
| `type()` | Returns the session type (`SessionType`) |
| `serviceName()` | Returns the service name requested at creation |
| `getFd()` | Returns the underlying connection's file descriptor |
| `process()` | Processes a batch of pending events; returns the sd-bus process return code |
| `wait(timeoutUsec = UINT64_MAX)` | Blocks waiting for events for up to `timeoutUsec` microseconds; `UINT64_MAX` (default) waits forever |
| `flush()` | Flushes buffered outgoing messages to the bus |

## Registering interfaces

### `RegisterBuilder` (chainable builder)

Obtain one via `registerBuilder(path, iface)` to batch-register methods/signals/properties for the same (path, interface), then publish them as a single vtable with `commit()`. The builder cannot be extended further after `commit()`.

```cpp
class RegisterBuilder {
public:
    RegisterBuilder(RegisterBuilder&&) noexcept = default;
    RegisterBuilder& operator=(RegisterBuilder&&) noexcept = default;
    RegisterBuilder(const RegisterBuilder&) = delete;
    RegisterBuilder& operator=(const RegisterBuilder&) = delete;

    template<typename Func>
    RegisterBuilder& addMethod(std::string_view aName, Func aFunc);
    template<typename Cls, typename Ret, typename... Args>
    RegisterBuilder& addMethod(std::string_view aName, Cls* aCls, Ret(Cls::*aFunc)(Args...));
    template<typename... Args>
    RegisterBuilder& addSignal(std::string_view aName);
    template<typename T>
    RegisterBuilder& addProperty(std::string_view aName, T aValue, bool writable = true);
    [[nodiscard]] Status commit();
};
```

| Method | Description |
| --- | --- |
| `addMethod(name, callable)` | Registers a method backed by any callable (lambda/function object/`std::function`) |
| `addMethod(name, cls, func)` | Registers a method backed by a member function |
| `addSignal<Args...>(name)` | Registers a signal with the given argument types |
| `addProperty<T>(name, init, writable=true)` | Registers a property; the wrapper owns its own copy; `writable=false` makes it read-only |
| `commit()` | Commits and publishes the accumulated vtable; returns `Status` |

### Convenience registration methods

```cpp
template<typename Func>
[[nodiscard]] Status registerMethod(std::string_view aPath, std::string aIface,
    std::string_view aFuncName, Func&& aFunc);
template<typename Cls, typename Ret, typename... Args>
[[nodiscard]] Status registerMethod(std::string_view aPath, std::string aIface,
    std::string_view aFuncName, Cls* aCls, Ret(Cls::*aFunc)(Args...));
template<typename... Args>
[[nodiscard]] Status registerSignal(std::string_view aPath, std::string aIface,
    std::string_view aSignalName);
template<typename T>
[[nodiscard]] Status registerObject(std::string_view aPath, std::string aIface, T* aObj);
```

- `registerMethod`: registers a method, two overloads (callable / member function).
- `registerSignal`: registers a signal with the given argument types.
- `registerObject`: registers all `DBUSXX_*`-annotated members of an object (the object must be a `MetaObject<T>` subclass).

## Remote calls

### Synchronous call

```cpp
template<typename Ret=void, uint64_t TimeoutUsec=0, typename... Args>
[[nodiscard]] Reply<Ret> callSync(std::string_view aService, std::string_view aPath,
    std::string_view aIface, std::string_view aMethod, const Args&... aArgs);
```

Blocks until the reply arrives. `Ret` is the return type (default `void`); `TimeoutUsec` is an optional timeout in microseconds (0 = default).

### Async call (returns a handle)

```cpp
template<typename Ret=void, uint64_t TimeoutUsec=0, typename... Args,
    std::enable_if_t<!CallbackLikeFirstArg<Ret, Args...>::value, int> = 0>
[[nodiscard]] PendingReply<Ret> callAsync(std::string_view aService, std::string_view aPath,
    std::string_view aIface, std::string_view aMethod, const Args&... aArgs);
```

Returns a `PendingReply<Ret>` that you can `wait()` on or `setCallback()` on. Note: **the first argument must not be a callback** (otherwise the callback overload below is selected).

### Async call (with callback)

```cpp
template<typename Ret=void, uint64_t TimeoutUsec=0, typename Callback, typename... Args,
    std::enable_if_t<std::is_invocable_r_v<void, Callback, Reply<Ret>>, int> = 0>
[[nodiscard]] Status callAsync(std::string_view aService, std::string_view aPath,
    std::string_view aIface, std::string_view aMethod, Callback&& aCallback, const Args&... aArgs);
```

The callback signature is `void(Reply<Ret>)`. Returns a `Status` indicating whether the call was dispatched.

## Signals

### Listening to signals

```cpp
template<typename Callback>
[[nodiscard]] Status listenSignal(std::string_view aSender,
    std::string_view aPath, std::string_view aIface,
    std::string_view aSignal, Callback&& aCallback);
template<typename Cls, typename Ret, typename... Args>
[[nodiscard]] Status listenSignal(std::string_view aSender,
    std::string_view aPath, std::string_view aIface,
    std::string_view aSignal, Cls* aCls, Ret(Cls::*aFunc)(Args...));
```

- Callback overload: the callback argument types are the signal argument types.
- Member-function overload: dispatches the signal to a member function of `aCls`.
- An empty `aSender` matches any sender.

### Emitting signals

```cpp
template<typename... Args>
[[nodiscard]] Status emitSignal(std::string_view aPath, std::string_view aIface,
    std::string_view aSignal, const Args&... aArgs);
```

## Properties

### Local properties

```cpp
template<typename T>
[[nodiscard]] Status getLocalProperty(std::string_view aPath, std::string_view aIface,
    std::string_view aName, T& aValue);
template<typename T>
[[nodiscard]] Status setLocalProperty(std::string_view aPath, std::string_view aIface,
    std::string_view aName, const T& aValue);
template<typename T>
[[nodiscard]] Status onLocalPropertyChanged(std::string_view aPath, std::string_view aIface,
    std::string_view aName, std::function<void(const T&)>&& aCallback);
```

- `getLocalProperty`: reads a locally registered property value.
- `setLocalProperty`: writes a local property value.
- `onLocalPropertyChanged`: registers a local property change callback (receives the new value).

### Remote properties

```cpp
template<typename Ret>
[[nodiscard]] Reply<Ret> getRemoteProperty(std::string_view aService, std::string_view aPath,
    std::string_view aIface, std::string_view aProp);
template<typename T>
[[nodiscard]] Status setRemoteProperty(std::string_view aService, std::string_view aPath,
    std::string_view aIface, std::string_view aProp, const T& aValue);
template<typename Callback>
[[nodiscard]] Status onRemotePropertyChanged(std::string_view aService, std::string_view aPath,
    std::string_view aIface, std::string_view aProp, Callback&& aCallback);
```

- `getRemoteProperty`: reads a remote property via `Properties.Get`.
- `setRemoteProperty`: writes a remote property via `Properties.Set`.
- `onRemotePropertyChanged`: listens for `PropertiesChanged` of a remote property.

## Per-API Examples

> A `Session` is single-threaded: use two independent connections for serving and calling.

```cpp
#include <dbusxx/Session.hpp>
#include <dbusxx/Looper.hpp>

using namespace Dbusxx;

// ── Static factory methods ─────────────────────────────────────
// (1) systemSession() — connect to the system bus
Session s1 = Session::systemSession();
// (2) systemSession(name) — system bus and request a service name
Session s2 = Session::systemSession("com.example.Svc");
// (3) userSession() — connect to the user/session bus
Session s3 = Session::userSession();
// (4) userSession(name) — user/session bus and request a service name
Session s4 = Session::userSession("com.example.Calc");
// (5) peerSession(name, isServer) — peer-to-peer connection (no bus daemon)
Session s5 = Session::peerSession("unix:/tmp/dbus.sock", /*isServer=*/true);
// (6) createSession(type, name, isServer) — generic factory
Session s6 = Session::createSession(SessionType::USER);

// ── Basic info ─────────────────────────────────────────────────
Session sess = Session::userSession("com.example.Calc");
// (7) type() — session type
SessionType tp = sess.type();
// (8) serviceName() — requested service name
std::string svc = sess.serviceName();
// (9) getFd() — underlying connection file descriptor
int fd = sess.getFd();
// (10) process() — process a batch of pending events (returns sd-bus code)
int prc = sess.process();
// (11) wait(timeoutUsec) — block waiting for events (microseconds)
int w = sess.wait(1000);
// (12) flush() — flush buffered outgoing messages
sess.flush();

// ── RegisterBuilder (chainable registration) ───────────────────
// (13) registerBuilder(path, iface) — create a builder
auto builder = sess.registerBuilder("/com/example/calc", "com.example.Calc");
// (14) addMethod(name, callable) — register a method with a callable
builder.addMethod("add", [](int32_t a, int32_t b) { return a + b; });
// (15) addMethod(name, cls, memfn) — register a method with a member function
struct Calc { int32_t sub(int32_t a, int32_t b) { return a - b; } } calc;
builder.addMethod("sub", &calc, &Calc::sub);
// (16) addSignal<Args...>(name) — register a signal
builder.addSignal<int32_t, std::string>("valueChanged");
// (17) addProperty<T>(name, init, writable) — register a property (writable=false → read-only)
builder.addProperty<int32_t>("counter", 0, /*writable=*/true);
// (18) commit() — commit and publish the vtable
Status st18 = builder.commit();

// ── Convenience registration ───────────────────────────────────
// (19) registerMethod(path, iface, name, callable)
Status st19 = sess.registerMethod("/com/example/calc", "com.example.Calc",
    "mul", [](int32_t a, int32_t b) { return a * b; });
// (20) registerMethod(path, iface, name, cls, memfn)
Status st20 = sess.registerMethod("/com/example/calc", "com.example.Calc",
    "sub2", &calc, &Calc::sub);
// (21) registerSignal<Args...>(path, iface, name)
Status st21 = sess.registerSignal<int32_t>("/com/example/calc",
    "com.example.Calc", "changed");
// (22) registerObject(path, iface, obj) — register a DBUSXX_*-annotated object
//     It must derive from MetaObject<MyObj>:
//     Status st22 = sess.registerObject("/com/example/calc",
//         "com.example.Calc", &obj);

// ── Remote calls ───────────────────────────────────────────────
// (23) callSync<Ret, TimeoutUsec>(service, path, iface, method, args...)
auto rep = sess.callSync<int32_t>("com.example.Calc", "/com/example/calc",
    "com.example.Calc", "add", 20, 22);
std::cout << rep.value();                       // 42
// (24) callAsync<Ret>(...) — async, returns a handle
auto pend = sess.callAsync<int32_t>("com.example.Calc", "/com/example/calc",
    "com.example.Calc", "add", 1, 2);
pend.wait();
// (25) callAsync<Ret>(..., cb, args...) — async, with callback
Status st25 = sess.callAsync<int32_t>("com.example.Calc", "/com/example/calc",
    "com.example.Calc", "add", [](Reply<int32_t> r) {
        std::cout << r.value();
    }, 3, 4);

// ── Signals ────────────────────────────────────────────────────
// (26) listenSignal(sender, path, iface, signal, callback)
Status st26 = sess.listenSignal("", "/com/example/calc", "com.example.Calc",
    "valueChanged", [](int32_t v) { std::cout << v; });
// (27) listenSignal(sender, path, iface, signal, cls, memfn)
Status st27 = sess.listenSignal("", "/com/example/calc", "com.example.Calc",
    "valueChanged", &calc, &Calc::sub);
// (28) emitSignal(path, iface, signal, args...)
Status st28 = sess.emitSignal("/com/example/calc", "com.example.Calc",
    "valueChanged", 7);

// ── Local properties ───────────────────────────────────────────
int32_t cur = 0;
// (29) getLocalProperty(path, iface, name, out&)
Status st29 = sess.getLocalProperty("/com/example/calc", "com.example.Calc",
    "counter", cur);
// (30) setLocalProperty(path, iface, name, value)
Status st30 = sess.setLocalProperty("/com/example/calc", "com.example.Calc",
    "counter", 42);
// (31) onLocalPropertyChanged<T>(path, iface, name, cb)
Status st31 = sess.onLocalPropertyChanged<int32_t>(
    "/com/example/calc", "com.example.Calc", "counter",
    [](const int32_t& v) { std::cout << v; });

// ── Remote properties ──────────────────────────────────────────
// (32) getRemoteProperty<Ret>(service, path, iface, prop)
auto rp = sess.getRemoteProperty<int32_t>("com.example.Calc",
    "/com/example/calc", "com.example.Calc", "counter");
// (33) setRemoteProperty<T>(service, path, iface, prop, value)
Status st33 = sess.setRemoteProperty<int32_t>("com.example.Calc",
    "/com/example/calc", "com.example.Calc", "counter", 9);
// (34) onRemotePropertyChanged(service, path, iface, prop, cb)
Status st34 = sess.onRemotePropertyChanged(
    "com.example.Calc", "/com/example/calc", "com.example.Calc",
    "counter", [](const int32_t& v) { std::cout << v; });
```

## Notes

- A `Session` is single-threaded: serving and calling require two independent connections.
- Async callbacks (`callAsync`, `listenSignal`, `onRemotePropertyChanged`, ...) are invoked on the event-loop thread; don't block inside callbacks.
- `registerObject` requires the object to derive from `MetaObject<T>`.
