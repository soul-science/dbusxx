# Reply 类型化回复

> 对应头文件：`library/include/Reply.hpp`，公开命名空间：`Dbusxx`

## 简介

`Reply<Ret>` 包装一次远端方法调用的回复消息，并解析出类型为 `Ret` 的返回值。它在读取 `value()` 之前应先检查 `isError()`（或 `status()`）；失败时 `value()` 返回默认构造的 `Ret`。

`Reply<Ret>` 继承自 `Message`，因此也具备 `Message` 的全部读写能力。

## 模板类：`Reply<Ret>`

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
| `status()` | 返回调用的整体状态；优先返回底层消息错误 |
| `isError()` | 载荷解析错误或底层消息错误均视为错误 |
| `errorMessage()` | 返回错误描述（底层消息错误优先，否则为载荷解析错误描述） |

## 特化：`Reply<void>`

```cpp
template<>
class Reply<void> : public Message {
public:
    using Message::Message;
};
```

无返回值调用使用该特化，仅继承 `Message` 的能力，没有 `value()`。

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

// (5) status() —— 整体状态（底层消息错误优先）
Status st = r.status();
std::cout << st.message();
```

## 注意事项

- 请始终先调用 `isError()` 再读 `value()`；失败时 `value()` 为默认值。
- `status()` 与 `isError()` 会同时考虑载荷解析状态与底层消息状态。
- 复制语义可用：`Reply` 可被拷贝/移动，方便存入容器或在回调间传递。
