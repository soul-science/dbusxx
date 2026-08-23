# Status 状态与错误处理

> 对应头文件：`library/include/Status.hpp`，公开命名空间：`Dbusxx`

## 简介

`Status.hpp` 定义库内统一的错误码与状态封装：

- `StatusCode` —— 枚举所有可能的结果码（成功 / 各类错误），按类别分组；
- `Status` —— 轻量包装 `StatusCode`，提供 `isSuccess()` / `isError()` 等查询接口；
- `statusMessage()` —— 将状态码转为人类可读的字符串。

库中几乎所有操作（方法调用、信号订阅、属性读写、注册等）都以 `Status` 作为返回值或错误携带者。建议始终用 `isSuccess()` / `isError()` 判断结果，而不是依赖隐式布尔转换。

## 枚举：`StatusCode`

取值按类别分组（底层为 `uint8_t`）：

| 类别 | 值 | 含义 |
| --- | --- | --- |
| 成功 | `SUCCESS` | 操作成功 |
| 调用方错误 | `INVALID_ARG` | 参数非法 |
| 调用方错误 | `NOT_FOUND` | 服务/对象/接口未找到 |
| 调用方错误 | `NO_SERVICE` | 服务未找到 |
| 调用方错误 | `NO_METHOD` | 方法未找到（可能是路径/接口/方法错误） |
| 调用方错误 | `ACCESS_DENIED` | 权限不足 |
| 调用方错误 | `NAME_EXISTS` | 总线名已被占用 |
| 连接错误 | `NOT_CONNECTED` | 未连接到总线 |
| 连接错误 | `CONN_RESET` | 连接被重置 |
| 连接错误 | `BUSY` | 资源忙 |
| 传输错误 | `TIMEOUT` | 调用超时 |
| 传输错误 | `NO_MEMORY` | 内存不足 |
| 传输错误 | `NO_REPLY` | 未收到回复 |
| 传输错误 | `IO_ERROR` | I/O 错误 |
| 传输错误 | `MSG_TOO_LONG` | 消息过长 |
| 传输错误 | `LIMIT_EXCEEDED` | 超出限制 |
| 协议错误 | `PROTOCOL_ERROR` | 协议错误 |
| 协议错误 | `TYPE_MISMATCH` | 类型不匹配 |
| 协议错误 | `DISCONNECTED` | 对端已断开 |
| 未知 | `UNKNOWN_ERROR` | 未知错误（兜底） |

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

## 函数：`statusMessage()`

```cpp
constexpr const char* statusMessage(StatusCode aCode);
```

把状态码转换为人类可读的描述字符串。未知/越界码返回 `"Unknown"`。

## 类：`Status`

```cpp
class Status {
public:
    Status() = default;                                  // 默认成功
    Status(StatusCode aCode);                            // 由码构造

    [[nodiscard]] StatusCode code() const;               // 底层码
    [[nodiscard]] bool isSuccess() const;                // 是否成功
    [[nodiscard]] bool isError() const;                  // 是否失败
    [[nodiscard]] std::string message() const;           // 可读描述
};
```

### 成员说明

| 方法 | 说明 |
| --- | --- |
| `code()` | 返回底层 `StatusCode` |
| `isSuccess()` | 返回 `mCode == StatusCode::SUCCESS` |
| `isError()` | 返回 `mCode != StatusCode::SUCCESS` |
| `message()` | 返回 `statusMessage(mCode)` 的结果 |

## API 示例（逐项）

```cpp
#include <dbusxx/Status.hpp>
#include <iostream>

using namespace Dbusxx;

// (1) Status() —— 默认构造即成功
Status ok;
std::cout << ok.isSuccess();                      // true

// (2) Status(StatusCode) —— 由状态码构造
Status err(StatusCode::TIMEOUT);
std::cout << err.isError();                       // true

// (3) code() —— 取回底层状态码
StatusCode c = err.code();
std::cout << (c == StatusCode::TIMEOUT);          // true

// (4) isSuccess() —— 是否成功
if (ok.isSuccess()) {
    // 成功分支
}

// (5) isError() —— 是否失败
if (err.isError()) {
    // 失败分支
}

// (6) message() —— 转可读描述
std::cout << err.message();                       // "Operation timed out"

// (7) statusMessage(StatusCode) —— 独立函数转可读描述
std::cout << statusMessage(StatusCode::NO_SERVICE);   // "Service not found"
std::cout << statusMessage(StatusCode::SUCCESS);      // "Success"
```

## 注意事项

- `Status` 默认构造为成功，便于用作返回值初始值。
- 判断结果请使用 `isSuccess()` / `isError()`，不要用隐式布尔转换。
- `statusMessage()` 是 `constexpr`，可在编译期使用。
