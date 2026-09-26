# dxxcpp Manual

[中文](../cn/dxxcpp.md)

`dxxcpp` is dbusxx's interface code generator: it reads a `.dxx` interface description and emits compilable C++ sources — a types header, a server skeleton and a client proxy.

Annotating members with `DBUSXX_METHOD` / `DBUSXX_SIGNAL` / `DBUSXX_PROPERTY_*` by hand works too, but once the interface grows, `.dxx` is less work: signatures, object path, interface name and proxy methods are all generated, so a server and its clients can no longer drift apart.

- Contents: this manual / [Command line](#1-command-line) / [The dxx language](#2-the-dxx-language) / [Generated files](#3-generated-files) / [Using the generated code](#4-using-the-generated-code) / [CMake integration](#5-cmake-integration) / [Rules and limits](#6-rules-and-limits) / [Troubleshooting](#7-troubleshooting)
- Syntax cheat sheet and full positive example: `tool/dxxcpp/tests/Sample.dxx`
- Negative cases (one per validation rule): `tool/dxxcpp/tests/VerifyNegative.cpp`

---

## 0. Quick start

```bash
# Build (standalone project, not part of the root CMake build)
cmake -S tool/dxxcpp -B tool/dxxcpp/build -DCMAKE_BUILD_TYPE=Release
cmake --build tool/dxxcpp/build -j
cmake --install tool/dxxcpp/build --prefix /usr/local   # installs <prefix>/bin/dxxcpp
```

Write `calc.dxx`:

```dxx
package com.example.calc;

struct Point {
    int32 x;
    int32 y;
};

using ConfigMap = map<string, string>;

interface Calculator {
    method add(int32 a, int32 b) -> int32;
    method translate(Point p, int32 dx) -> Point;

    @timeout(3000) method getConfig() -> ConfigMap;
    @deprecated method legacy(int32 code);
    @sync method syncOnly(int32 val) -> int32;

    signal valueChanged(int32 oldVal, int32 newVal);

    @readonly property version -> string{"1.0.0"};
    property counter -> int32{0};
};
```

Generate:

```bash
dxxcpp calc.dxx -o gen
```

You get (the `-o` directory is created automatically):

```
gen/ComExampleCalcTypes.hpp       # types: namespace / struct / using / property initializers
gen/CalculatorSkeleton.hpp/.cpp   # server: <Iface>Interface pure virtual base + <Iface>Server
gen/CalculatorProxy.hpp/.cpp      # client: <Iface>Proxy
```

Generated code always includes `<dbusxx/...>` (installed layout), and the service name is injected by the build system through `DBUSXX_SERVICE_NAME`. The two files that actually use that macro (`<Interface>Proxy.hpp` and `<Interface>Skeleton.cpp`) carry an `#error` guard, so forgetting it fails the build instead of silently connecting to the wrong bus.

The least-effort wiring is to let CMake do it: see [CMake integration](#5-cmake-integration).

---

## 1. Command line

```
dxxcpp [--dbus] <input.dxx> [-o <output-dir>]
```

| Option | Meaning |
|---|---|
| `--dbus` | Backend selection. D-Bus (dbusxx) is currently the only backend and is the default; passing it changes nothing |
| `-o, --output-dir <dir>` | Output directory, default is the current directory. `--output-dir=<dir>` and `-o<dir>` are accepted too |
| `--list-outputs` | Print the output names only (one `<role>:<name>` per line), **writing nothing and creating no directory** |
| `-h, --help` | Print usage |

**Exit codes**

| Code | Meaning |
|---|---|
| `0` | Success (including `--help` / `--list-outputs`) |
| `1` | Syntax or semantic errors in the input, or a file read/write failure |
| `2` | Command-line usage error (missing input, unknown option, `-o` without an argument, `-o ""`, more than one input file, …) |

Diagnostics are printed to stderr as clickable `file:line:col: error: message` lines. On success each written file prints one `wrote <path> (<n> bytes)` line.

The `--list-outputs` order matches the order the files are written; build systems use it (`dxxcpp_generate_lib()` does).

---

## 2. The dxx language

`.dxx` is a line-oriented syntax: every declaration ends with `;`, and declarations are told apart by their keyword.

### 2.1 File layout

A file has one `package`, then the type declarations, then the interfaces. Types must come before the interfaces that use them.

```dxx
//! Package declaration: decides the object path and the generated namespace
package com.example.calc;

//! Custom types
struct Point {
    int32 x;
    int32 y;
};

//! Type alias
using ConfigMap = map<string, string>;

//! Interfaces (a file may hold several)
interface Calculator {
    //! ...
};

interface Logger {
    method log(string message);
    signal logAdded(string message);
};
```

**Package declaration**

- The form is fixed: `a.b.c` with **at least two segments**; each segment must be a valid C++ identifier and not a C++ keyword
- A `.dxx` has exactly one `package`, and it must be the first declaration
- Three things are derived from it:

  | Derived | `com.example.calc` → |
  |---|---|
  | D-Bus object path | `/com/example/calc` |
  | C++ namespace | `Com::Example::Calc` |
  | Types header file name | `ComExampleCalcTypes.hpp` |
  | Include guard prefix | `COM_EXAMPLE_CALC` |

- **The D-Bus well-known name is not part of `.dxx`**: the build system injects it via `DBUSXX_SERVICE_NAME` (see [CMake integration](#5-cmake-integration))

### 2.2 Basic types

| `.dxx` | C++ | D-Bus signature |
|---|---|---|
| `int8` / `uint8` | `std::int8_t` / `std::uint8_t` | `y` |
| `int16` / `uint16` | `std::int16_t` / `std::uint16_t` | `n` / `q` |
| `int32` / `uint32` | `std::int32_t` / `std::uint32_t` | `i` / `u` |
| `int64` / `uint64` | `std::int64_t` / `std::uint64_t` | `x` / `t` |
| `float` | `float` | `d` (adapted as `double` by the library) |
| `double` | `double` | `d` |
| `bool` | `bool` | `b` |
| `string` | `std::string` | `s` |
| `bytes` | `std::vector<std::uint8_t>` | `ay` |

### 2.3 Containers

| Syntax | C++ | Notes |
|---|---|---|
| `vector<T>` | `std::vector<T>` | dynamic array |
| `array<T, N>` | `std::array<T, N>` | fixed-size array; `N` must be a positive integer |
| `map<K, V>` | `std::map<K, V>` | dictionary |

- Containers nest arbitrarily: `vector<vector<int32>>`, `map<string, vector<Point>>`
- A wrong number of template arguments is rejected explicitly (`vector<T> need 1 arg` / `array<T,N> need 2 args` / `map<K,V> need 2 args`)
- Type nesting depth is capped at **64 levels** (a deeper nest is an error, so neither the compiler nor the tool gets dragged down)
- Use `array<T, N>` for fixed-size arrays; there is **no** `T[N]` syntax in `.dxx`
- In `map<K, V>` the **`K` must resolve to a basic type** (`string`, `int32`, …) — structs and containers cannot be keys

### 2.4 Structs and aliases

```dxx
struct Point {
    int32 x;
    int32 y;
};

using ConfigMap = map<string, string>;
using UserId = int32;
```

The generated struct is an **aggregate** (no constructors), and therefore:

- **Fields are embedded by value**, so a struct may only reference structs **declared before** it: `struct A { B b; }; struct B { ... };` is rejected; write `B` first
- Structs must not include each other (cycles are rejected), and alias chains must not cycle either
- **Empty structs are not allowed** (at least one field) — `()` is not a valid D-Bus signature and the library cannot use it
- Field names must be unique and valid identifiers
- The generator emits a field-by-field `operator==` for every struct (as a member function, so the aggregate property survives); the library's property setter needs it to compare old and new values

Type names (struct and alias names) must be unique within the file and must not be C++ keywords — and note that **the basic type names plus `vector` / `array` / `map` are reserved**, so they cannot be used as struct or alias names either.

An alias is **pure sugar**: it is emitted as a `using` declaration in `Types.hpp`, but **every use site is expanded to the underlying type** — the `.dxx` above writes `method getConfig() -> ConfigMap;`, yet the Skeleton/Proxy show `std::map<std::string, std::string>`.

### 2.5 Interface members

#### Methods

```dxx
method add(int32 a, int32 b) -> int32;      // with a return value
method notify(string msg);                  // no return value (void)
method getConfig() -> map<string, string>;  // container return value
```

- Omitting `-> ReturnType` means `void`
- Parameters are written `Type name`, separated by `,`, with **no trailing comma**
- A parameter may not be called `aCallback` (that is the generated async callback parameter) unless the method is marked `@sync`

#### Signals

```dxx
signal valueChanged(int32 oldVal, int32 newVal);
signal itemAdded(Point item);
```

- The Skeleton side **only registers** signals (emitting `DBUSXX_SIGNAL(...)`) and generates **no send wrapper**: to emit, call `Server::emit(path, iface, signal, args...)` yourself (thread-safe, callable across threads) (**TODO**: a typed send wrapper per signal is planned)
- The Proxy side generates one listener per signal, `on<Capitalized signal name>` (see [3.4 Client proxy](#34-client-proxy))

#### Properties

```dxx
@readonly property version -> string{"1.0.0"};
property counter -> int32{0};
property metadata -> map<string, string>{};
property samples -> vector<int32>{1, 2, 3};
property origin -> Point{1, 2};
property history -> vector<Point>{{1, 2}, {3, 4}};
property untouched -> int32;                // no initializer
```

- `@readonly` → read-only property (`DBUSXX_PROPERTY_RO`); without it → read-write (`DBUSXX_PROPERTY_RW`)
- The initializer is written inside `{...}` and is passed through **verbatim** to the macro in the generated code, where the `,` inside it is absorbed by the variadic parameters of `DBUSXX_PROPERTY_*`; multi-value and nested-brace initializers therefore work
- Elements must be separated by `,`, with **no trailing comma** (consistent with parameter and field lists)
- Without an initializer the generator emits a value-initialized expression (`std::int32_t{}`)
- Initializer brace nesting is capped at **64 levels**
- When a property type contains a `,` (e.g. `map<string, string>`), the generator wraps it in `decltype(T{})` automatically — a trap you would have to handle yourself when writing the macro by hand

### 2.6 Annotations

Annotations precede a declaration and only affect code generation; they change no runtime semantics.

| Annotation | Applies to | Meaning |
|---|---|---|
| `@readonly` | property | read-only property |
| `@deprecated` | method / signal | mark as deprecated; the Proxy side carries `[[deprecated]]` (the Skeleton side only gets a comment, see below) |
| `@sync` | method | generate the sync shape only |
| `@async` | method | generate the two async shapes only |
| `@timeout(milliseconds)` | method | call timeout, **positive integer**, in milliseconds; converted to microseconds when generating (`@timeout(3000)` → `callSync<T, 3000000>`) |

- `@sync` and `@async` are **mutually exclusive**
- Misplaced annotations (e.g. `@timeout` on a property), duplicates, unexpected values (e.g. `@deprecated(true)`) and unknown annotations are all rejected
- On the Skeleton side `@deprecated` produces a `//! @deprecated` **comment** rather than a `[[deprecated]]` attribute — because `DBUSXX_METHOD(&Self::f)` takes the member's address, and marking it would make the generated header itself warn under `-Wdeprecated-declarations`. On the Proxy side it is a real attribute, so call sites do get the warning

### 2.7 Comments

A line comment runs from `//` to the end of the line and is dropped by the lexer; it works both on its own line and after a declaration.

The convention here is **`//!` for documentation comments** and `//` for ordinary ones (the spec and the `tests/*.dxx` fixtures follow it); the implementation treats the two identically. `.dxx` has no block comments.

### 2.8 Lexical details

- The `;` is mandatory; when one is missing the tool skips to the next recovery point and keeps going, so several errors are reported at once
- A `>>` in nested templates is **not** merged into a shift operator, so `map<string, vector<int32>>` closes correctly
- A UTF-8 BOM is skipped (saving from VS Code with "UTF-8 with BOM" is fine)
- String literals use double quotes; numeric literals are used for `array<T, N>`'s `N` and for `@timeout(...)`

---

## 3. Generated files

### 3.1 File list

| File | `--list-outputs` role | Contents |
|---|---|---|
| `<Package>Types.hpp` | `types` | Namespace, structs (with field-by-field `operator==`), aliases, aggregate static assertions |
| `<Interface>Skeleton.hpp` | `server` | `<Iface>Interface` (pure virtual base) + `<Iface>Server` (CRTP + reflection macros), declarations |
| `<Interface>Skeleton.cpp` | `server` | `<Iface>Server` constructor and method forwarding definitions |
| `<Interface>Proxy.hpp` | `client` | `<Iface>Proxy` declarations |
| `<Interface>Proxy.cpp` | `client` | `<Iface>Proxy` constructor plus method/listener definitions |

- **One header + one source per interface**; the types header is **shared per package**
- Header names come from the `.dxx` itself (package / interface names), not from the `LIB_PREFIX` of `dxxcpp_generate_lib(LIB_PREFIX ...)`
- Headers include each other by bare name (`#include "ThisFile.hpp"`, same directory), so consumers include them by bare name too
- Output is **split into declarations and definitions**: the call bodies of Proxy/Skeleton live in `.cpp`, so they compile into static/shared libraries without header-only duplicate-definition trouble
- The `#error` guard for `DBUSXX_SERVICE_NAME` only appears in **the two files that actually use the macro** — `<Interface>Proxy.hpp` and `<Interface>Skeleton.cpp`; `<Package>Types.hpp` and `<Interface>Skeleton.hpp` never mention it (the latter only holds registration macros like `DBUSXX_PATH` / `DBUSXX_METHOD`, which do not reference the service name), so they carry no guard

### 3.2 Types header

`<Package>Types.hpp` (one per package):

```cpp
#ifndef COM_EXAMPLE_CALC_TYPES_HPP
#define COM_EXAMPLE_CALC_TYPES_HPP

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <type_traits>
#include <vector>

namespace Com::Example::Calc {
struct Point {
    std::int32_t x;
    std::int32_t y;

    bool operator==(const Point& aOther) const {
        return x == aOther.x
            && y == aOther.y;
    }
};

using ConfigMap = std::map<std::string, std::string>;

static_assert(std::is_aggregate_v<Point>, "Point must be an aggregate");
} // namespace Com::Example::Calc
#endif
```

- Struct order follows the `.dxx` declaration order (which is exactly why "declare before use" is required)
- `operator==` is a **member function** so the aggregate property is preserved (the `static_assert` proves it)
- Aliases are always emitted after the structs

### 3.3 Server skeleton

`<Interface>Skeleton.hpp` / `.cpp` hold **two classes**:

```cpp
#ifndef COM_EXAMPLE_CALC_CALCULATOR_SKELETON_HPP
#define COM_EXAMPLE_CALC_CALCULATOR_SKELETON_HPP

#include <dbusxx/Server.hpp>
#include <dbusxx/Session.hpp>
#include <memory>

#include "ComExampleCalcTypes.hpp"

namespace Com::Example::Calc {
class CalculatorInterface {
public:
    virtual ~CalculatorInterface() = default;
    virtual std::int32_t add(std::int32_t a, std::int32_t b) = 0;
    virtual Point translate(const Point& p, std::int32_t dx) = 0;
    virtual std::map<std::string, std::string> getConfig() = 0;
    virtual void legacy(std::int32_t code) = 0;
    virtual std::int32_t syncOnly(std::int32_t val) = 0;
};

class CalculatorServer final : public Dbusxx::Server<CalculatorServer> {
public:
    explicit CalculatorServer(std::unique_ptr<CalculatorInterface> aIface);

    DBUSXX_PATH("/com/example/calc")
    DBUSXX_IFACE("com.example.calc.Calculator")

    std::int32_t add(std::int32_t a, std::int32_t b);
    DBUSXX_METHOD(add)

    Point translate(const Point& p, std::int32_t dx);
    DBUSXX_METHOD(translate)

    std::map<std::string, std::string> getConfig();
    DBUSXX_METHOD(getConfig)

    //! @deprecated
    void legacy(std::int32_t code);
    DBUSXX_METHOD(legacy)

    std::int32_t syncOnly(std::int32_t val);
    DBUSXX_METHOD(syncOnly)

    DBUSXX_SIGNAL(valueChanged, std::int32_t, std::int32_t)

    DBUSXX_PROPERTY_RO(version, std::string, {"1.0.0"})

    DBUSXX_PROPERTY_RW(counter, std::int32_t, {0})

private:
    std::unique_ptr<CalculatorInterface> mIface;
};

} // namespace Com::Example::Calc
#endif
```

Key points:

- You implement **all** pure virtuals of `<Iface>Interface` (miss one and the class stays abstract, so `make_unique` will not compile); `<Iface>Server` forwards calls to `mIface`
- Parameter and return passing is chosen automatically: scalars by value, `string`/containers/structs by `const&`
- `@sync` and `@async` **do not affect the server**: server members are always plain synchronous functions
- Signals are only registered via `DBUSXX_SIGNAL`; there is **no** `emitXxx` wrapper → call `Server::emit(path, iface, signal, args...)` yourself (**TODO**: a send wrapper is planned)
- Properties are registered directly on the Skeleton class (`DBUSXX_PROPERTY_RO/RW`), and the server can read/write them with `getLocalProperty` / `setLocalProperty`

The `.cpp` holds the constructor and the method definitions (excerpt):

```cpp
#include "CalculatorSkeleton.hpp"

#include <memory>
#include <utility>

#ifndef DBUSXX_SERVICE_NAME
#error "DBUSXX_SERVICE_NAME must be defined (e.g. -DDBUSXX_SERVICE_NAME=\"com.example.app\")"
#endif

namespace Com::Example::Calc {
CalculatorServer::CalculatorServer(std::unique_ptr<CalculatorInterface> aIface)
    : Dbusxx::Server<CalculatorServer>(DBUSXX_SERVICE_NAME)
    , mIface(std::move(aIface)) {}

std::int32_t CalculatorServer::add(std::int32_t a, std::int32_t b) {
    return mIface->add(a, b);
}
}
```

> ⚠️ `DBUSXX_IFACE` uses `package + interface name` (`com.example.calc.Calculator`) while `DBUSXX_PATH` uses the package expanded into a path (`/com/example/calc`) — one interface type maps to one path, with **no** extra sub-path per interface.

### 3.4 Client proxy

`<Interface>Proxy.hpp` declares (the definitions live in the `.cpp`). The excerpt below drops the `//! Sync/Async` comment lines and some methods:

```cpp
class CalculatorProxy {
public:
    explicit CalculatorProxy();

    CalculatorProxy(const CalculatorProxy&) = delete;
    CalculatorProxy& operator=(const CalculatorProxy&) = delete;
    CalculatorProxy(CalculatorProxy&&) = default;
    CalculatorProxy& operator=(CalculatorProxy&&) = default;

    [[nodiscard]] Dbusxx::Reply<std::int32_t> add(std::int32_t a, std::int32_t b);

    [[nodiscard]] Dbusxx::PendingReply<std::int32_t> addAsync(std::int32_t a, std::int32_t b);
    [[nodiscard]] Dbusxx::Status addAsync(
        std::function<void(Dbusxx::Reply<std::int32_t>)> aCallback,
        std::int32_t a, std::int32_t b);

    [[nodiscard]] Dbusxx::Status onValueChanged(
        std::function<void(std::int32_t oldVal, std::int32_t newVal)> aCallback);
};
```

**Method shapes** (how many functions are generated depends on the annotations)

| Annotation | Generated functions |
|---|---|
| none | `<name>` → `Dbusxx::Reply<T>` (`callSync`)<br>`<name>Async(args...)` → `Dbusxx::PendingReply<T>` (`callAsync`)<br>`<name>Async(aCallback, args...)` → `Dbusxx::Status` |
| `@sync` | only `<name>` |
| `@async` | only the two `<name>Async` (**no** bare `<name>`) |

- The two async overloads **share** the single name `<name>Async` and are told apart by the second parameter: the handle form returns `PendingReply<T>`, the callback form returns `Status`
- A `void` return becomes `Dbusxx::Reply<void>` / `Dbusxx::PendingReply<void>`
- Every method is `[[nodiscard]]`; `@deprecated` methods additionally carry `[[deprecated]]` (the call site warns, and under `-Werror=deprecated-declarations` the build fails)
- `@timeout` works as before: `mClient.callSync<T, microseconds>("name", ...)`; with no return value it generates `callSync<void, microseconds>(...)`

**Signal listeners**

Every signal yields a listener named `on` plus the capitalized signal name, forwarding to `Client::listenSignal`:

```cpp
Dbusxx::Status onValueChanged(std::function<void(std::int32_t, std::int32_t)> aCallback);
```

A `@deprecated` signal still gets its listener, but with `[[deprecated]]`.

> ⚠️ **Listeners are permanent and cannot be cancelled** (**TODO**: a cancellable handle is planned): once registered they stay active until the Proxy is destroyed. The callback runs on the **event loop thread** at any time after registration, so a closure may only capture objects that **outlive the Proxy** (file-scope statics, top-level objects, or locals declared before the Proxy). Capturing a block-local by reference and firing after that block has exited is *stack-use-after-scope*.

**No property accessors are generated** (**TODO**: generated get/set accessors are planned)

A `property` in `.dxx` is registered on the Skeleton side only; the Proxy side gets **no** `getVersion()` / `setCounter()` accessors and does not expose its inner `Client`. To read or write properties from a client, use the library's `Client` directly:

```cpp
Dbusxx::Client c(Dbusxx::SessionType::USER, DBUSXX_SERVICE_NAME,
                 "/com/example/calc", "com.example.calc.Calculator");
auto ver = c.getProperty<std::string>("version");
(void)c.setProperty<std::int32_t>("counter", 10);
```

The `.cpp` hard-codes the service name, path and interface name in the constructor:

```cpp
CalculatorProxy::CalculatorProxy()
    : mClient(Dbusxx::SessionType::USER, DBUSXX_SERVICE_NAME,
        "/com/example/calc", "com.example.calc.Calculator") {}
```

- The service name comes from `DBUSXX_SERVICE_NAME`, **not** from a Proxy constructor argument: one Proxy library serves exactly one service name, symmetric with the server, and the client never writes the macro itself (the build system injects it)
- The connection type is fixed to `SessionType::USER`; for the system bus or peer-to-peer, use the library's `Client`/`Session` directly

---

## 4. Using the generated code

### 4.1 Server

```cpp
#include "CalculatorSkeleton.hpp"

#include <map>
#include <memory>
#include <string>

using namespace Com::Example::Calc;

// Implement every pure virtual of <Iface>Interface (five methods, matching the .dxx in §0)
class CalcImpl : public CalculatorInterface {
public:
    std::int32_t add(std::int32_t a, std::int32_t b) override { return a + b; }

    Point translate(const Point& p, std::int32_t dx) override {
        return Point { p.x + dx, p.y };
    }

    std::map<std::string, std::string> getConfig() override { return {}; }
    void legacy(std::int32_t code) override { (void)code; }
    std::int32_t syncOnly(std::int32_t val) override { return val; }
};

int main() {
    CalculatorServer server(std::make_unique<CalcImpl>());
    server.run();          // registers the interface and runs the event loop (blocking)
    return 0;
}
```

Emitting a signal (your own call, there is no generated wrapper; **TODO**: a generated wrapper is planned):

```cpp
(void)server.emit("/com/example/calc", "com.example.calc.Calculator",
                  "valueChanged", 1, 2);
```

### 4.2 Client

```cpp
#include "CalculatorProxy.hpp"

#include <iostream>

using namespace Com::Example::Calc;

int main() {
    CalculatorProxy proxy;

    auto r = proxy.add(20, 22);
    if (!r.isError()) {
        std::cout << r.value() << "\n";       // 42
    }

    // Async (callback form)
    (void)proxy.addAsync([](Dbusxx::Reply<std::int32_t> aRep) {
        std::cout << "async " << aRep.value() << "\n";
    }, 1, 2);

    // Signal listener: captured objects must outlive the proxy
    (void)proxy.onValueChanged([](std::int32_t aOld, std::int32_t aNew) {
        std::cout << aOld << " -> " << aNew << "\n";
    });

    return 0;
}
```

### 4.3 Directory and includes

The generated files are **flat** in one directory and include each other by bare name, so adding that directory to the include path is all it takes:

```cmake
target_include_directories(my_app PRIVATE ${GEN_DIR})
```

Include them by bare file name (`"CalculatorProxy.hpp"`). One generation directory holds exactly one `<Package>Types.hpp`, so **each `.dxx` needs its own output directory** (see the next section).

---

## 5. CMake integration

Installing `dbusxx` also installs `dxxcppGenerator.cmake` into the package directory, so after `find_package(dbusxx)` you can call `dxxcpp_generate_lib()`.

### 5.1 Basic usage

```cmake
cmake_minimum_required(VERSION 3.15)
project(hello_service CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(dbusxx REQUIRED)

dxxcpp_generate_lib(
    LIB_PREFIX hello
    INPUT      hello.dxx
    SERVICE    com.example.hello
)

add_executable(hello_service src/service.cpp)
target_link_libraries(hello_service PRIVATE hello_dxx_server)
```

One call creates **three targets**:

| Target | Kind | Contents |
|---|---|---|
| `<LIB_PREFIX>_dxx_types` | `INTERFACE` | Types header + both include roots + `DBUSXX_SERVICE_NAME="<SERVICE>"` + the `dbusxx` dependency |
| `<LIB_PREFIX>_dxx_server` | `STATIC` | `<Iface>Skeleton.hpp/.cpp` (for the service implementation) |
| `<LIB_PREFIX>_dxx_client` | `SHARED` | `<Iface>Proxy.hpp/.cpp` (for clients) |

The latter two link `_types` `PUBLIC`, so linking `_server` / `_client` automatically brings in the include paths (build tree or install tree), the service-name macro and the `dbusxx` dependency.

**Arguments**

| Argument | Required | Meaning |
|---|---|---|
| `LIB_PREFIX <prefix>` | ✔ | Prefix for target names, the package and the install directory; unique per call |
| `INPUT <file.dxx>` | ✔ | The interface description (relative paths are turned absolute) |
| `SERVICE <name>` | ✔ | D-Bus well-known name, emitted as the `DBUSXX_SERVICE_NAME` macro |
| `OUTPUT_DIR <dir>` | ✘ | Output directory, defaults to `${CMAKE_CURRENT_BINARY_DIR}/dxx_<file-stem>` |
| `INSTALL_CLIENT <on\|off>` | ✘ | Defaults to `on`; `off` means build-only, install nothing |

**When generation happens**: at configure time the helper runs `dxxcpp --list-outputs` once to learn the file names, then registers an `add_custom_command` that generates for real at **build time**. The `.dxx` is added to `CMAKE_CONFIGURE_DEPENDS`, so editing it reconfigures and regenerates on the next build without a manual `cmake` run.

**`OUTPUT_DIR` exclusivity**: the generation rule is directory-scoped, so

- same `.dxx` + same directory → the existing rule is reused (you may create further `LIB_PREFIX`/`SERVICE` targets from the same sources)
- same directory claimed by a **different** `.dxx` → error (otherwise two interfaces would write into the same `<Package>Types.hpp`)
- same `.dxx` declared again from another directory → error (use a different build directory, as the error message suggests)

**Tool discovery order**: `-DDXXCPP_EXE=<path>` → `<prefix>/bin/dxxcpp` derived from `dbusxx_DIR` → `dxxcpp` on `PATH`. If none is found the helper fails with a hint.

### 5.2 Publishing to downstream

With the default (`INSTALL_CLIENT` `on`) the install tree receives:

```
lib/lib<prefix>_dxx_client.so
include/<prefix>_dxx/<Package>Types.hpp
include/<prefix>_dxx/<Iface>Proxy.hpp
lib/cmake/<prefix>_dxx/Config.cmake      # find_dependency(dbusxx CONFIG)
lib/cmake/<prefix>_dxx/Targets.cmake
lib/cmake/<prefix>_dxx/Targets-noconfig.cmake
```

A downstream project then does:

```cmake
find_package(dbusxx CONFIG REQUIRED)
find_package(hello_dxx CONFIG REQUIRED)

add_executable(client_app main.cpp)
target_link_libraries(client_app PRIVATE hello_dxx_client)
```

The downstream project does **not** define `DBUSXX_SERVICE_NAME` itself — the service name sits on `hello_dxx_types` and propagates with the target.

> **The server side is never published**: no Skeleton header and no `.a` in the install tree. Whoever needs to implement the same interface generates the skeleton from the `.dxx` again — what a server and its clients share is the interface definition (`.dxx`), not an implementation.
>
> Also note `_types` is an `INTERFACE` target that produces no file, but it **must be part of the export set**: since `_client` links it `PUBLIC`, leaving it out means "an exported target depends on a non-exported target" and CMake fails at generate time.

### 5.3 Without the helper

To wire it into your own build system, take the file names from `--list-outputs` and register `add_custom_command` the way `dxxcpp_generate_lib()` does. The essentials:

- Put both the `.dxx` and the `dxxcpp` executable into `DEPENDS`
- Give every `.dxx` its own output directory
- The consumer's include path needs **both** the generation directory (for bare includes like `"<Iface>Proxy.hpp"`) **and** dbusxx's installed include root (for `<dbusxx/...>`)

---

## 6. Rules and limits

The tool runs a full syntax + semantic pass before generating, and **any error means no file is produced at all**.

### 6.1 Hard constraints

| Constraint | Detail |
|---|---|
| `package` has at least two segments | `a.b.c` form; segments must be valid identifiers and not C++ keywords |
| Every declaration ends with `;` | A missing one is reported, then the parser resumes at the next recovery point |
| Structs are declared before use | Fields are embedded by value |
| Structs must not be empty | at least one field |
| No struct/alias cycles | alias chains and mutually including structs are rejected |
| Type nesting ≤ 64 levels | for `vector<vector<...>>` and friends |
| Initializer brace nesting ≤ 64 levels | for `{{1,2},{3,4}}` and friends |
| No trailing comma in initializers | consistent with parameter and field lists |
| Generated names must be unique | see below |

### 6.2 Member names and generated names

Within one interface, **methods / signals / properties share a single member namespace**, and a duplicate reports `duplicate member`:

```dxx
interface I {
    method f(int32 v) -> int32;
    property f -> int32;                // duplicate member 'f'
};
```

On top of that the Proxy declares names for each member, and a collision would make the Proxy fail to compile, so the tool rejects it up front:

- method `X` → `X` (unless `@async`) and `XAsync` (unless `@sync`)
- signal `S` → `onS` (first letter capitalized)
- **properties do not take part** (the Proxy has no property names)

Cases that report `'<name>' is generated twice in <I>: by <ownerA> and <ownerB>`:

```dxx
interface I {
    method f(int32 v) -> int32;
    method fAsync(int32 v) -> int32;     // 'fAsync' is generated twice
};

interface I {
    method onValueChanged(int32 v) -> int32;
    signal valueChanged(int32 v);        // 'onValueChanged' is generated twice
};

interface I {
    signal value(int32 v);
    signal Value(int32 v);               // both capitalize to 'onValue' → generated twice
};
```

These two are **legal** (the tool is precise about it):

```dxx
interface I {
    method f(int32 v) -> int32;
    property fAsync -> int32;            // properties are not in the Proxy, no clash
};

interface I {
    @sync method g(int32 aCallback) -> int32;   // @sync generates no callback overload, so the parameter is free
};
```

### 6.3 Names and keywords

- Type, interface, member, field and parameter names must be valid C++ identifiers
- They must not be C++ keywords (`new`, `class`, …). The generated code escapes **field and parameter** names (`new` → `new_`), but names that must become identifiers as-is — **type names, interface names, member names** — are rejected outright
- A parameter may not be named `aCallback` (the generated async callback parameter) unless the method is `@sync`

---

## 7. Troubleshooting

| Symptom | Cause / fix |
|---|---|
| `error: no input .dxx file` (exit 2) | No input file given |
| `error: cannot open '...'` (1) | The input file does not exist or is unreadable |
| `error: multiple input files` (2) | Only one `.dxx` per run |
| `error: '<flag>' doesn't take a value` (2) | `=` value given to `--dbus` / `--list-outputs` / `--help` |
| `file:line:col: error: ...` + `has syntax error(s)` / `has semantic error(s)` (1) | Fix the `.dxx`; the semantic pass reports several errors at once |
| Generated code fails with `#error "DBUSXX_SERVICE_NAME must be defined"` | The service name was not defined by the build system: use `dxxcpp_generate_lib()`, or add `-DDBUSXX_SERVICE_NAME="com.example.app"` yourself |
| `unknown argument(s): ...` | A misspelled/unsupported argument to `dxxcpp_generate_lib()` (e.g. the removed `INSTALL_SERVER`) |
| `target 'dbusxx' is missing` | `find_package(dbusxx)` was not called before `dxxcpp_generate_lib()` |
| `dxxcpp not found` | Installed under another prefix: pass `-DDXXCPP_EXE=<path>`, or install `dbusxx` and `dxxcpp` under the same prefix |
| `OUTPUT_DIR '...' is already used by '...'` | Two `.dxx` files share one directory → give each its own `OUTPUT_DIR` |
| `INPUT '...' was generated in '...'` | The same `.dxx` was already declared in another binary dir → use a different `OUTPUT_DIR` or build directory |
| `'...' declares no interface` | Only types, no `interface`; `dxxcpp_generate_lib()` needs at least one to build the server/client libraries |

Debugging recipe: run the tool on its own to see the diagnostics, then hand it to CMake:

```bash
dxxcpp --list-outputs path/to/foo.dxx     # just see how the tool reads this input
dxxcpp path/to/foo.dxx -o /tmp/gen        # generate standalone, easy to compile-check
```

---

## 8. References

- Syntax example: `tool/dxxcpp/tests/Sample.dxx`
- Negative cases (one per validation rule, with the expected error message): `tool/dxxcpp/tests/VerifyNegative.cpp`
- Generator sources: `tool/dxxcpp/src/` (`Lexer` → `Parser` → `Sema` → `Codegen`)
- CMake helper source: `library/cmake/dxxcppGenerator.cmake`
- Library API: the repository root `README.md` and `docs/en/overview.en.md`
