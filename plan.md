# ssdbus 项目计划

> 最后更新：2026-06-26

---

## 一、项目完成度评估

### ✅ 已完成（核心功能可用）

| 模块 | 文件 | 状态 |
|---|---|---|
| 错误体系 | `Status.hpp` + `StatusCode` | ✅ 完整，分层干净 |
| Adaptor 层 | `RawAdaptor.hpp` | ✅ int→Status 转换全覆盖，仍需清理异常 |
| 消息读写 | `Message.hpp` / `MessagePrivate.hpp` | ✅ read/write 模板完整 |
| 返回值封装 | `Reply.hpp` / `PendingReply.hpp` | ✅ sync/async 结果统一 |
| 客户端调用 | `Method.hpp` (callSync / callAsync) | ✅ 带参/无参、带回调/不带回调 |
| 服务端注册 | `Method.hpp` (registerMethod) | ✅ vtable 自动生成，类型安全 |
| 会话管理 | `Session.hpp` / `SessionPrivate.hpp` | ✅ 开/关/发/收/注册 |
| 事件循环 | `DbusEventLoop.hpp` | ✅ 可用，但日志太多 |
| 资源管理 | `DbusSlot.hpp` / 各 SharePtr | ✅ RAII，引用计数正确 |
| 信号监听 | `SignalHandler.hpp` / `Method.hpp` (listenSignal) | ✅ 类型安全，支持任意回调签名 |
| 示例完善 | `example/main.cpp` | ✅ listenSignal 在 setInfo 之前注册 |

### 🔶 遗留代码（风格不统一，可能需要整合或废弃）

| 文件 | 问题 |
|---|---|
| `DbusContext.hpp/cpp` | 老风格，直操作 `sd_bus*`/`sd_event*`，不经过 Session |
| `DbusManager.cpp` | 依赖不存在的 `DbusManager.hpp`，无法编译 |
| `DbusInterface.hpp` | 依赖不存在的 `DbusSession.hpp`，与新的 Session 体系不兼容 |
| `DbusError.hpp` | 还在 include 里，如果不再用可以移除 |

### ❌ 未实现

| 功能 | 优先级 | 说明 |
|---|---|---|
| **emitSignal（发送信号）** | **P0** | `Method.hpp` 中 `emitSignal` 已注释，仅有 `listenSignal` |
| **Property（属性读写）** | P1 | sd-bus 支持 property get/set，vtable 尚未涉及 |
| **vtable 回调错误处理** | P1 | `IMethodWrapper::call` 未使用 `RawBusErrorPtr aErr` |
| **callSync 远端错误信息** | P2 | 当前传 `nullptr`，拿不到远端错误 name/message |
| **调试日志清理** | P2 | `cout`/`cerr`/`fprintf` 散布多处 |
| **匿名函数注册** | P3 | `main.cpp` 中有 TODO 注释，`registerMethod` 不兼容自由函数/lambda |
| **CMake 集成整理** | P2 | CMakeLists.txt 未包含所有头文件，依赖管理需整理 |

---

## 二、优先级规划

```
P0 — 本周必须做
  ├─ emitSignal（发送信号）— listenSignal 已完成
  └─ 清理 DbusContext/DbusManager/DbusInterface 遗留代码
        决定：整合到新体系，还是直接删除

P1 — 下一迭代
  ├─ vtable 回调通过 aErr 返回错误
  ├─ Property get/set 支持
  └─ callSync 传入 sd_bus_error* 获取远端错误

P2 — 完善阶段
  ├─ 去除调试日志（或换成可配置的 log level）
  ├─ CMakeLists.txt 整理
  └─ 补充单元测试（Status、fromErrno、Message read/write）

P3 — 锦上添花
  ├─ 匿名函数/静态函数注册
  ├─ Introspectable 接口自动生成
  └─ 文档/示例程序
```

---

## 三、架构总览

```
┌────────────────────────────────────────────────────┐
│  业务层: Session, Method, Reply, PendingReply        │
│  只使用 Status / StatusCode，不碰 errno             │
└──────────────┬─────────────────────────────────────┘
               │ 依赖（单向）
               ▼
┌────────────────────────────────────────────────────┐
│  适配层: Adaptor (RawBus, RawMessage, RawSlot, ...) │
│  fromErrno() — 唯一的 errno → StatusCode 映射点     │
│  makeStatus() — sd-bus int ret → Status            │
│  所有 sd-bus C API 的薄封装                         │
└──────────────┬─────────────────────────────────────┘
               │ 依赖（单向）
               ▼
┌────────────────────────────────────────────────────┐
│  纯类型层: Status.hpp                              │
│  enum class StatusCode, class Status               │
│  零平台依赖，零外部宏污染                           │
└────────────────────────────────────────────────────┘
```

---

## 四、关键设计决策记录

| 决策 | 结论 |
|---|---|
| 错误处理范式 | Status 返回值，不加 operator bool，不抛异常 |
| errno 映射位置 | RawAdaptor.hpp 的 RawError 命名空间，不在 Status.hpp |
| StatusCode 语义 | 按 sd-bus 上下文语义映射，不机械翻译 POSIX 宏名 |
| 回调清理 | RAII Scope Guard + enable_shared_from_this |
| 重载决议 | Callback 用独立模板参数 `Callback&&`，不用 `std::function` |
| DbusException | 逐步消除，统一为 Status 返回值 |
| 分层原则 | 依赖方向：不稳定（平台层）→ 稳定（类型层） |
| `std::apply` + 成员函数指针 | **不可行**：`obj->*func` 不是 Callable，不能直接传给 `std::apply`；必须用 lambda 包装或用 `std::tuple_cat` 把 `obj` 也塞进 tuple |
| SignalHandler 设计 | 复用 `FuncTrait` 提取回调签名 → `MessagePrivate::read` 反序列化参数 → `std::apply` 调用回调；slot 由 handler 持有，通过 `addSignalHandler` 管理生命周期 |
| listenSignal 时序 | **必须先 `listenSignal` 再 `setInfo`**，否则 `NameAcquired` 信号在监听注册前就已发出并丢失 |
| `listenSignal` vs `NameOwnerChanged` | `NameAcquired` 仅发给获取名字的连接（unicast）；要监控其他连接的名称变化需监听 `NameOwnerChanged`（broadcast） |
