# AGENTS.md

C++17 D-Bus library (`libdbusxx`) built on systemd **sd-bus** (requires `libsystemd >= 249`).
Library API docs live in [`docs/cn`](docs/cn/overview.md) (Chinese) / [`docs/en`](docs/en/overview.en.md) (English) — **read these instead of guessing the API**.

## Layout

| Path | Contents |
| --- | --- |
| `library/include/*.hpp` | Public headers (one per core type: `Session`, `Server`, `Client`, `Looper`, `Reply`, `PendingReply`, `Message`, `MetaObject`, `Status`, `Args`, `UnixFd`, `Utils`) |
| `library/include/private/**` | Internal headers, grouped by `adaptor/` `message/` `method/` `session/` |
| `library/src/**` | `.cpp` — mirrors the header tree; `Server.hpp` is header-only (no `.cpp`) |
| `tests/unit_tests`, `tests/benchmark_tests` | GoogleTest / GoogleBenchmark suites (see below) |
| `tool/dxxcpp` | **Standalone** `.dxx` IDL code generator — separate CMake project, NOT part of the root build |
| `example/`, `example_install/` | Usage examples; `example_install/` verifies consuming the installed package via `find_package(dbusxx)` |
| `.workflow/ci.yml` | CI (Gitee Go, GCC). There is no `.github/` directory. |

## Build & test

Prerequisites: CMake >= 3.15, a C++17 compiler (GCC 9+ / Clang), and libsystemd >= 249 + `pkg-config` (`sudo apt install cmake g++ pkg-config libsystemd-dev`).

```bash
# Debug (adds ASan+UBSan via the dbusxx_opts target)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build
# Release (-O3 -DNDEBUG -flto -march=native on GCC)
cmake -S . -B build_release -DCMAKE_BUILD_TYPE=Release && cmake --build build_release

ctest --test-dir build --output-on-failure                    # unit tests (benchmarks disabled)
cmake -S . -B build_release -DCMAKE_BUILD_TYPE=Release -DDBUSXX_CTEST_BENCHMARKS=ON
ctest --test-dir build_release --output-on-failure            # unit tests + benchmarks

./build/tests/unit_tests/dbusxx_unit_tests --gtest_filter='StatusTest.*'   # single binary/filter
```

- CMake options: `DBUSXX_BUILD_TESTS` (ON), `DBUSXX_BUILD_BENCHMARKS` (ON), `DBUSXX_CTEST_BENCHMARKS` (OFF — benchmarks are excluded from `ctest` unless enabled).
- Tests/benchmarks spawn their **own private `dbus-daemon`** and override `DBUS_SESSION_BUS_ADDRESS` (`tests/unit_tests/TestUtil.hpp`), so **no system session bus is needed**. If `dbus-daemon` is missing, bus-dependent tests `GTEST_SKIP()`.
- CI runs `ctest --test-dir build_release --rerun-failed --output-on-failure`, then the same for `tool/dxxcpp`.

### Adding a test or benchmark

New `.cpp` files are **not auto-discovered** — add each one by hand to the `add_executable(...)` source list in `tests/unit_tests/CMakeLists.txt` (or `tests/benchmark_tests/CMakeLists.txt`).

- Unit tests: `TestXxx.cpp`, free `TEST(SuiteName, CaseName)` (suites end in `Test`; use `TEST_F` only when a fixture/threading is needed). Custom `main.cpp` starts the private daemon and prints a banner; tests link `GTest::gtest` — **not** `gtest_main`, and gmock is not linked.
- Benchmarks: split by layer (`BenchmarkStatus` pure-logic … `BenchmarkE2E`). Custom `BenchmarkMain.cpp`; do not link `benchmark_main`.

## Library conventions

- Single-threaded model: a `Session` is driven by one `Looper` on its owner thread. Cross-thread calls go through `Looper::post()`; use `isOwnerThread()` to branch between direct execution and posting. Never touch sd-bus handles off the owner thread — async handler cleanup **must** be posted back to the sd-bus thread.
- `Server<Derived>` is CRTP (`public MetaObject<Derived>`); the `DBUSXX_*` macros self-register members at static-init time via `MetaObject::registry()`.
- Errors are returned as `Status`/`StatusCode` — **no exceptions to callers**. Failures return `if (st.isError()) return st;`. Value-returning APIs are `[[nodiscard]]`.

| Kind | Style | Example |
| --- | --- | --- |
| Namespace | `Dbusxx`, `Dbusxx::Private`, `Dbusxx::Adaptor`, `Dbusxx::Method` | |
| Types / classes | PascalCase | `SessionPrivate`, `VTableRegistrar` |
| Methods / free functions | camelCase | `registerBuilder`, `callSync` |
| Member variables | `m` + PascalCase | `mSession`, `mPrivate` |
| Function params | `a` + PascalCase | `aSession`, `aCallback` |
| Macros / enums | `DBUSXX_` + UPPER_SNAKE; `enum class` + UPPER_SNAKE members | `DBUSXX_METHOD`, `StatusCode::SUCCESS` |

- 4-space indent, 1TBS braces, member-init-list commas **leading**. Doxygen `/** */` on classes, `//!` on members.
- Include guards are hand-written `DBUSXX_..._HPP` — **no `#pragma once`**. Keep the existing (sometimes inconsistent) guard names when editing.
- Include style: public headers include siblings by bare name (`#include "Status.hpp"`); private headers are included as `#include "private/<group>/X.hpp"` (e.g. `private/method/Method.hpp`).
- Prefer RAII wrappers (`RawBusSharePtr`, `RawSlotSharePtr`, `RawMessageSharePtr`, `RawEventSharePtr`, `UnixFd`) over raw sd-bus pointers. Small accessors are marked `inline`.
- Adding a source file is a **manual** step: put the `.cpp` in `library/src/` (or `library/src/private/<group>/`) **and** list it in the `SOURCES` list in `library/CMakeLists.txt` — there is no globbing. Public↔private layout mirrors between `library/include/**` and `library/src/**`.

## Pitfalls

- **`VTableRegistrar::addSiganl` is intentionally misspelled** (not `addSignal`) and is called from multiple sites — do not rename in isolation; the public `Session::RegisterBuilder::addSignal` is spelled correctly.
- Marshalling reads that look up values need care: initialize `const char*` to `nullptr` before reading a string; `dup()` any received `UnixFd` (sd-bus only lends it).
- Release builds use `-march=native` (artifacts are machine-specific); Debug builds add ASan+UBSan, so leaks/UB fail test runs.
- Offline builds: drop a googletest checkout into `tests/unit_tests/third_party/googletest` (gitignored); otherwise CMake falls back to `FetchContent` (GTest v1.14.0, Benchmark v1.8.3), which needs network.
- `library/include/private/**` is **installed** to `<includedir>/dbusxx/private/...` and is includable by consumers.

## dxxcpp (separate project)

```bash
cmake -S tool/dxxcpp -B tool/dxxcpp/build -DCMAKE_BUILD_TYPE=Release
cmake --build tool/dxxcpp/build -j
ctest --test-dir tool/dxxcpp/build --output-on-failure
cmake --install tool/dxxcpp/build --prefix /usr/local   # installs <prefix>/bin/dxxcpp
```

Multi-stage pipeline: `Lexer` → `Parser` (→ `Ast.hpp`) → `Sema` (validate + `Ir.hpp`) → `Codegen` (`Cpp.hpp` helpers). Uses `a`/`m` prefixes, `//!` comments, `DXXCPP_..._HPP` guards. `.dxx` grammar and generated artifacts (`<Package>Types.hpp`, `<Iface>Skeleton.*`, `<Iface>Proxy.*`) are documented in [`tool/dxxcpp/docs/cn/dxxcpp.md`](tool/dxxcpp/docs/cn/dxxcpp.md); syntax examples: `tool/dxxcpp/tests/Sample.dxx` (valid) and `tests/Bad.dxx` (invalid).

`dxxcpp_generate_lib(LIB_PREFIX ... INPUT ... SERVICE ...)` is defined in `library/cmake/dxxcppGenerator.cmake` and installed with the CMake package. Generated `<Iface>Skeleton.cpp`/`Proxy.hpp` `#error` out unless `DBUSXX_SERVICE_NAME` is defined — the CMake helper sets it for you; a hand-rolled build must define it too.

## Docs

Docs are **mirrored CN/EN**: `docs/cn/<Name>.md` ↔ `docs/en/<Name>.en.md` (same `Name`), and `tool/dxxcpp/docs/cn/dxxcpp.md` ↔ `tool/dxxcpp/docs/en/dxxcpp.en.md`. `README.md` ↔ `README.en.md`. Each `docs/cn/<Name>.md` maps to one header (`> 对应头文件：library/include/<Name>.hpp`) and is indexed in `docs/cn/overview.md`. When adding/changing an API, update **both** languages and the overview table.
