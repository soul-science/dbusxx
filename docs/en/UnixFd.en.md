# UnixFd file descriptor

## Overview

D-Bus can pass **Unix file descriptors** inside messages (type signature `h`). Under the hood `SCM_RIGHTS` makes the kernel hand the open file description from the sender to the receiver — the receiving end gets a **new fd number referring to the same open file**. dbusxx wraps this in the RAII type `UnixFd`, which behaves like an ordinary value while managing `dup`/`close` for you, avoiding both leaks and double closes.

`UnixFd` can be used directly as a **method argument/return value or signal parameter**, and as a member of an **aggregate struct** (e.g. `struct { std::string tag; UnixFd data; }` → `(sh)`).

> ⚠️ **`UnixFd` cannot be used as a property type.** Properties are stable, comparable, cacheable values; every `dup`/copy of an fd yields a new fd with no stable comparable value, which conflicts with that model. Pass fds via method arguments/return values or signals instead.

## Class: `UnixFd`

| Method | Description |
| --- | --- |
| `UnixFd()` | constructs an invalid fd (`get()` returns -1) |
| `UnixFd(int fd)` | takes ownership of an open fd (validated via `fcntl(F_GETFD)`; an invalid fd is recorded as `INVALID_ARG`) |
| `UnixFd(const UnixFd&)` | **copy = `dup`**: a fresh fd referring to the same open file |
| `UnixFd(UnixFd&&)` | move transfers ownership; the source becomes invalid |
| `operator=` (copy/move) | same semantics; first closes the currently held fd |
| `int get() const` | the raw fd (-1 when invalid) |
| `Status status() const` | whether the fd is valid (`SUCCESS` / `INVALID_ARG`) |
| `int release()` | relinquishes ownership and returns the raw fd (the object is then invalid and will not `close`) |
| `void reset(int fd = -1)` | closes the current fd and replaces it (or empties it) |
| `bool operator==(const UnixFd&) const` | whether both hold the same fd number |

## Why not a raw `int`?

A bare fd is ambiguous in D-Bus: is the sender, the receiver, or the message destructor responsible for `close`? `UnixFd` pins the rules down:

- **copy = `dup`** — every copy produces a new fd that must be `close`d independently; each instance closes its own, so there is never a double close;
- **move = ownership transfer** — the source is emptied and the destination is the sole owner;
- **destructor = `close`** — no leaks.

## Example

```cpp
#include <unistd.h>      // pipe / write / dup
#include <utility>       // std::move
#include <vector>        // std::vector<UnixFd>

#include <dbusxx/UnixFd.hpp>
#include <dbusxx/Client.hpp>
#include <dbusxx/Reply.hpp>

using namespace Dbusxx;

// ── Server ──────────────────────────────────────────────────
// Expose an "echo the fd back" method (signature h) via Server<Derived>:
// class MyServer : public Server<MyServer> {
// public:
//     MyServer() : Server("com.example.Svc") {}
//     DBUSXX_PATH("/svc")
//     DBUSXX_IFACE("com.example.Iface")
//     UnixFd echoFd(UnixFd fd) { return fd; }            // DBUSXX_METHOD(echoFd)
//     std::vector<UnixFd> echoFdList(std::vector<UnixFd> fds) { return fds; }
//                                                         // DBUSXX_METHOD(echoFdList) → ah
// };
// MyServer server;
// std::thread serverThread([&] { server.run(); });        // event-loop thread

// ── Client (self-managed: owns a Session + event-loop thread) ─
Client client(SessionType::USER, "com.example.Svc",
              "/svc", "com.example.Iface");

// Send the pipe read end to the server and get it back
int pipefd[2];
::pipe(pipefd);

UnixFd in(pipefd[0]);                                   // take ownership (closes on destruction)
auto rep = client.callSync<UnixFd>("echoFd", in);       // round-trip; get the fd the server sent back
if (rep.isError()) { /* handle error */ }

// rep.value() is a *borrowed* UnixFd (closed when rep dies) —
// copy it if you need to keep it: copy = dup, each holds its own fd, closed independently.
UnixFd owned = rep.value();
::write(owned.get(), "hi", 2);                           // read/write the pipe via the raw fd

// Batch: std::vector<UnixFd> (signature ah)
UnixFd in2(::dup(owned.get()));
auto repBatch = client.callSync<std::vector<UnixFd>>(
    "echoFdList", std::vector<UnixFd>{ std::move(owned), std::move(in2) });

// Out of scope: UnixFd / rep each close their fd on destruction — no manual
// management; don't close an fd you have handed to a UnixFd (release() it first).
```

> Note: once an fd is handed to a `UnixFd` (or written into a message), its lifetime is managed by `UnixFd`/D-Bus. Do not manually `close` an fd you have handed over, unless you first take ownership back with `release()`.
