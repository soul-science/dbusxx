# Message 消息

> 对应头文件：`library/include/Message.hpp`，公开命名空间：`Dbusxx`

## 简介

`Message` 是对一条原始 D-Bus 消息的类型安全、流式（stream-like）包装：用 `operator<<` / `write()` 追加参数，用 `operator>>` / `read()` 提取参数。它同时也是**为精细操作预留的入口**——需要手工构造消息或逐项读写载荷时用它；构造路径尚未开放，实例目前只由库内部产生。

`Message` 内部通过 `Private::MessagePrivate`（PIMPL）持有底层 `sd-bus` 消息句柄，支持基本类型、`std::string`、容器、`UnixFd` 文件描述符、结构体等的自动编解码。

## 类：`Message`

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

### 构造函数

| 构造方式 | 说明 |
| --- | --- |
| `Message()` | 构造空（非法）消息 |
| `Message(std::shared_ptr<Private::MessagePrivate>)` | 包装已有的共享实现 |
| `Message(Private::MessagePrivate&&)` | 移动构造实现 |

`Message` 还没有公开的构造路径，实例目前只由库内部产生；它的定位是为精细操作预留：将来开放后即可手工构造消息、逐项读写载荷。`Reply<Ret>` 内部持有它、`Reply<void>` 私有继承它，但都不暴露读写接口。

### 写入参数

| 方法 | 说明 |
| --- | --- |
| `write(const T&)` | 追加单个值到消息载荷（`T` 为 `std::tuple` 时会展开为元素逐个写入） |
| `write(const First&, const Rests&...)` | 追加多个不同类型的值 |
| `operator<<(const T&)` | 流式追加单个值，返回 `*this` 以便链式调用 |

### 读取参数

| 方法 | 说明 |
| --- | --- |
| `read(T&)` | 从载荷读取单个值到 `aVal` |
| `read(First&, Rests&...)` | 从载荷读取多个不同类型的值 |
| `read(std::tuple<Args...>&)` | 将多个值读入一个 `std::tuple` |
| `operator>>(T&)` | 流式读取单个值，返回 `*this` 以便链式调用 |

### 状态与元信息

| 方法 | 说明 |
| --- | --- |
| `getSender()` | 消息发送者的唯一名（未知时为空字符串） |
| `isError()` | 消息是否表示错误回复 |
| `status()` | 消息的传输/解析状态 |
| `errorMessage()` | 若为错误，返回错误描述 |

## API 示例（逐项）

> `Message` 的构造路径尚未开放，实例目前只由库内部产生（`Method` / `PropertyHandler` /
> `SignalHandler` 等）；公开 API 返回的是 `Reply<Ret>` / `PendingReply<Ret>`。下面的片段
> 只用于说明各接口的形态与返回类型。

```cpp
#include <dbusxx/Message.hpp>
#include <tuple>

using namespace Dbusxx;

// (1) Message() —— 构造空消息：没有底层实现，因此下面每个 read/write 都会
//     返回 StatusCode::UNKNOWN_ERROR。精细操作入口开放后，把真实消息实例
//     换到这里即可。
Message msg;

// (2) operator>> —— 流式读取单个值
int32_t n = 0;
msg >> n;

// (3) read(T&) —— 读取单个值
Status st1 = msg.read(n);

// (4) read(First&, Rests&...) —— 读取多个不同类型的值（取决于实际载荷）
std::string s;
Status st2 = msg.read(n, s);

// (5) read(std::tuple<Args...>&) —— 读取到 tuple
std::tuple<int32_t, std::string> t;
Status st3 = msg.read(t);

// (6) operator<< —— 流式写入单个值（库内部构造请求时使用）
msg << 42;

// (7) write(const T&) —— 写入单个值
Status st4 = msg.write(42);

// (8) write(const First&, const Rests&...) —— 写入多个值
Status st5 = msg.write(1, std::string("a"));

// (9) getSender() —— 发送者唯一名（未知时为空字符串）
std::string sender = msg.getSender();

// (10) isError() —— 消息是否为错误
bool isErr = msg.isError();

// (11) status() —— 传输/解析状态
Status st6 = msg.status();

// (12) errorMessage() —— 错误描述
std::string em = msg.errorMessage();
```

## 支持的载荷类型

通过 `MessagePrivate` 的模板 `read`/`write` 支持以下类型（即库内 `isValidArg` 允许的合法参数类型）：

- 基本类型：整数（8/16/32/64 位，含无符号）、`float`/`double`、`bool`、`char*`、`const char*`
- 字符串：`std::string`、`std::string_view`
- 容器：`std::vector<T>`、`std::array<T, N>`、`std::map<K, V>`、`std::unordered_map<K, V>`
- 复合：`std::tuple<...>`、任意自定义聚合体结构体（自动展开字段）

## 注意事项

- 读取前请先用 `status()` / `isError()` 检查是否成功；读取失败时部分值可能未被填充。
- `std::string_view` 在写入时会先拷贝为 `std::string` 再取 `c_str()`，保证结尾 `\0`。
- `float` 统一按 `double` 编解码，保证读写字节数一致。
