# PendingReply — Async Call Handle

> Header: `library/include/PendingReply.hpp` · public namespace: `Dbusxx`

## Overview

`PendingReply<Ret>` holds the state of a single in-flight asynchronous remote call (`callAsync`). The result can be obtained in two ways:

1. Install a completion callback with `setCallback()`;
2. Block on `wait()` and then read the result via `reply()`.

Internally it uses `std::shared_future` + `std::promise` to deliver the result uniformly for both synchronous and asynchronous consumers.

## Template class: `PendingReply<Ret>`

```cpp
template<typename Ret>
class PendingReply {
    static_assert(isValidArg<Ret>(), "Unsupported value type");
public:
    PendingReply() = default;
    explicit PendingReply(std::shared_ptr<Private::ReplyAsyncHandler> aHandler);

    [[nodiscard]] bool isError() const;
    [[nodiscard]] std::string errorMessage() const;
    [[nodiscard]] Status getStatus() const;

    void setCallback(std::function<void(Reply<Ret>)> aCallback);

    void wait();
    [[nodiscard]] Reply<Ret> reply() const;
};
```

### Template parameters

| Parameter | Description |
| --- | --- |
| `Ret` | The return-value type; it must satisfy `isValidArg<Ret>()`. `void` has a dedicated specialization |

### Constructors

| Constructor | Description |
| --- | --- |
| `PendingReply()` | Constructs an empty (invalid) handle |
| `PendingReply(std::shared_ptr<Private::ReplyAsyncHandler>)` | Constructs from an async reply handler; the handler is consumed immediately and, if the reply already arrived, it is delivered synchronously |

Ordinary users usually don't construct `PendingReply` directly; it is the return value of `Session::callAsync` / `Client::callAsync`.

### Members

| Method | Description |
| --- | --- |
| `setCallback(std::function<void(Reply<Ret>)>)` | Installs a completion callback; re-installing replaces the previous one |
| `wait()` | Blocks until the reply arrives |
| `reply()` | Returns the reply obtained by `wait()` |
| `isError()` | Whether the call failed |
| `errorMessage()` | Error description when the call failed |
| `getStatus()` | Current call status (`INVALID_ARG` for an empty handle) |

## Specialization: `PendingReply<void>`

```cpp
template<>
class PendingReply<void> {
public:
    PendingReply() = default;
    explicit PendingReply(std::shared_ptr<Private::ReplyAsyncHandler> aHandler);

    [[nodiscard]] bool isError() const;
    [[nodiscard]] std::string errorMessage() const;
    [[nodiscard]] Status getStatus() const;

    void setCallback(std::function<void(Reply<void>)> aCallback);
    void wait();
    [[nodiscard]] Reply<void> reply() const;
};
```

This specialization is used for void-returning calls; the callback signature is `void(Reply<void>)`.

## Per-API Examples

```cpp
#include <dbusxx/PendingReply.hpp>
#include <dbusxx/Session.hpp>
#include <iostream>

using namespace Dbusxx;

Session sess = Session::userSession();

// (1) PendingReply() — construct an empty handle (invalid; getStatus() returns INVALID_ARG)
PendingReply<int32_t> empty;

// An async call returns a real handle (constructed internally by the library)
auto pend = sess.callAsync<int32_t>(
    "com.example.Calc", "/com/example/calc", "com.example.Calc",
    "add", 1, 2);

// (2) getStatus() — current call status
Status st = pend.getStatus();
std::cout << st.message();

// (3) setCallback(...) — install a completion callback (the latest one wins)
pend.setCallback([](Reply<int32_t> r) {
    std::cout << "async result = " << r.value() << std::endl;
});

// (4) wait() — block until the reply arrives
pend.wait();

// (5) reply() — retrieve the reply obtained by wait()
auto rep = pend.reply();
std::cout << rep.value();                     // 3

// (6) isError() — whether the call failed
if (pend.isError()) {
    // (7) errorMessage() — failure description
    std::cerr << pend.errorMessage() << std::endl;
}
```

## Notes

- `setCallback` and `wait` are two alternatives; `wait()` blocks the current thread until the reply arrives.
- When the callback fires, the same `Reply<Ret>` is also written into the internal `promise`, so `wait()` and the callback are equivalent delivery channels.
- The `getStatus()` of an empty (default-constructed) handle returns `INVALID_ARG`.
