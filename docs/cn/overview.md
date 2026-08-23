# dbusxx 文档总览

> 本目录（`docs/cn`）包含 dbusxx 库的完整 API 参考（中文版），英文版见 [docs/en](../en/overview.en.md)。

## 库简介

dbusxx 是一个基于 systemd sd-bus 的 C++17 D-Bus 库，支持方法、信号、属性的**类型安全**注册与调用，覆盖系统总线、用户总线与点对点连接。所有底层细节（D-Bus 签名、消息、句柄、事件循环）都已封装，你只需写业务代码。

## 文档导航

按「核心 → 支撑」组织：

| 文档 | 对应头文件 | 内容 |
| --- | --- | --- |
| [Session.md](Session.md) | `Session.hpp` | **核心**：会话（连接），注册方法/信号/属性、同步/异步调用、本地与远端属性 |
| [Server.md](Server.md) | `Server.hpp` | **核心**：服务端封装（CRTP + 反射宏），开箱即用的服务端 |
| [Client.md](Client.md) | `Client.hpp` | **核心**：远端服务代理（自管 / 外部事件循环两种模式） |
| [Looper.md](Looper.md) | `Looper.hpp` | 事件循环（sd-event），派发消息、跨线程投递任务 |
| [MetaObject.md](MetaObject.md) | `MetaObject.hpp` | 反射元对象与 `DBUSXX_*` 注解宏 |
| [Message.md](Message.md) | `Message.hpp` | 类型安全的 D-Bus 消息（流式读写） |
| [Reply.md](Reply.md) | `Reply.hpp` | 类型化回复（方法返回值） |
| [PendingReply.md](PendingReply.md) | `PendingReply.hpp` | 异步调用句柄 |
| [Status.md](Status.md) | `Status.hpp` | 状态码与错误处理 |
| [Utils.md](Utils.md) | `Utils.hpp` | 通用类型（`SessionType`） |

## 按使用场景快速定位

- **暴露服务**：看 [Server.md](Server.md) + [MetaObject.md](MetaObject.md)（宏标注接口）
- **调用远端**：看 [Client.md](Client.md)（高层代理）或 [Session.md](Session.md)（底层直接调用）
- **收发信号 / 读写属性**：看 [Session.md](Session.md) / [Client.md](Client.md)
- **事件循环**：看 [Looper.md](Looper.md)
- **返回值与错误**：看 [Reply.md](Reply.md)、[PendingReply.md](PendingReply.md)、[Status.md](Status.md)

## 建议阅读顺序

1. [Status.md](Status.md) —— 先理解统一的错误处理
2. [Session.md](Session.md) —— 核心概念（单线程模型、连接、注册与调用）
3. [Server.md](Server.md) / [Client.md](Client.md) —— 最常见的服务端 / 客户端用法
4. 按需查阅其余支撑文档（消息、回复、事件循环、宏等）
