# Utils 通用类型

> 对应头文件：`library/include/Utils.hpp`，公开命名空间：`Dbusxx`

## 简介

`Utils.hpp` 目前定义库内最基础的枚举 `SessionType`，用于标识一个会话（连接）绑定到哪种总线。它被 `Session`、`Client`、`Server` 等核心类型广泛使用。

## 枚举：`SessionType`

```cpp
enum class SessionType : uint8_t {
    SYSTEM,     // 系统总线
    USER,       // 用户/会话总线
    PEER,       // 点对点连接（无总线守护进程）
    INVALID     // 未初始化 / 非法
};
```

| 值 | 含义 |
| --- | --- |
| `SYSTEM` | 系统总线（如 systemd、NetworkManager 等系统服务） |
| `USER` | 用户/会话总线（桌面应用的常规选择） |
| `PEER` | 点对点连接，不需要总线守护进程，通过 socket 直连 |
| `INVALID` | 未初始化或非法的占位值 |

## API 示例（逐项）

```cpp
#include <dbusxx/Utils.hpp>

using namespace Dbusxx;

// (1) SessionType::SYSTEM —— 系统总线
SessionType sys = SessionType::SYSTEM;

// (2) SessionType::USER —— 用户/会话总线
SessionType usr = SessionType::USER;

// (3) SessionType::PEER —— 点对点连接
SessionType peer = SessionType::PEER;

// (4) SessionType::INVALID —— 未初始化占位值
SessionType inv = SessionType::INVALID;

// 比较与 switch
if (usr == SessionType::USER) { /* ... */ }
switch (sys) {
    case SessionType::SYSTEM:   break;
    case SessionType::USER:     break;
    case SessionType::PEER:     break;
    case SessionType::INVALID:  break;
}
```

## 相关链接

- `SessionType` 是 `Session::createSession()`、`Client`、`Server` 构造函数的入口参数之一，详见 [Session.md](Session.md)、[Client.md](Client.md)、[Server.md](Server.md)。
