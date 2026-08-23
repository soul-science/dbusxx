# Utils — Common Types

> Header: `library/include/Utils.hpp` · public namespace: `Dbusxx`

## Overview

`Utils.hpp` currently defines the most fundamental enum of the library, `SessionType`, which identifies which bus a session (connection) is bound to. It is widely used by core types such as `Session`, `Client` and `Server`.

## Enum: `SessionType`

```cpp
enum class SessionType : uint8_t {
    SYSTEM,     // system bus
    USER,       // user/session bus
    PEER,       // peer-to-peer connection (no bus daemon)
    INVALID     // uninitialized / invalid
};
```

| Value | Meaning |
| --- | --- |
| `SYSTEM` | System bus (e.g. systemd, NetworkManager and other system services) |
| `USER` | User/session bus (the usual choice for desktop applications) |
| `PEER` | Peer-to-peer connection — no bus daemon, direct connection over a socket |
| `INVALID` | Placeholder for uninitialized or invalid state |

## Per-API Examples

```cpp
#include <dbusxx/Utils.hpp>

using namespace Dbusxx;

// (1) SessionType::SYSTEM — system bus
SessionType sys = SessionType::SYSTEM;

// (2) SessionType::USER — user/session bus
SessionType usr = SessionType::USER;

// (3) SessionType::PEER — peer-to-peer connection
SessionType peer = SessionType::PEER;

// (4) SessionType::INVALID — uninitialized placeholder
SessionType inv = SessionType::INVALID;

// Comparison and switch
if (usr == SessionType::USER) { /* ... */ }
switch (sys) {
    case SessionType::SYSTEM:   break;
    case SessionType::USER:     break;
    case SessionType::PEER:     break;
    case SessionType::INVALID:  break;
}
```

## Related

- `SessionType` is one of the entry parameters of `Session::createSession()` and the `Client`/`Server` constructors — see [Session.en.md](Session.en.md), [Client.en.md](Client.en.md), [Server.en.md](Server.en.md).
