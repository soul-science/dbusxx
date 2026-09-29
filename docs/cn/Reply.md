# Reply 类型化回复

> 对应头文件：`library/include/Reply.hpp`，公开命名空间：`Dbusxx`

## 简介

`Reply<Ret>` 包装一次远端方法调用的回复消息，并解析出类型为 `Ret` 的返回值。它在读取 `value()` 之前应先检查 `isError()`（或 `status()`）；失败时 `value()` 返回默认构造的 `Ret`。

它内部持有一条私有 `Message` 成员，仅用于解析载荷。

## 模板类：`Reply<Ret>`

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
    Message mMessage {};   // 仅用于解析载荷
    Ret mValue {};
    Status mStatus { StatusCode::UNKNOWN_ERROR };
};
```

### 模板参数

| 参数 | 说明 |
| --- | --- |
| `Ret` | 返回值的类型，必须满足库内 `isValidArg<Ret>()` 编译期校验。`void` 有专门特化 |

### 构造函数

| 构造方式 | 说明 |
| --- | --- |
| `Reply()` | 构造空回复 |
| `Reply(std::shared_ptr<Private::MessagePrivate>)` | 包装共享实现并解析载荷，构造时即完成 `read(mValue)` |
| `Reply(Private::MessagePrivate&&)` | 移动构造实现并解析载荷 |

### 成员方法

| 方法 | 说明 |
| --- | --- |
| `value()` | 返回解析后的返回值（仅在 `isError()` 为 false 时有效） |
| `getSender()` | 消息发送者的唯一名（取自内部 `Message` 成员；未知时为空字符串） |
| `status()` | 返回调用的整体状态；优先返回底层消息错误 |
| `isError()` | 载荷解析错误或底层消息错误均视为错误 |
| `errorMessage()` | 返回错误描述（底层消息错误优先，否则为载荷解析错误描述） |

## 特化：`Reply<void>`

```cpp
template<>
class Reply<void> : private Message {
public:
    using Message::Message;
<<<<<<< HEAD

=======
<<<<<<< HEAD

=======
>>>>>>> 8400048 ([Enhancement] Improve Reply and PendingReply)
>>>>>>> cc0fd85 ([Enhancement] Improve Reply and PendingReply)
    using Message::status;
    using Message::isError;
    using Message::errorMessage;
    using Message::getSender;
};
```

无返回值调用使用该特化：没有 `value()`，状态查询（`status()` / `isError()` / `errorMessage()` / `getSender()`）直接复用 `Message` 的实现——`void` 没有载荷可解析，不需要额外包装。

## API 示例（逐项）

```cpp
#include <dbusxx/Reply.hpp>
#include <dbusxx/Session.hpp>
#include <iostream>

using namespace Dbusxx;

Session sess = Session::userSession();

// (1) Reply() —— 构造空回复（无底层消息，通常不直接使用）
Reply<int32_t> empty;

// 一次同步调用返回真实 Reply（构造函数由库内部调用并解析载荷）
auto r = sess.callSync<int32_t>(
    "com.example.Calc", "/com/example/calc", "com.example.Calc", "add",
    20, 22);

// (2) value() —— 解析后的返回值（仅在 isError()==false 时有效）
std::cout << r.value();                       // 42

// (3) isError() —— 载荷解析错误或底层消息错误均视为错误
if (r.isError()) {
    // (4) errorMessage() —— 错误描述
    std::cerr << r.errorMessage() << std::endl;
}

// (5) getSender() —— 发送者唯一名（转发自 Message）
std::cout << r.getSender();

// (6) status() —— 整体状态（底层消息错误优先）
Status st = r.status();
std::cout << st.message();
```

## 注意事项

- 请始终先调用 `isError()` 再读 `value()`；失败时 `value()` 为默认值。
- `status()` 与 `isError()` 会同时考虑载荷解析状态与底层消息状态。
- 复制语义可用：`Reply` 可被拷贝/移动，方便存入容器或在回调间传递。
- `Message` 的读写接口不对 `Reply` 暴露：`Reply<Ret>` 只解析**第一个**返回值，不支持追加参数或读取多个值。
- `Reply` 不能当作 `Message` 使用：`Reply<Ret>` 是组合、`Reply<void>` 是私有继承，两者对外都不可转换。
- 空回复（默认构造，或 `waitFor()` 超时前尚未收到回复）视为错误：`status()` 为 `UNKNOWN_ERROR`，`isError()` 为 true。
