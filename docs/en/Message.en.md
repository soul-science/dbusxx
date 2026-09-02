# Message — D-Bus Message

> Header: `library/include/Message.hpp` · public namespace: `Dbusxx`

## Overview

`Message` is a type-safe, stream-like wrapper around a raw D-Bus message. Append arguments with `operator<<` / `write()`, and extract them with `operator>>` / `read()`. It is also the base class of `Reply<Ret>`.

Internally `Message` holds the underlying `sd-bus` message handle via `Private::MessagePrivate` (PIMPL) and supports automatic (de)serialization of basic types, `std::string`, containers, `UnixFd` file descriptors, structs, etc.

## Class: `Message`

```cpp
class Message {
public:
    Message() = default;
    explicit Message(std::shared_ptr<Private::MessagePrivate> aImpl);
    explicit Message(Private::MessagePrivate&& aImpl);

    ~Message() = default;
    Message(Message&&) noexcept = default;
    Message& operator=(Message&&) noexcept = default;
    Message(const Message&) = default;
    Message& operator=(const Message&) = default;

    template<typename T> Message& operator>>(T& aVal);
    template<typename T> Message& operator<<(const T& aVal);

    template<typename T> [[nodiscard]] Status read(T& aVal);
    template<typename First, typename... Rests> [[nodiscard]] Status read(First&, Rests&...);
    template<typename... Args> [[nodiscard]] Status read(std::tuple<Args...>& aVals);

    template<typename T> [[nodiscard]] Status write(const T& aVal);
    template<typename First, typename... Rests> [[nodiscard]] Status write(const First&, const Rests&...);

    [[nodiscard]] std::string getSender() const;
    [[nodiscard]] bool isError() const;
    [[nodiscard]] Status status() const;
    [[nodiscard]] std::string errorMessage() const;
};
```

### Constructors

| Constructor | Description |
| --- | --- |
| `Message()` | Constructs an empty (invalid) message |
| `Message(std::shared_ptr<Private::MessagePrivate>)` | Wraps an existing shared implementation |
| `Message(Private::MessagePrivate&&)` | Move-constructs from an implementation |

Ordinary users usually don't construct `Message` directly; it is used internally by `Reply<Ret>` or returned by sessions/clients.

### Writing arguments

| Method | Description |
| --- | --- |
| `write(const T&)` | Appends a single value to the payload (when `T` is a `std::tuple`, elements are written one by one) |
| `write(const First&, const Rests&...)` | Appends multiple values of possibly different types |
| `operator<<(const T&)` | Stream-style append of a single value; returns `*this` for chaining |

### Reading arguments

| Method | Description |
| --- | --- |
| `read(T&)` | Reads a single value from the payload into `aVal` |
| `read(First&, Rests&...)` | Reads multiple values of possibly different types |
| `read(std::tuple<Args...>&)` | Reads multiple values into a `std::tuple` |
| `operator>>(T&)` | Stream-style read of a single value; returns `*this` for chaining |

### Status & metadata

| Method | Description |
| --- | --- |
| `getSender()` | Unique name of the message sender (empty string if unknown) |
| `isError()` | Whether the message is an error reply |
| `status()` | Transport/parse status of the message |
| `errorMessage()` | Error description when the message is an error |

## Per-API Examples

> `Message` is usually constructed internally by the library (building requests / parsing replies); ordinary code generally reaches it indirectly through `Reply<Ret>` (its base class). The example below uses a `Reply` returned by a remote call.

```cpp
#include <dbusxx/Message.hpp>
#include <dbusxx/Reply.hpp>
#include <dbusxx/Session.hpp>

using namespace Dbusxx;

Session sess = Session::userSession();   // establish a session first

// (1) Message() — construct an empty message; it has no backing
//     implementation. Real instances come from the library or Reply.
Message empty;

// Get a real message from a synchronous call (Reply derives from Message)
auto reply = sess.callSync<int32_t>(
    "com.example.Svc", "/com/example", "com.example.Iface", "method", 1);
Message& msg = reply;                    // base-class reference

// (2) operator>> — stream-style read of a single value
int32_t n = 0;
msg >> n;

// (3) read(T&) — read a single value
Status st1 = msg.read(n);

// (4) read(First&, Rests&...) — read multiple values (depends on payload)
std::string s;
Status st2 = msg.read(n, s);

// (5) read(std::tuple<Args...>&) — read into a tuple
std::tuple<int32_t, std::string> t;
Status st3 = msg.read(t);

// (6) operator<< — stream-style write of a single value (used internally
//     by the library when building requests)
msg << 42;

// (7) write(const T&) — write a single value
Status st4 = msg.write(42);

// (8) write(const First&, const Rests&...) — write multiple values
Status st5 = msg.write(1, std::string("a"));

// (9) getSender() — sender unique name (empty if unknown)
std::string sender = msg.getSender();

// (10) isError() — whether the message is an error
bool isErr = msg.isError();

// (11) status() — transport/parse status
Status st6 = msg.status();

// (12) errorMessage() — error description
std::string em = msg.errorMessage();
```

## Supported payload types

The template `read`/`write` on `MessagePrivate` support the following types (i.e. the legal argument types accepted by the library's `isValidArg`):

- Basic types: integers (8/16/32/64-bit, signed and unsigned), `float`/`double`, `bool`, `char*`, `const char*`
- Strings: `std::string`, `std::string_view`
- Containers: `std::vector<T>`, `std::array<T, N>`, `std::map<K, V>`, `std::unordered_map<K, V>`
- Composite: `std::tuple<...>`, any custom aggregate struct (fields are expanded automatically)

## Notes

- Check `status()` / `isError()` before reading; on failure some values may not be filled.
- `std::string_view` is copied into a `std::string` before writing so that `c_str()` is NUL-terminated.
- `float` is (de)serialized as `double` to keep the byte count consistent between reads and writes.
