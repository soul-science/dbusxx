# dbusxx

[中文](README.md)

A C++17 D-Bus library for Linux, built on systemd's sd-bus.

## Documentation

- **Library API reference**: **[Documentation Overview](docs/en/overview.en.md)** / [文档总览（中文）](docs/cn/overview.md)
- **Code generator**: **[dxxcpp Manual](tool/dxxcpp/docs/en/dxxcpp.en.md)** / [dxxcpp 使用手册](tool/dxxcpp/docs/cn/dxxcpp.md)

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
- **Built-in IDL toolchain**: one `.dxx` description generates the types header, the server skeleton and the client proxy (`dxxcpp`), wired into your build with a single CMake function.
- **No resource leaks**: resources are managed automatically.
- **Minimal dependencies**: depends only on the system-provided libsystemd.

## Features

- Method arguments/return values, signals and properties are validated at compile time by templates; supports basic types, `std::string`, `std::vector`, `std::array`, `std::map` / `std::unordered_map`, plus arbitrary **custom aggregate structs** (auto-generated `(...)` signatures, with nested/container recursion)
- Three connection types: system bus, session bus, and peer-to-peer (no bus daemon required)
- Synchronous `callSync` / asynchronous `callAsync` calls, with timeouts specified in microseconds via template parameters
- Servers are built with `Server<Derived>` + reflection macros (`DBUSXX_METHOD` / `DBUSXX_SIGNAL` / `DBUSXX_PROPERTY_*`), or directly with `Session::registerMethod` / `RegisterBuilder`
- Signals: `emitSignal` / `listenSignal`; callbacks can be any callable or member function
- Properties: local properties support registration, read/write and change callbacks; remote properties go through `Properties.Get/Set` and `PropertiesChanged` notifications
- `Looper` event loop (sd-event) with cross-thread `post()` and `onReady` initialization callbacks
- `Client` is a proxy for a remote service; it can own its event loop or reuse an external `Looper`
- Comes with a **`.dxx` interface description language and the `dxxcpp` generator**: one `.dxx` produces `<Package>Types.hpp`, `<Iface>Skeleton.hpp/.cpp` (server) and `<Iface>Proxy.hpp/.cpp` (client); once installed, the CMake function `dxxcpp_generate_lib()` wires it into a project
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
cmake -S . -B build_release -DCMAKE_BUILD_TYPE=Release
cmake --build build_release

# Install (prefer the Release build)
sudo cmake --install build_release
```

Installs to `/usr/local` by default:

- Headers: `/usr/local/include/dbusxx/`
- Shared library: `/usr/local/lib/libdbusxx.so` (versioned as `libdbusxx.so.1`)
- CMake package: `/usr/local/lib/cmake/dbusxx/` (available via `find_package(dbusxx)`, together with the code generator helper)

### Build options

| Option | Default | Purpose |
|---|---|---|
| `DBUSXX_BUILD_TESTS` | `ON` | build the unit tests (Google Test, target `dbusxx_unit_tests`) |
| `DBUSXX_BUILD_BENCHMARKS` | `ON` | build the benchmarks (Google Benchmark, target `dbusxx_benchmarks`) |
| `DBUSXX_CTEST_BENCHMARKS` | `OFF` | also run the benchmarks under `ctest`; by default only the unit tests run |

```bash
cmake -S . -B build_release -DCMAKE_BUILD_TYPE=Release -DDBUSXX_CTEST_BENCHMARKS=ON
cmake --build build_release -j
ctest --test-dir build_release --output-on-failure
```

Both the unit tests and the benchmarks spawn their own **private `dbus-daemon`** (forked child + overridden `DBUS_SESSION_BUS_ADDRESS`), so no working session bus is required on the machine.

### Building `dxxcpp`

`tool/dxxcpp` is a standalone project and is not part of the root build; configure it separately:

```bash
cmake -S tool/dxxcpp -B tool/dxxcpp/build -DCMAKE_BUILD_TYPE=Release
cmake --build tool/dxxcpp/build -j
cmake --install tool/dxxcpp/build --prefix /usr/local   # installs <prefix>/bin/dxxcpp
ctest --test-dir tool/dxxcpp/build --output-on-failure   # optional: the generator's own regression tests
```

The full story (`.dxx` syntax, generated files, validation rules, listener lifetimes) is in the **[dxxcpp Manual](tool/dxxcpp/docs/en/dxxcpp.en.md)**; a quick walkthrough is in "Usage" below.

## Usage

### Generating code from `.dxx` (dxxcpp)

Describe the interface in `.dxx` and let `dxxcpp` generate the types header, the server skeleton and the client proxy — far less work than annotating macros by hand once the interface grows.

```dxx
package com.example.calc;

struct Point {
    int32 x;
    int32 y;
};

interface Calculator {
    method add(int32 a, int32 b) -> int32;
    method translate(Point p, int32 dx) -> Point;

    @timeout(3000) method getConfig() -> map<string, string>;
    @sync method syncOnly(int32 val) -> int32;

    signal valueChanged(int32 oldVal, int32 newVal);

    @readonly property version -> string{"1.0.0"};
    property counter -> int32{0};
};
```

```bash
dxxcpp calc.dxx -o gen                 # write the generated files into gen/
dxxcpp --list-outputs calc.dxx         # print the output names only (for build systems; writes nothing)
```

Each interface gets a `<Interface>Skeleton.hpp/.cpp` (server: pure virtual base + `Server`) and a `<Interface>Proxy.hpp/.cpp` (client: sync/async methods + signal listeners); the types header is shared per package as `<Package>Types.hpp`.

Once `dbusxx` is installed, one CMake call wires it into your build:

```cmake
find_package(dbusxx REQUIRED)

dxxcpp_generate_lib(LIB_PREFIX hello INPUT hello.dxx SERVICE com.example.hello)
# -> hello_dxx_types (headers) / hello_dxx_server (STATIC) / hello_dxx_client (SHARED)
```

The full syntax, the generated files, listener lifetimes and the validation rules are in the **[dxxcpp Manual](tool/dxxcpp/docs/en/dxxcpp.en.md)**.

You do not have to use the generator — "Handwritten code" below is the equivalent hand-rolled route.

### Handwritten code

No `dxxcpp` needed: annotate the macros and use `Server` / `Client` / `Session` directly.

#### Server

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

#### Client

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

#### Using Session directly

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

#### Custom structs

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
| `PendingReply<Ret>` | Handle for an asynchronous call (`setCallback` / `wait` / `waitFor` / `reply`) |
| `Status` / `StatusCode` | Error codes and status |
| `MetaObject<Derived>` | Reflection metadata base class (used by the macros) |

## Supported types

Methods, signals and properties all accept these types; they are checked at compile time by `isValidArg` and serialized with auto-generated D-Bus signatures at runtime.

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
| `std::string` | `s` | UTF-8 string |
| `std::string_view` | `s` | **write direction only** (as an argument); ⚠️ **not usable as a return value / `Ret` / callback parameter**, see below |
| `const char*` / `char*` | `s` | C-style string |
| `UnixFd` | `h` | Unix file descriptor (RAII: copy = `dup`; usable as a method arg/return, signal parameter or struct member; ⚠️ **not usable as a property**, see [docs/en/UnixFd.en.md](docs/en/UnixFd.en.md)) |

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
- `std::string_view` works in the **write direction only** (method arguments, signal parameters, property values): `write` converts it to `std::string` first so the payload is NUL-terminated. In the **read direction** (`callSync`'s `Ret`, `getProperty`'s `T`, callback parameters, struct members) use `std::string` — `read` has no `string_view` branch, so the fallback writes a `const char*` into the object's storage and **reports success while `size()` is garbage** (measured: after `.read()` the `size()` came back a random large number and the content did not match what was written)
- `float` is handled as `double` in both directions
- Unsupported C++ types fail with a `static_assert` at compile time, never at runtime
- A struct must be **declared before it is used**: fields are embedded by value, so `struct A { B b; }; struct B { … };` does not compile
- **Empty structs are not supported** (`()` is not a valid D-Bus signature); a struct needs at least one field
- As a **property**, a struct must be comparable (`operator==`): the property setter uses `==` to detect a real change, and C++17 aggregates do not get `operator==` for free — write it yourself (the `dxxcpp`-generated types header adds it automatically)

> ⚠️ **Important limitation: native C arrays `T[N]` are not supported as struct members**. Use `std::array<T, N>` for fixed-size array fields and `std::vector<T>` for dynamic arrays.

## Reflection macros

| Macro | Purpose |
|---|---|
| `DBUSXX_PATH(path)` | sets the object path for the annotations below |
| `DBUSXX_IFACE(iface)` | sets the interface name for the annotations below |
| `DBUSXX_METHOD(name)` | exposes a member function as a D-Bus method |
| `DBUSXX_SIGNAL(name, Types...)` | declares a signal |
| `DBUSXX_PROPERTY_RO(name, Type, init...)` | read-only property |
| `DBUSXX_PROPERTY_RW(name, Type, init...)` | read-write property |

> The property macros take **variadic initializer arguments from the third parameter on**, so multi-value / nested-brace initializers work directly:
> `DBUSXX_PROPERTY_RW(samples, std::vector<int32_t>, {1, 2, 3})`, `DBUSXX_PROPERTY_RW(origin, Point, {1, 2})`.
> A **type** containing a comma, however, must be wrapped in `decltype(...)`:
> `DBUSXX_PROPERTY_RO(meta, decltype(std::map<std::string, std::string>{}), {})`.

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

A sample generator input is `tool/dxxcpp/tests/Sample.dxx`, which covers the whole `.dxx` syntax.

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

With `.dxx` it is two more lines (the generator has to be installed under the same prefix):

```cmake
find_package(dbusxx REQUIRED)

dxxcpp_generate_lib(
    LIB_PREFIX my_iface
    INPUT      my_iface.dxx
    SERVICE    com.example.my
)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE my_iface_dxx_server)
```

## License

[LGPL-3.0](./LICENSE)
