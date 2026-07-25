# ssdbus 项目计划

> 最后更新：2026-07-25（跨线程 emit 支持 + example_server 完整测试用例）

---

## 一、项目完成度评估（约 90% 核心可用）

### ✅ 已完成

| 模块 | 文件 | 状态 |
|---|---|---|
| 错误体系 | `Status.hpp` + `StatusCode` (18 枚举) | ✅ 完整，分层干净 |
| Adaptor 层 | `RawCommon.hpp` | ✅ int→Status 全覆盖，仍有风格问题 |
| 消息读写 | `Message.hpp` / `MessagePrivate.hpp` | ✅ 基本类型 read/write 模板完整 |
| 返回值封装 | `Reply.hpp` / `PendingReply.hpp` | ✅ sync/async 统一 |
| 客户端调用 | `Method.hpp` (callSync ×4 / callAsync ×2) | ✅ 带参/无参、带/不带回调 |
| 服务端注册 | `Method.hpp` (registerMethod / registerBuilder) | ✅ vtable 自动生成，支持链式批量注册 |
| 信号发送 | `Method.hpp` (emitSignal) | ✅ 带参/无参 |
| 信号注册 | `Method.hpp` / `RegisterBuilder` (registerSignal / addSignal) | ✅ vtable 注册 + 链式，introspect 可见 |
| VTable 管理 | `VTableRegistrar.hpp` / `RegisterBuilder` | ✅ 链式 API + commit 一次提交完整 vtable。VTableRegistrar 与 SessionPrivate 解耦（传 RawBusSharePtr），commit 只负责 sd-bus 交互 |
| 信号监听 | `Method.hpp` / `SignalHandler.hpp` | ✅ 类型安全回调 |
| 会话管理 | `Session.hpp` / `SessionPrivate.hpp` | ✅ 开/关/发/收/注册 |
| 事件循环 | `Looper.hpp` / `LooperPrivate.hpp` | ✅ sd-event 集成，IO 驱动 + 优雅退出 + **跨线程 `post()` 投递（eventfd + 任务队列 + mutex）+ `isOwnerThread()`** |
| 资源管理 | `RawSlotSharePtr` / SharePtr 系列 | ✅ RAII，移动语义完整 |
| 类型推导 | `FunctionTrait.hpp` / `DbusArgs.hpp` | ✅ 编译期萃取 |
| 属性注册 | `Method.hpp` / `Session.hpp` / `VTableRegistrar.hpp` | ✅ 自拥有值 + `getProperty`/`setProperty` + `onPropertyChanged` callback + `sd_bus_emit_properties_changed` + `typeid` 类型安全检查 |
| 示例 | `example/*.cpp` | ✅ 覆盖主要 API：Session 直连、MetaObject 反射、registerMethod 多类型、服务端；**`example_server.cpp` 18 项完整测试（sync/async/同线程 emit/跨线程 emit/信号监听/双 Session 隔离）** |
| 反射注册 | `MetaObject.hpp` | ✅ CRTP + 宏标注 → `registerObject` 一键注册：`SSDBUS_METHOD` / `SSDBUS_SIGNAL` / `SSDBUS_LISTEN` |
| 服务端封装 | `Server.hpp` | ✅ `Server<Derived>` 捆绑 Session + Looper + registerObject，`run()` 一行启动；**`emit()` 自动线程检测（同线程直调 / 跨线程 post）** |
| 遗留代码清理 | DbusContext/DbusManager/DbusInterface/DbusError | ✅ 全部删除 |

### 🔴 已知 Bug（按严重度）

| # | 严重度 | 文件 | 问题 |
|---|--------|------|------|
| B1 | 已关闭 | `RawCommon.hpp` | `sd_bus_message_copy` 签名 `(msg*, msg*, int)` 正确——假阳性 |
| B2 | ✅ 已修复 | `DbusArgs.hpp` | `int8_t` 映射为 `'y'` |
| B3 | ✅ 已修复 | `SessionPrivate.hpp` | `isInvalidInfo` → `isValidInfo` |
| B4 | ✅ 已修复 | `RawCommon.hpp` | `wait/process` 的 `assert(aBus)` → `if (!aBus) return -1` |
| B5 | ✅ 已修复 | `RawBusSharePtr.hpp` | 拷贝构造/赋值已加 `mIsOwned` 检查 |
| B6 | ✅ 已修复 | `RawMessageSharePtr.hpp` | 拷贝构造/赋值已加 `mIsOwned` 检查 |
| B7 | ✅ 已修复 | `MessagePrivate.hpp` | `write(string_view)` 先转 `std::string` 再 `.c_str()` |
| B8 | 设计 | `Reply.hpp` | `value()` 读失败返回默认值——调用者应先调 `isError()` 判断 |
| B9 | ✅ 已修复 | `PendingReply.hpp` | `setCallback` 改用 `shared_ptr<Reply<Ret>>`，move 安全 |
| B10 | ✅ 已修复 | `DbusArgs.hpp` / `MessagePrivate.hpp` | `float` → `double` 读写不对称 —— `write` 中显式 `static_cast<double>` |
| B11 | ✅ 已修复 | `Session.hpp` | `callAsync` 改用 `shared_ptr<vector>` 捕获，拷贝安全 |
| B12 | 🟡 风格 | `RawCommon.hpp` | `DbusException` 抛出 vs `Status` 返回混用 |
| B13 | ✅ 已修复 | `Utils.hpp` | `__safeRead`/`__safeWrite` 死码已清理，`ServiceInfo` 保留；函数迁移至 `LooperPrivate.hpp` 作私有方法 (2026-07-21) |
| B14 | ✅ 已修复 | `EventLoop.hpp` | 旧文件已删除，日志噪音已清理，由 `LooperPrivate` 替代 (2026-07-21) |
| B15 | ✅ 已修复 | `example/main.cpp` | `testUint32` 打印 `"testInt32"` |
| B16 | ✅ 已修复 | `Method.hpp` | `registerMethod` 对同一 key 两次 map 查找 |
| B17 | ✅ 已修复 | `Method.hpp` | `registerSingleSignal` 提前插入空壳 + commit 也查 map → 自己挡自己；修复：VTableRegistrar 解耦 SessionPrivate，key 统一为 name (2026-07-23) |

---

## 二、未实现功能（按优先级）

### P0 — 阻断性缺陷 ✅ 已全部修复（2026-06-30）

| # | 任务 | 状态 |
|---|------|------|
| P0-1 | ~~修复 `copyMessage` 编译错误~~ | 假阳性——`sd_bus_message_copy` 签名匹配 |
| P0-2 | 修复 `int8_t` D-Bus 签名 | ✅ 改为 `'y'` |
| P0-3 | 修复 `isInvalidInfo` 逻辑 | ✅ 重命名为 `isValidInfo` |
| P0-4 | `assert` → 错误返回 | ✅ `wait/process` 改为 `if (!aBus) return -1` |
| P0-5 | 修复 SharePtr ref-count | ✅ `RawBusSharePtr` + `RawMessageSharePtr` 均加 `mIsOwned` 检查 |
| P0-6 | 修复 `string_view::data()` UB + `float` 读写 | ✅ `string_view`→`string`，`float`→`double` 显式转换 |
| P0-7 | 修复 `PendingReply` `[this]` 悬空 | ✅ 改用 `shared_ptr<Reply<Ret>>` 捕获 |

### P1 — 生产就绪必要功能

| # | 任务 | 说明 |
|---|------|------|
| P1-1 | ✅ 已完成 | `PropertyWrapper`: 自拥有值 → `set()` 自动 `emit_properties_changed` + `onChange` 回调；`Session`: `getProperty`/`setProperty`/`onPropertyChanged` + `getPropPrivate<T>` typeid 类型检查 (2026-07-02) |
| P1-2 | ✅ 数组类型已完成 | `vector<T>` / `vector<vector<T>>` / `array<T,N>` read/write + `getSignature` 递归展开 |
| P1-2a | ⏸️ 降级 P3 | 变体(`v`)、字典(`a{e}`) — 自有 Property 系统已覆盖标准 Properties 接口需求 |
| P1-3 | ✅ 已完成 | `callSync`/`callAsync` 远端错误 → `RawRemoteError::toStatus()`；`onCall`/`onGetter`/`onSetter` 失败时 `fromStatus()` 填充 `aErr` |
| P1-4 | ✅ 已完成（合并至 P1-3） | vtable 回调通过 `RawRemoteError::fromStatus()` 填充 `aErr` 返回对端 |
| P1-5 | ✅ 设计如此 | `Reply::value()` 读取失败返回默认值——调用者应先调 `isError()` 判断 (2026-07-20) |
| P1-6 | ✅ 已修复 | `float` 类型读写对称 —— `write` 中显式 `static_cast<double>` |
| P1-7 | 🟡 部分解决 | ~~单线程使用~~ → `Session` 仍单线程，`Server::emit()` 在服务端层面做了跨线程适配（自动检测 `isOwnerThread()`，跨线程走 `Looper::post()` 投递到事件循环执行）。调用方通过 `Server` 使用 emit 无需关心线程；直接用 `Session` 仍需自行加锁 (2026-07-25) |
| P1-8 | ✅ 已完成 | `VTableRegistrar` + `RegisterBuilder`：链式 API + 字符串生命周期托管，私有头文件不暴露 |

### P2 — 完善阶段

| # | 任务 | 说明 |
|---|------|------|
| P2-1 | **统一日志系统** | 替换 `cout/cerr/fprintf` 为可配置 log level |
| P2-2 | ✅ 已完成 | Looper/LooperPrivate 集成 sd_event，IO 事件驱动 + 优雅退出 |
| P2-3 | ✅ 已完成 | **example_server 完整测试用例**：18 项测试覆盖 sync/async/同线程 emit/跨线程 emit/信号监听/优雅关闭，双 Session 架构隔离 sync 与事件循环 fd 竞争 (2026-07-25) |
| P2-4 | ✅ Lambda 注册已完成 | `registerMethod` 支持自由函数和 lambda |
| P2-5 | ✅ 已完成 | `ReplyAsyncHandler` 持有 `RawSlotSharePtr`，RAII 自动 unref slot，无需手动 `cancel()` (2026-07-20) |
| P2-6 | **CMakeLists.txt 整理** | 移除已删除文件的引用、补充新头文件 |
| P2-7 | ✅ 已完成 | `Message::operator<<` 改为 const T&，支持右值 |
| P2-8 | ✅ 已完成 | **Looper 跨线程任务投递**：`post(std::function<void()>)` + `eventfd` 唤醒 + `std::deque` 任务队列 + `std::mutex` 保护，`isOwnerThread()` 线程检测 (2026-07-25) |

### P3 — 锦上添花

| # | 任务 | 说明 |
|---|------|------|
| P3-1 | **API 文档** | Doxygen 注释 + README 快速入门 |
| P3-2 | **自动 introspect** | 遍历注册表自动生成 XML |
| P3-3 | **连接重连** | 断线自动重连 + 重新注册 vtable |
| P3-4 | **name owner 追踪** | 封装 `sd_bus_track` 或 `NameOwnerChanged` |
| P3-5 | **peer-to-peer 连接** | `sd_bus_open()` P2P 模式 |
| P3-6 | ✅ 已完成 | `RawCommon.hpp` 拆分 → RawBus/RawMessage/RawSlot/RawEvent/RawErrorConvert (2026-07-20) |
| P3-7 | **`FuncTrait` 补全** | noexcept/volatile/引用限定符版本 |
| P3-8 | **变体类型 (`v`)** | 按需实现：`std::any` 或 tagged union 包装，含运行时签名 |
| P3-9 | **字典类型 (`a{e}`)** | 按需实现：`std::map<K, V>` ↔ `a{KV}`，依赖变体则常用 `a{sv}` |

---

## 三、当前优先级路线图

```
P0 — ✅ 全部完成（2026-06-28）

P1 — ✅ 全部完成（2026-07-09）
  ├─ ✅ VTableRegistrar 封装             ← 链式 API + 字符串托管
  ├─ ✅ Property get/set/onChanged      ← PropertyWrapper 自拥有值 + typeid 类型安全 + callback
  ├─ ✅ vector<T> 数组类型               ← read/write 递归展开 + getSignature 递归
  └─ ✅ 远端错误双向传递                 ← toStatus() / fromStatus() 映射表 + callSync/callAsync/onCall

P2 — 完善阶段
  ├─ ✅ Lambda 注册                    ← 自由函数 + lambda
  ├─ ✅ 事件循环改进                    ← Looper 集成 sd_event，IO 驱动替代忙循环
  ├─ ✅ MetaObject 反射注册             ← SSDBUS_METHOD / SSDBUS_SIGNAL / SSDBUS_LISTEN + registerObject
  ├─ ✅ Server 一站式封装               ← Server<T> = Session + Looper + registerObject
  ├─ 日志系统
  ├─ 单元测试
  └─ 预计工作量: ~2 天

P3 — 锦上添花
  └─ 视需求决定
```

---

## 四、架构总览

### 分层架构

```mermaid
graph TD
    subgraph 业务层["业务层"]
        Server["Server&lt;T&gt;<br/>Session+Looper+注册 一体"]
        Session["Session<br/>用户入口"]
        MetaObject["MetaObject&lt;T&gt;<br/>CRTP 反射基类"]
        RegisterBuilder["RegisterBuilder<br/>链式注册代理"]
        Reply["Reply / PendingReply<br/>返回值封装"]
    end

    subgraph 逻辑层["逻辑层"]
        Method["Method / MethodWrapper<br/>方法包装与分发"]
        VTableRegistrar["VTableRegistrar<br/>vtable 构建器"]
        VTableContext["VTableContext<br/>自持字符串 + vtable + slot"]
        FuncTrait["FuncTrait<br/>编译期签名萃取"]
        SignalHandler["SignalHandler<br/>信号回调类型擦除"]
    end

    subgraph 适配层["适配层"]
        Adaptor["Adaptor<br/>RawBus / RawMessage / RawSlot / RawEvent<br/>sd-bus C API 薄封装"]
    end

    subgraph 纯类型层["纯类型层"]
        Status["Status.hpp<br/>StatusCode / Status"]
    end

    业务层 -->|依赖单向| 逻辑层
    逻辑层 -->|依赖单向| 适配层
    适配层 -->|依赖单向| 纯类型层
```

### 关键组件关系

```mermaid
graph TD
    Server["Server&lt;T&gt;"] -->|mSession| Session_c["Session"]
    Server -->|mLooper| Looper_c["Looper"]
    Server -->|init| RO["registerObject(this)"]

    Session_c -->|mPrivate| SP["SessionPrivate"]
    Session_c -->|mRepsPtr| Reps["vector&lt;shared_ptr&lt;void&gt;&gt;<br/>异步回调生命周期"]
    Session_c -->|registerObject| MO["MetaObject&lt;T&gt;<br/>static registry"]

    MO -->|SSDBUS_METHOD| RM["entry: Method → addMethod"]
    MO -->|SSDBUS_SIGNAL| RS["entry: Signal → addSignal"]
    MO -->|SSDBUS_LISTEN| RL["entry: SignalListen → listenSignal 即时生效"]

    SP -->|methods| MM["MethodMap"]
    SP -->|signals| SM["SignalMap"]
    SP -->|sigHandler| SigH["SignalHandler 列表"]

    RegisterBuilder -->|session| SP
    RegisterBuilder -->|reg| VReg["VTableRegistrar"]

    VReg -->|commit| VCTX["vector&lt;VTableContext&gt;"]

    MM -->|value| MI["MethodInfo<br/>data / context"]
    SM -->|value| SI["SignalInfo<br/>context"]

    MI -->|context| VCTX
    SI -->|context| VCTX
```

---

## 五、关键设计决策记录

| 决策 | 结论 |
|---|---|
| 错误处理范式 | Status 返回值，不加 operator bool，不抛异常 |
| errno 映射位置 | RawCommon.hpp 的 RawErrorConvert 命名空间，不在 Status.hpp |
| StatusCode 语义 | 按 sd-bus 上下文语义映射，不机械翻译 POSIX 宏名 |
| 回调清理 | RAII Scope Guard + enable_shared_from_this |
| 重载决议 | Callback 用独立模板参数 `Callback&&`，不用 `std::function` |
| 声明注册分离 | 用 `SSDBUS_METHOD(name)` 紧挨声明标注，实现放 `.cpp` |
| 反射注册 | CRTP `MetaObject<Derived>`逐类独立 registry + `RegisterFunc` 类型擦除，`Session::registerObject` 按 `EntryType` 分发 |
| 分层原则 | 依赖方向：不稳定（平台层）→ 稳定（类型层） |
| 跨线程 emit | `Server::emit()` 在服务端层面做了跨线程适配：自动检测 `isOwnerThread()`，同线程直调 `Session::emitSignal()`（同步返回 Status），跨线程通过 `Looper::post()` 投递（eventfd + mutex 保护队列 + swap 最小化锁持有），返回 `SUCCESS` 表示已入队。**`Session` 本身仍单线程**——直接使用 `Session::emitSignal()` 需自行加锁 |
| 双 Session 隔离 | 同一 `sd_bus*` 的 `sd_bus_call`（同步 poll）和 `sd_event_loop`（异步 epoll）不能跨线程共用——会竞争同一个内核 fd 导致回复丢失。正确做法：sync 调用用独立 `Session`（不绑事件循环），信号/async 用另一个 `Session`（绑事件循环） |
| `std::apply` + 成员函数指针 | **不可行**：`obj->*func` 不是 Callable；必须用 lambda 包装 |
| SignalHandler 设计 | `FuncTrait` 提取签名 → `MessagePrivate::read` 反序列化 → `std::apply` 调用回调 |
| listenSignal 时序 | **必须先 `listenSignal` 再 `setInfo`**，否则 `NameAcquired` 丢失 |
| `listenSignal` vs `NameOwnerChanged` | `NameAcquired` 仅 unicast；监控其他连接需 `NameOwnerChanged`（broadcast） |
| emitSignal + registerSignal 分离 | `emitSignal` 只发送消息；`registerSignal` 注册 vtable 使 introspect 可见 |
| RawMessageSharePtr vs RawBusSharePtr | `RawMessageSharePtr` 包装 `sd_bus_message*`，`RawBusSharePtr` 包装 `sd_bus*`，不可混用 |
| SharePtr 所有权语义 | `owned=true` 时析构调用 `unref`；`owned=false` 仅 borrow。拷贝时应仅在 `owned=true` 时 ref |
| callSync 超时语义 | `timeout=0` 在 sd-bus 中表示"默认超时"（~25s），非"无限等待"。`UINT64_MAX` 才表示无限 |
| callAsync 回调生命周期 | **禁止 lambda 捕获 `this`**：`PendingReply` / `Clear` RAII 中一律用 `shared_ptr` 捕获（`RepsPtr`、`key`），确保 Session 拷贝/move 后回调仍安全。`PendingRepsV` 用 `shared_ptr<vector>` 包装，Session 析构时统一回收 |
| callAsync 清理策略 | 回调完成后从 `mRepsPtr` 中 `erase` 已完成的 `PendingReply`，避免 vector 无限增长。`shared_ptr<void>` 可直接用 `std::find` 比较，无需 `void*` + lambda 匹配 |
| VTableRegistrar 设计 | **方案 B（链式 API + 字符串托管）**：`Session::registerBuilder()` 返回 `RegisterBuilder`，支持 `.addMethod().addSignal().commit()`。`VTableContext` 自持 `func/input/output` 字符串，与 vtable 同生共死——sd-bus **不拷贝** vtable 字符串，只存指针。VTableRegistrar 持 RawBusSharePtr 而非 SessionPrivate*，commit 只负责 sd-bus 交互，不重复查 map |
| Method/Signal key 策略 | 使用纯 name 作为 map key（非 name+input），因 D-Bus 协议层面同名 method/signal 只能有唯一签名。registerBuilder 内 `mMethods[aName]` 和 `mSignals[regName]` 统一用 name 索引 |
| registerSingleSig/Method 可叠加 | `sd_bus_add_object_vtable` 支持多次调用叠加；注册 API 的 `registerMethod`/`registerSignal` 每个独立 commit vtable，依赖 sd-bus 自动合并 |
| RegisterBuilder 生命周期 | `RegisterBuilder` 作为 `Session` 内嵌 struct，`commit()` 将 `MethodMap`/`SignalMap` 移入 `SessionPrivate`，Builder 析构不持有任何注册资源 |
| Server 封装 | `Server<Derived>` 捆绑 Session + Looper + `registerObject(this)`，`run()` 一行启动完整服务。构造时传 ServiceInfo，`run()` 自动注册所有 SSDBUS_METHOD/SIGNAL/LISTEN。move-only（不可拷贝），`stop()` 优雅退出 |
