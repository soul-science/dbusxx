# Reply — Typed Reply

> Header: `library/include/Reply.hpp` · public namespace: `Dbusxx`

## Overview

`Reply<Ret>` wraps the reply message of a remote method call and parses out a return value of type `Ret`. Check `isError()` (or `status()`) before reading `value()`; on failure `value()` returns a default-constructed `Ret`.

Internally it holds a private `Message` member, used only to parse the payload.

## Template class: `Reply<Ret>`

```cpp
template<typename Ret>
class Reply {
    static_assert(isValidArg<Ret>(), "Unsupported value type");
public:
    Reply() = default;
    explicit Reply(std::shared_ptr<Private::MessagePrivate> aImpl);
    explicit Reply(Private::MessagePrivate&& aImpl);

    Reply(const Reply&) = default;
    Reply(Reply&&) noexcept = default;
    Reply& operator=(const Reply&) = default;
    Reply& operator=(Reply&&) = default;

    [[nodiscard]] const Ret& value() const;
    [[nodiscard]] std::string getSender() const;
    [[nodiscard]] Status status() const;
    [[nodiscard]] bool isError() const;
    [[nodiscard]] std::string errorMessage() const;

private:
    Message mMessage;   // used only to parse the payload
    Ret mValue {};
    Status mStatus { StatusCode::SUCCESS };
};
```

### Template parameters

| Parameter | Description |
| --- | --- |
| `Ret` | The return-value type; it must satisfy the library's compile-time `isValidArg<Ret>()` check. `void` has a dedicated specialization |

### Constructors

| Constructor | Description |
| --- | --- |
| `Reply()` | Constructs an empty reply |
| `Reply(std::shared_ptr<Private::MessagePrivate>)` | Wraps a shared implementation and parses the payload; `read(mValue)` runs during construction |
| `Reply(Private::MessagePrivate&&)` | Move-constructs the implementation and parses the payload |

### Members

| Method | Description |
| --- | --- |
| `value()` | Returns the parsed return value (only valid when `isError()` is false) |
| `getSender()` | Unique name of the message sender (taken from the internal `Message` member; empty if unknown) |
| `status()` | Overall status of the call; underlying message errors take precedence |
| `isError()` | True if either the payload parse failed or the underlying message is an error |
| `errorMessage()` | Error description (underlying message error first, otherwise the payload parse error) |

## Specialization: `Reply<void>`

```cpp
template<>
class Reply<void> : private Message {
public:
    using Message::Message;

    using Message::status;
    using Message::isError;
    using Message::errorMessage;
    using Message::getSender;
};
```

This specialization is used for void-returning calls: it has no `value()`, and the status accessors (`status()` / `isError()` / `errorMessage()` / `getSender()`) reuse `Message`'s implementations directly — a `void` call has no payload to parse.

## Per-API Examples

```cpp
#include <dbusxx/Reply.hpp>
#include <dbusxx/Session.hpp>
#include <iostream>

using namespace Dbusxx;

Session sess = Session::userSession();

// (1) Reply() — construct an empty reply (no backing message; not normally used directly)
Reply<int32_t> empty;

// A synchronous call returns a real Reply (constructed and parsed internally)
auto r = sess.callSync<int32_t>(
    "com.example.Calc", "/com/example/calc", "com.example.Calc", "add",
    20, 22);

// (2) value() — parsed return value (valid only when isError()==false)
std::cout << r.value();                       // 42

// (3) isError() — true if the payload parse or the underlying message errored
if (r.isError()) {
    // (4) errorMessage() — error description
    std::cerr << r.errorMessage() << std::endl;
}

// (5) getSender() — sender unique name (forwarded from Message)
std::cout << r.getSender();

// (6) status() — overall status (underlying message error takes precedence)
Status st = r.status();
std::cout << st.message();
```

## Notes

- Always call `isError()` before reading `value()`; on failure `value()` is a default value.
- Both `status()` and `isError()` consider the payload parse status and the underlying message status.
- `Reply` is copyable/movable, handy for storing in containers or passing between callbacks.
- `Message`'s read/write surface is not exposed on `Reply`: `Reply<Ret>` parses only the **first** return value; appending arguments or reading multiple values is not supported.
- A `Reply` cannot be used where a `Message` is expected: `Reply<Ret>` composes one and `Reply<void>` inherits it privately.
- An empty reply (default-constructed, or a timeout before the reply arrived) counts as an error: `status()` is `UNKNOWN_ERROR` and `isError()` is true.
