# PendingReply 异步调用句柄

> 对应头文件：`library/include/PendingReply.hpp`，公开命名空间：`Dbusxx`

## 简介

`PendingReply<Ret>` 持有一次在途异步远端调用（`callAsync`）的状态。可以通过两种方式获取结果：

1. 用 `setCallback()` 安装完成回调；
2. 用 `wait()` 阻塞等待，然后通过 `reply()` 读取结果。

它内部使用 `std::shared_future` + `std::promise` 实现同步/异步的统一交付。

## 模板类：`PendingReply<Ret>`

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

### 模板参数

| 参数 | 说明 |
| --- | --- |
| `Ret` | 返回值的类型，必须满足 `isValidArg<Ret>()`。`void` 有专门特化 |

### 构造函数

| 构造方式 | 说明 |
| --- | --- |
| `PendingReply()` | 构造空（非法）句柄 |
| `PendingReply(std::shared_ptr<Private::ReplyAsyncHandler>)` | 由异步回复处理器构造；处理器立即被消费，若回复已到达则同步交付 |

普通用户一般不需要直接构造 `PendingReply`，它是 `Session::callAsync` / `Client::callAsync` 的返回值。

### 成员方法

| 方法 | 说明 |
| --- | --- |
| `setCallback(std::function<void(Reply<Ret>)>)` | 安装完成回调；重复安装时后者覆盖前者 |
| `wait()` | 阻塞直到回复到达 |
| `reply()` | 返回 `wait()` 获取到的回复 |
| `isError()` | 调用是否失败 |
| `errorMessage()` | 失败时的错误描述 |
| `getStatus()` | 当前调用状态（空句柄返回 `INVALID_ARG`） |

## 特化：`PendingReply<void>`

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

无返回值调用使用该特化，回调签名为 `void(Reply<void>)`。

## API 示例（逐项）

```cpp
#include <dbusxx/PendingReply.hpp>
#include <dbusxx/Session.hpp>
#include <iostream>

using namespace Dbusxx;

Session sess = Session::userSession();

// (1) PendingReply() —— 构造空句柄（非法；getStatus() 返回 INVALID_ARG）
PendingReply<int32_t> empty;

// 异步调用返回真实句柄（构造函数由库内部调用）
auto pend = sess.callAsync<int32_t>(
    "com.example.Calc", "/com/example/calc", "com.example.Calc",
    "add", 1, 2);

// (2) getStatus() —— 当前调用状态
Status st = pend.getStatus();
std::cout << st.message();

// (3) setCallback(...) —— 安装完成回调（重复安装时后者覆盖前者）
pend.setCallback([](Reply<int32_t> r) {
    std::cout << "async result = " << r.value() << std::endl;
});

// (4) wait() —— 阻塞直到回复到达
pend.wait();

// (5) reply() —— 取回 wait() 得到的回复
auto rep = pend.reply();
std::cout << rep.value();                     // 3

// (6) isError() —— 调用是否失败
if (pend.isError()) {
    // (7) errorMessage() —— 失败描述
    std::cerr << pend.errorMessage() << std::endl;
}
```

## 注意事项

- `setCallback` 与 `wait` 可二选一；`wait()` 会阻塞当前线程直到回复到达。
- 回调被调用时，同一个 `Reply<Ret>` 也会写入内部 `promise`，因此 `wait()` 与回调是等价的交付通道。
- 空句柄（默认构造）的 `getStatus()` 返回 `INVALID_ARG`。
