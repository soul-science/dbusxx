# Status — Status & Error Handling

> Header: `library/include/Status.hpp` · public namespace: `Dbusxx`

## Overview

`Status.hpp` defines the library-wide result codes and status wrapper:

- `StatusCode` — enumerates all possible result codes (success / various errors), grouped by category;
- `Status` — a lightweight wrapper around `StatusCode` exposing `isSuccess()` / `isError()` and friends;
- `statusMessage()` — turns a status code into a human-readable string.

Nearly every operation in the library (method calls, signal subscriptions, property read/write, registration, ...) returns or carries a `Status`. Prefer `isSuccess()` / `isError()` over implicit boolean conversion when inspecting results.

## Enum: `StatusCode`

Values are grouped by category (backed by `uint8_t`):

| Category | Value | Meaning |
| --- | --- | --- |
| Success | `SUCCESS` | Operation succeeded |
| Caller error | `INVALID_ARG` | Invalid argument |
| Caller error | `NOT_FOUND` | Service/object/interface not found |
| Caller error | `NO_SERVICE` | Service not found |
| Caller error | `NO_METHOD` | Method not found (possibly path/interface/method error) |
| Caller error | `ACCESS_DENIED` | Insufficient permission |
| Caller error | `NAME_EXISTS` | Bus name already taken |
| Connection error | `NOT_CONNECTED` | Not connected to the bus |
| Connection error | `CONN_RESET` | Connection reset |
| Connection error | `BUSY` | Resource busy |
| Transport error | `TIMEOUT` | Call timed out |
| Transport error | `NO_MEMORY` | Out of memory |
| Transport error | `NO_REPLY` | No reply received |
| Transport error | `IO_ERROR` | I/O error |
| Transport error | `MSG_TOO_LONG` | Message too long |
| Transport error | `LIMIT_EXCEEDED` | Limit exceeded |
| Protocol error | `PROTOCOL_ERROR` | Protocol error |
| Protocol error | `TYPE_MISMATCH` | Type mismatch |
| Protocol error | `DISCONNECTED` | Peer disconnected |
| Unknown | `UNKNOWN_ERROR` | Unknown error (fallback) |

```cpp
enum class StatusCode : uint8_t {
    SUCCESS = 0,
    INVALID_ARG, NOT_FOUND, NO_SERVICE, NO_METHOD,
    ACCESS_DENIED, NAME_EXISTS,
    NOT_CONNECTED, CONN_RESET, BUSY,
    TIMEOUT, NO_MEMORY, NO_REPLY, IO_ERROR,
    MSG_TOO_LONG, LIMIT_EXCEEDED,
    PROTOCOL_ERROR, TYPE_MISMATCH, DISCONNECTED,
    UNKNOWN_ERROR
};
```

## Function: `statusMessage()`

```cpp
constexpr const char* statusMessage(StatusCode aCode);
```

Converts a status code into a human-readable description. Unknown/out-of-range codes return `"Unknown"`.

## Class: `Status`

```cpp
class Status {
public:
    Status() = default;                                  // success by default
    Status(StatusCode aCode);                            // construct from a code

    [[nodiscard]] StatusCode code() const;               // underlying code
    [[nodiscard]] bool isSuccess() const;                // whether it succeeded
    [[nodiscard]] bool isError() const;                  // whether it failed
    [[nodiscard]] std::string message() const;           // readable description
};
```

### Members

| Method | Description |
| --- | --- |
| `code()` | Returns the underlying `StatusCode` |
| `isSuccess()` | Returns `mCode == StatusCode::SUCCESS` |
| `isError()` | Returns `mCode != StatusCode::SUCCESS` |
| `message()` | Returns the result of `statusMessage(mCode)` |

## Per-API Examples

```cpp
#include <dbusxx/Status.hpp>
#include <iostream>

using namespace Dbusxx;

// (1) Status() — success by default
Status ok;
std::cout << ok.isSuccess();                      // true

// (2) Status(StatusCode) — construct from a status code
Status err(StatusCode::TIMEOUT);
std::cout << err.isError();                       // true

// (3) code() — get the underlying status code
StatusCode c = err.code();
std::cout << (c == StatusCode::TIMEOUT);          // true

// (4) isSuccess() — whether it succeeded
if (ok.isSuccess()) {
    // success branch
}

// (5) isError() — whether it failed
if (err.isError()) {
    // error branch
}

// (6) message() — readable description
std::cout << err.message();                       // "Operation timed out"

// (7) statusMessage(StatusCode) — standalone function for readable text
std::cout << statusMessage(StatusCode::NO_SERVICE);   // "Service not found"
std::cout << statusMessage(StatusCode::SUCCESS);      // "Success"
```

## Notes

- `Status` is successful by default, handy as an initial return value.
- Use `isSuccess()` / `isError()` to inspect results, not implicit boolean conversion.
- `statusMessage()` is `constexpr` and usable at compile time.
