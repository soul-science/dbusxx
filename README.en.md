# dbusxx

[中文](README.md)

A C++17 D-Bus library for Linux, built on systemd's sd-bus.

## Introduction

dbusxx is a C++17 D-Bus library for Linux that lets you communicate between processes with just a few lines of code — whether you're exposing your own service or calling someone else's interface.

With it you can:

- **Expose a service in a few lines**: inherit `Server<Derived>` and add a macro to each member function; methods, signals and properties are registered on the bus.
- **Call remote code like local code**: a single `Client` object represents the remote service; choose sync `callSync` or async `callAsync`.
- **Pass the data you actually use**: basic types, `std::string`, containers, even custom structs work directly as arguments/return values with automatic (de)serialization.
- **Send/receive signals, read/write properties**: type-safe signal callbacks and property get/set with change notifications are already wrapped.

You only write business code — D-Bus signatures, messages, handles and the event loop are all handled by the library.

Use cases: inter-process communication in desktop/embedded apps, IPC between system services, and talking to existing D-Bus services such as systemd or NetworkManager. As long as libsystemd is available (it is on most mainstream distributions), you don't need to be on systemd to use it.

Key advantages:

- **Type safety**: method arguments, return values, signals and properties are real C++ types, checked at compile time. No hand-written D-Bus signatures.
- **Pass the data you use**: basic types, `std::string`, containers and even custom structs work directly as arguments/return values with automatic (de)serialization.
- **Expressive API**: expose interfaces by inheriting `Server<Derived>` and annotating member functions with macros; a single `Client` object points at a remote service.
- **Full coverage**: system bus, session bus and peer-to-peer connections; methods, signals, properties (including remote properties), event loop and automatic reconnection.
- **No resource leaks**: resources are managed automatically.
- **Minimal dependencies**: depends only on the system-provided libsystemd.

## Features

- Method arguments/return values, signals and properties are validated at compile time by templates; supports basic types, `std::string`, `std::vector`, `std::array`, `std::map`, `std::tuple`, plus arbitrary **custom aggregate structs** (auto-generated `(...)` signatures, with nested/container recursion)
- Three connection types: system bus, session bus, and peer-to-peer (no bus daemon required)
- Synchronous `callSync` / asynchronous `callAsync` calls, with timeouts specified in microseconds via template parameters
- Servers are built with `Server<Derived>` + reflection macros (`DBUSXX_METHOD` / `DBUSXX_SIGNAL` / `DBUSXX_PROPERTY_*`), or directly with `Session::registerMethod` / `RegisterBuilder`
- Signals: `emitSignal` / `listenSignal`; callbacks can be any callable or member function
- Properties: local properties support registration, read/write and change callbacks; remote properties go through `Properties.Get/Set` and `PropertiesChanged` notifications
- `Looper` event loop (sd-event) with cross-thread `post()` and `onReady` initialization callbacks
- `Client` is a proxy for a remote service; it can own its event loop or reuse an external `Looper`
- RAII resource management, automatic reconnection on disconnect, unified `Status` / `StatusCode` error handling

## Dependencies

- CMake >= 3.15
- A C++17 compiler (GCC 9+ / Clang)
- systemd development library **libsystemd >= 249**

On Ubuntu/Debian:

```bash
sudo apt install cmake g++ pkg-config libsystemd-dev
```

## Build & Install

```bash
# Debug (with ASan/UBSan)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# Release
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release

# Install (prefer the Release build)
sudo cmake --install build-release
```

Installs to `/usr/local` by default:

- Headers: `/usr/local/include/dbusxx/`
- Shared library: `/usr/local/lib/libdbusxx.so` (versioned as `libdbusxx.so.1`)
- CMake package: `/usr/local/lib/cmake/dbusxx/` (available via `find_package(dbusxx)`)

## Usage

### Server

Inherit `Server<Derived>` and annotate the methods, signals and properties you want to expose with macros, then call `run()`:

```cpp
#include <dbusxx/Server.hpp>

using namespace Dbusxx;

class CalcServer : public Server<CalcServer> {
public:
    CalcServer() : Server("com.example.Calc") {}

    // Object path / interface for the annotations below
    DBUSXX_PATH("/com/example/calc")
    DBUSXX_IFACE("com.example.Calc")

    int32_t add(int32_t a, int32_t b) { return a + b; }
    DBUSXX_METHOD(add)

    std::string greet(const std::string& name) { return "Hello, " + name + "!"; }
    DBUSXX_METHOD(greet)

    DBUSXX_SIGNAL(valueChanged, int32_t, int32_t)

    DBUSXX_PROPERTY_RO(version, std::string, std::string("1.0.0"))
    DBUSXX_PROPERTY_RW(counter, int32_t, 0)
};

int main() {
    CalcServer server;   // session bus, requests the name "com.example.Calc"
    server.run();        // registers the interface and runs the event loop (blocking)
}
```

### Client

```cpp
#include <dbusxx/Client.hpp>
#include <dbusxx/Looper.hpp>
#include <dbusxx/Session.hpp>

#include <iostream>

using namespace Dbusxx;

int main() {
    // Self-managed mode: owns its session and event loop thread
    Client c(SessionType::USER, "com.example.Calc",
             "/com/example/calc", "com.example.Calc");

    // Synchronous call
    auto r = c.callSync<int32_t>("add", 20, 22);
    if (!r.isError()) {
        std::cout << "add(20,22) = " << r.value() << std::endl;
    }

    // Asynchronous call (callback signature is void(Reply<Ret>); callback first, arguments after)
    (void)c.callAsync<int32_t>("add", [](Reply<int32_t> rep) {
        std::cout << "async add = " << rep.value() << std::endl;
    }, 1, 2);

    // Property read/write
    auto ver = c.getProperty<std::string>("version");
    std::cout << "version = " << ver.value() << std::endl;
    (void)c.setProperty<int32_t>("counter", 10);

    // Signal listening
    (void)c.listenSignal("valueChanged", [](int32_t oldV, int32_t newV) {
        std::cout << "valueChanged: " << oldV << " -> " << newV << std::endl;
    });

    // External Looper mode: reuse an existing Session + Looper
    Session sess = Session::userSession();
    Looper looper(sess);
    Client c2(looper, "com.example.Calc", "/com/example/calc", "com.example.Calc");
    looper.run();   // event loop (blocking)
}
```

### Using Session directly

If you don't need the Server/Client layer, you can operate on a Session directly:

```cpp
#include <dbusxx/Session.hpp>
#include <dbusxx/Looper.hpp>

#include <chrono>
#include <iostream>
#include <thread>

using namespace Dbusxx;

// A Session is single-threaded: registering a method needs an event loop to
// dispatch, and calling needs to receive the reply. Both must live on two
// separate connections (one serves, one calls).
int main() {
    // Connection A: register a method + event loop (server side)
    Session server = Session::userSession("com.example.Calc");
    (void)server.registerMethod("/com/example/calc", "com.example.Calc", "add",
        [](int32_t a, int32_t b) -> int32_t { return a + b; });

    Looper looper(server);
    std::thread t([&looper] { looper.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // Connection B: call the remote method (separate connection)
    Session client = Session::userSession();
    auto r = client.callSync<int32_t>(
        "com.example.Calc", "/com/example/calc", "com.example.Calc", "add",
        20, 22);
    std::cout << r.value() << std::endl;   // 42

    looper.stop();
    t.join();
    return 0;
}
```

### Custom structs

Aggregate structs can be used directly as method arguments/return values and signal parameters; fields map automatically to a D-Bus struct, with no extra registration:

```cpp
#include <dbusxx/Server.hpp>
#include <dbusxx/Client.hpp>

#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace Dbusxx;

// Custom struct (aggregate; fields map to a D-Bus struct in declaration order)
struct Point {
    int32_t x;
    int32_t y;
};

struct Person {
    std::string              name;
    int32_t                  age;
    std::vector<std::string> tags;   // a member can itself be a container
};

class GeoServer : public Server<GeoServer> {
public:
    GeoServer() : Server("com.example.Geo") {}

    DBUSXX_PATH("/com/example/geo")
    DBUSXX_IFACE("com.example.Geo")

    Point addPoint(const Point& p, const Point& q) {
        return Point { p.x + q.x, p.y + q.y };
    }
    DBUSXX_METHOD(addPoint)

    Person echoPerson(const Person& p) { return p; }
    DBUSXX_METHOD(echoPerson)

    std::vector<Point> echoPoints(const std::vector<Point>& pts) { return pts; }
    DBUSXX_METHOD(echoPoints)
};

int main() {
    GeoServer server;
    std::thread serverThread([&server] { server.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    Client c(SessionType::USER, "com.example.Geo",
             "/com/example/geo", "com.example.Geo");

    // Struct in/out
    auto r = c.callSync<Point>("addPoint", Point { 3, 4 }, Point { 5, 6 });
    std::cout << "addPoint = (" << r.value().x << ", " << r.value().y << ")\n"; // (8, 10)

    // Mixed-field struct + container member
    auto r2 = c.callSync<Person>("echoPerson",
        Person { "alice", 30, { "a", "b" } });
    std::cout << "echoPerson = " << r2.value().name << ", " << r2.value().age << "\n";

    // Array of structs
    auto r3 = c.callSync<std::vector<Point>>("echoPoints",
        std::vector<Point> { { 1, 1 }, { 2, 2 } });
    std::cout << "echoPoints size = " << r3.value().size() << "\n";

    server.stop();
    serverThread.join();
    return 0;
}
```

## Core types

| Type | Purpose |
|---|---|
| `Session` | Connection management, registering methods/signals/properties, sync/async calls, signal send/receive, local & remote properties |
| `Client` | Proxy for a remote service; internally a `Session` + `Looper` |
| `Server<Derived>` | Server (CRTP) bundling `Session` + `Looper` + reflection registration |
| `Looper` | Event loop: `run/stop/post/onReady` |
| `Message` | A message, supporting stream-style `<<` / `>>` read/write |
| `Reply<Ret>` | Return value of a synchronous call (`value()/isError()/status()`) |
| `PendingReply<Ret>` | Handle for an asynchronous call (`setCallback` / `wait` / `reply`) |
| `Status` / `StatusCode` | Error codes and status |
| `MetaObject<Derived>` | Reflection metadata base class (used by the macros) |

## Supported types

Methods, signals and properties all accept these types; they are checked at compile time by `isValidArgs` and serialized with auto-generated D-Bus signatures at runtime.

### Basic types

| C++ type | D-Bus signature | Notes |
|---|---|---|
| `int8_t` / `uint8_t` | `y` | single byte |
| `int16_t` | `n` | 16-bit signed integer |
| `uint16_t` | `q` | 16-bit unsigned integer |
| `int32_t` | `i` | 32-bit signed integer |
| `uint32_t` | `u` | 32-bit unsigned integer |
| `int64_t` | `x` | 64-bit signed integer |
| `uint64_t` | `t` | 64-bit unsigned integer |
| `bool` | `b` | boolean |
| `double` | `d` | 64-bit float |
| `float` | `d` | adapted to `double` when serializing |
| `std::string` / `std::string_view` | `s` | UTF-8 string |
| `const char*` / `char*` | `s` | C-style string |

### Containers

| C++ type | D-Bus signature | Notes |
|---|---|---|
| `std::vector<T>` | `a<sig(T)>` | dynamic array (e.g. `std::vector<int32_t>` → `ai`) |
| `std::array<T, N>` | `a<sig(T)>` | fixed-size array |
| `std::map<K, V>` / `std::unordered_map<K, V>` | `a{<sig(K)><sig(V)>}` | dictionary (e.g. `std::map<std::string, int32_t>` → `a{si}`) |
| `std::tuple<Args...>` | element-wise | only for `Message` stream `read`/`write`; **cannot** be used as a method argument/return type |

### Custom structs (aggregates)

Any custom `struct` satisfying the following can be used directly as a method argument/return value or signal parameter, **with no registration**:

- It is an **aggregate** (no user-provided constructors, no virtual functions, no private/protected non-static data members)
- Every member is itself a supported type (basic types, containers, nested structs)
- The field count does not exceed 20

Fields map to a D-Bus struct `(…)` in declaration order, fully at compile time:

| C++ type | D-Bus signature | Notes |
|---|---|---|
| `struct Point { int32_t x; int32_t y; }` | `(ii)` | two fields |
| `struct Person { std::string name; int32_t age; bool vip; }` | `(sib)` | mixed fields |
| `std::vector<Point>` | `a(ii)` | array of structs |
| `struct Rect { Point a; Point b; }` | `((ii)(ii))` | struct of structs |

Notes:

- Containers can be nested, e.g. `std::vector<std::vector<int32_t>>` → `aai`
- `void` means no return value / no arguments (`Reply<void>` / `PendingReply<void>`)
- `std::string_view` is treated as `const char*`, and `float` as `double`, when reading/writing
- Unsupported C++ types fail with a `static_assert` at compile time, never at runtime

## Reflection macros

| Macro | Purpose |
|---|---|
| `DBUSXX_PATH(path)` | sets the object path for the annotations below |
| `DBUSXX_IFACE(iface)` | sets the interface name for the annotations below |
| `DBUSXX_METHOD(name)` | exposes a member function as a D-Bus method |
| `DBUSXX_SIGNAL(name, Types...)` | declares a signal |
| `DBUSXX_PROPERTY_RO(name, Type, init)` | read-only property |
| `DBUSXX_PROPERTY_RW(name, Type, init)` | read-write property |

## Examples

There are a few runnable examples under `example/`:

| Example | Description |
|---|---|
| `example_server` | full `Server<Derived>` usage: methods, signals, properties, sync/async, cross-thread emit |
| `example_session` | using `Session` directly |
| `example_register` | the various callable types supported by `registerMethod` / `registerSignal` |
| `example_client_internal` | `Client` in self-managed mode (with a custom-struct round-trip test, Step 8.5) |
| `example_client_external` | `Client` with an external `Looper` |
| `example_peer` | peer-to-peer connection (peer server + client) |
| `example_install` | verifies the installed artifacts can be used by an external project (`find_package(dbusxx)`) |

## Integrating into your project

```cmake
cmake_minimum_required(VERSION 3.15)
project(my_app CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(dbusxx REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE dbusxx)
```

## License

[GPL-2.0](./LICENSE)
