# Reply — Typed Reply

> Header: `library/include/Reply.hpp` · public namespace: `Dbusxx`

## Overview

`Reply<Ret>` wraps the reply message of a remote method call and parses out a return value of type `Ret`. Check `isError()` (or `status()`) before reading `value()`; on failure `value()` returns a default-constructed `Ret`.

`Reply<Ret>` derives from `Message`, so it also has all of `Message`'s read/write capabilities.

## Template class: `Reply<Ret>`

```cpp
template<typename Ret>
class Reply : public Message {
    static_assert(isValidArg<Ret>(), "Unsupported value type");
public:
    Reply() = default;
    explicit Reply(std::shared_ptr<Private::MessagePrivate> aImpl);
    explicit Reply(Private::MessagePrivate&& aImpl);

    Reply(const Reply&) = default;
    Reply(Reply&&) noexcept = default;
    Reply& operator=(const Reply&) = default;
    Reply& operator=(Reply&&) = default;

    [[nodiscard]] Ret value() const;
    [[nodiscard]] Status status() const;
    [[nodiscard]] bool isError() const;
    [[nodiscard]] std::string errorMessage() const;
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
| `status()` | Overall status of the call; underlying message errors take precedence |
| `isError()` | True if either the payload parse failed or the underlying message is an error |
| `errorMessage()` | Error description (underlying message error first, otherwise the payload parse error) |

## Specialization: `Reply<void>`

```cpp
template<>
class Reply<void> : public Message {
public:
    using Message::Message;
};
```

This specialization is used for void-returning calls; it only inherits `Message`'s capabilities and has no `value()`.

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

// (5) status() — overall status (underlying message error takes precedence)
Status st = r.status();
std::cout << st.message();
```

## Notes

- Always call `isError()` before reading `value()`; on failure `value()` is a default value.
- Both `status()` and `isError()` consider the payload parse status and the underlying message status.
- `Reply` is copyable/movable, handy for storing in containers or passing between callbacks.
