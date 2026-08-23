# dbusxx Documentation Overview

> This directory (`docs/en`) contains the complete API reference for the dbusxx library (English). Chinese version: [docs/cn](../cn/overview.md).

## About the library

dbusxx is a C++17 D-Bus library built on systemd sd-bus. It provides **type-safe** registration and invocation of methods, signals and properties, covering the system bus, the user bus and peer-to-peer connections. All low-level details (D-Bus signatures, messages, handles, event loop) are wrapped — you only write business code.

## Documentation map

Organized by "core → supporting":

| Document | Header | Contents |
| --- | --- | --- |
| [Session.en.md](Session.en.md) | `Session.hpp` | **Core**: session (connection), registering methods/signals/properties, sync & async calls, local & remote properties |
| [Server.en.md](Server.en.md) | `Server.hpp` | **Core**: server wrapper (CRTP + reflection macros), an out-of-the-box server |
| [Client.en.md](Client.en.md) | `Client.hpp` | **Core**: remote-service proxy (self-managed / external-loop modes) |
| [Looper.en.md](Looper.en.md) | `Looper.hpp` | Event loop (sd-event), dispatching messages, cross-thread task posting |
| [MetaObject.en.md](MetaObject.en.md) | `MetaObject.hpp` | Reflection meta object and the `DBUSXX_*` annotation macros |
| [Message.en.md](Message.en.md) | `Message.hpp` | Type-safe D-Bus message (stream-style read/write) |
| [Reply.en.md](Reply.en.md) | `Reply.hpp` | Typed reply (method return value) |
| [PendingReply.en.md](PendingReply.en.md) | `PendingReply.hpp` | Async-call handle |
| [Status.en.md](Status.en.md) | `Status.hpp` | Status codes and error handling |
| [Utils.en.md](Utils.en.md) | `Utils.hpp` | Common types (`SessionType`) |

## Quick start by scenario

- **Expose a service**: see [Server.en.md](Server.en.md) + [MetaObject.en.md](MetaObject.en.md) (annotate interfaces with macros)
- **Call a remote service**: see [Client.en.md](Client.en.md) (high-level proxy) or [Session.en.md](Session.en.md) (low-level direct calls)
- **Signals & properties**: see [Session.en.md](Session.en.md) / [Client.en.md](Client.en.md)
- **Event loop**: see [Looper.en.md](Looper.en.md)
- **Return values & errors**: see [Reply.en.md](Reply.en.md), [PendingReply.en.md](PendingReply.en.md), [Status.en.md](Status.en.md)

## Suggested reading order

1. [Status.en.md](Status.en.md) — understand the unified error handling first
2. [Session.en.md](Session.en.md) — core concepts (single-threaded model, connections, registration & calls)
3. [Server.en.md](Server.en.md) / [Client.en.md](Client.en.md) — the most common server / client usage
4. Consult the remaining supporting docs (message, reply, event loop, macros, ...) as needed
