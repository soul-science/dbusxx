# Session 会话

> 对应头文件：`library/include/Session.hpp`，公开命名空间：`Dbusxx`

## 简介

`Session` 是库的核心类型：管理一条 D-Bus 连接，支持注册方法/信号/属性（通过 `RegisterBuilder`）、发起同步/异步远端调用、收发信号等。

**重要线程模型**：`Session` 是单线程的。注册方法需要事件循环来派发，调用需要能收到回复，因此**服务端与客户端通常要使用两条独立连接**（一条 serve、一条 call）。

## 静态工厂方法

```cpp
static Session systemSession();
static Session systemSession(std::string_view aServiceName);
static Session userSession();
static Session userSession(std::string_view aServiceName);
static Session peerSession(std::string_view aServiceName, bool aIsServer = false);
static Session createSession(SessionType aType = SessionType::USER,
    std::string_view aServiceName = "", bool aIsServer = false);
```

| 方法 | 说明 |
| --- | --- |
| `systemSession()` | 连接系统总线 |
| `systemSession(name)` | 连接系统总线并请求服务名 |
| `userSession()` | 连接用户/会话总线 |
| `userSession(name)` | 连接用户/会话总线并请求服务名 |
| `peerSession(name, isServer)` | 通过 socket 建立点对点连接（无需总线守护进程） |
| `createSession(type, name, isServer)` | 通用工厂，按 `SessionType` 创建 |

## 基本信息

| 方法 | 说明 |
| --- | --- |
| `type()` | 返回会话类型（`SessionType`） |
| `serviceName()` | 返回创建时请求的服务名 |
| `getFd()` | 返回底层连接的文件描述符 |
| `process()` | 处理一批待处理事件，返回 sd-bus process 返回码 |
| `wait(timeoutUsec = UINT64_MAX)` | 阻塞等待事件，超时微秒；默认 `UINT64_MAX` 表示一直等 |
| `flush()` | 刷新缓冲的输出消息到总线 |

## 注册接口

### `RegisterBuilder`（链式构建器）

通过 `registerBuilder(path, iface)` 获得，可批量注册同一 (path, interface) 的方法/信号/属性，最后用 `commit()` 一次性发布为单个 vtable。`commit()` 之后不可再扩展该 builder。

```cpp
class RegisterBuilder {
public:
    RegisterBuilder(RegisterBuilder&&) noexcept = default;
    RegisterBuilder& operator=(RegisterBuilder&&) noexcept = default;
    RegisterBuilder(const RegisterBuilder&) = delete;
    RegisterBuilder& operator=(const RegisterBuilder&) = delete;

    template<typename Func>
    RegisterBuilder& addMethod(std::string_view aName, Func aFunc);
    template<typename Cls, typename Ret, typename... Args>
    RegisterBuilder& addMethod(std::string_view aName, Cls* aCls, Ret(Cls::*aFunc)(Args...));
    template<typename... Args>
    RegisterBuilder& addSignal(std::string_view aName);
    template<typename T>
    RegisterBuilder& addProperty(std::string_view aName, T aValue, bool writable = true);
    [[nodiscard]] Status commit();
};
```

| 方法 | 说明 |
| --- | --- |
| `addMethod(name, callable)` | 用任意可调用对象（lambda/函数对象/`std::function`）注册方法 |
| `addMethod(name, cls, func)` | 用成员函数注册方法 |
| `addSignal<Args...>(name)` | 注册指定参数类型列表的信号 |
| `addProperty<T>(name, init, writable=true)` | 注册属性，包装器持有自己的值副本；`writable=false` 为只读 |
| `commit()` | 提交并发布累积的 vtable，返回 `Status` |

### 快捷注册方法

```cpp
template<typename Func>
[[nodiscard]] Status registerMethod(std::string_view aPath, std::string aIface,
    std::string_view aFuncName, Func&& aFunc);
template<typename Cls, typename Ret, typename... Args>
[[nodiscard]] Status registerMethod(std::string_view aPath, std::string aIface,
    std::string_view aFuncName, Cls* aCls, Ret(Cls::*aFunc)(Args...));
template<typename... Args>
[[nodiscard]] Status registerSignal(std::string_view aPath, std::string aIface,
    std::string_view aSignalName);
template<typename T>
[[nodiscard]] Status registerObject(std::string_view aPath, std::string aIface, T* aObj);
```

- `registerMethod`：注册方法，两种重载（可调用对象 / 成员函数）。
- `registerSignal`：注册指定参数类型的信号。
- `registerObject`：注册对象上全部 `DBUSXX_*` 宏标注的成员（对象必须是 `MetaObject<T>` 子类）。

## 远端调用

### 同步调用

```cpp
template<typename Ret=void, uint64_t TimeoutUsec=0, typename... Args>
[[nodiscard]] Reply<Ret> callSync(std::string_view aService, std::string_view aPath,
    std::string_view aIface, std::string_view aMethod, const Args&... aArgs);
```

阻塞直到收到回复。`Ret` 为返回类型（默认 `void`），`TimeoutUsec` 为可选超时（微秒，0 表示默认）。

### 异步调用（返回句柄）

```cpp
template<typename Ret=void, uint64_t TimeoutUsec=0, typename... Args,
    std::enable_if_t<!CallbackLikeFirstArg<Ret, Args...>::value, int> = 0>
[[nodiscard]] PendingReply<Ret> callAsync(std::string_view aService, std::string_view aPath,
    std::string_view aIface, std::string_view aMethod, const Args&... aArgs);
```

返回 `PendingReply<Ret>`，可 `wait()` 或 `setCallback()`。注意：**首个实参不能是回调**（否则会匹配到下面的回调版本）。

### 异步调用（带回调）

```cpp
template<typename Ret=void, uint64_t TimeoutUsec=0, typename Callback, typename... Args,
    std::enable_if_t<std::is_invocable_r_v<void, Callback, Reply<Ret>>, int> = 0>
[[nodiscard]] Status callAsync(std::string_view aService, std::string_view aPath,
    std::string_view aIface, std::string_view aMethod, Callback&& aCallback, const Args&... aArgs);
```

回调签名为 `void(Reply<Ret>)`。返回 `Status` 表示是否成功派发。

回调会被内部存储，执行完毕后由 RAII 机制自动释放；回调在事件循环线程上被调用，若捕获外部对象请保证其生命周期跨线程安全（建议捕获 `std::shared_ptr` 而非裸指针）。

## 信号

### 订阅信号

```cpp
template<typename Callback>
[[nodiscard]] Status listenSignal(std::string_view aSender,
    std::string_view aPath, std::string_view aIface,
    std::string_view aSignal, Callback&& aCallback);
template<typename Cls, typename Ret, typename... Args>
[[nodiscard]] Status listenSignal(std::string_view aSender,
    std::string_view aPath, std::string_view aIface,
    std::string_view aSignal, Cls* aCls, Ret(Cls::*aFunc)(Args...));
```

- 回调版本：回调参数类型即信号参数类型。
- 成员函数版本：将信号派发到 `aCls` 的成员函数。
- `aSender` 为空表示匹配任意发送者。
- **生命周期**：当前 API 不提供取消订阅句柄，订阅与 `Session` 对象严格绑定（`Session` 销毁时随之清理）；回调在会话存续期间持续触发，长生命周期进程请留意回调资源，避免捕获裸指针或堆积状态。

### 发送信号

```cpp
template<typename... Args>
[[nodiscard]] Status emitSignal(std::string_view aPath, std::string_view aIface,
    std::string_view aSignal, const Args&... aArgs);
```

## 属性

### 本地属性

```cpp
template<typename T>
[[nodiscard]] Status getLocalProperty(std::string_view aPath, std::string_view aIface,
    std::string_view aName, T& aValue);
template<typename T>
[[nodiscard]] Status setLocalProperty(std::string_view aPath, std::string_view aIface,
    std::string_view aName, const T& aValue);
template<typename T>
[[nodiscard]] Status onLocalPropertyChanged(std::string_view aPath, std::string_view aIface,
    std::string_view aName, std::function<void(const T&)>&& aCallback);
```

- `getLocalProperty`：读取本地已注册属性值。
- `setLocalProperty`：写入本地属性值。
- `onLocalPropertyChanged`：注册本地属性变更回调（回调携带新值）。

### 远端属性

```cpp
template<typename Ret>
[[nodiscard]] Reply<Ret> getRemoteProperty(std::string_view aService, std::string_view aPath,
    std::string_view aIface, std::string_view aProp);
template<typename T>
[[nodiscard]] Status setRemoteProperty(std::string_view aService, std::string_view aPath,
    std::string_view aIface, std::string_view aProp, const T& aValue);
template<typename Callback>
[[nodiscard]] Status onRemotePropertyChanged(std::string_view aService, std::string_view aPath,
    std::string_view aIface, std::string_view aProp, Callback&& aCallback);
```

- `getRemoteProperty`：通过 `Properties.Get` 读取远端属性。
- `setRemoteProperty`：通过 `Properties.Set` 写入远端属性。
- `onRemotePropertyChanged`：监听远端属性的 `PropertiesChanged` 变更。与 `listenSignal` 相同，当前不提供取消订阅句柄，订阅随 `Session` 销毁而清理。

## API 示例（逐项）

> 一个 `Session` 是单线程的：serve 与 call 请使用两条独立连接。

```cpp
#include <dbusxx/Session.hpp>
#include <dbusxx/Looper.hpp>

using namespace Dbusxx;

// ── 静态工厂方法 ──────────────────────────────────────────────
// (1) systemSession() —— 连接系统总线
Session s1 = Session::systemSession();
// (2) systemSession(name) —— 系统总线并请求服务名
Session s2 = Session::systemSession("com.example.Svc");
// (3) userSession() —— 连接用户/会话总线
Session s3 = Session::userSession();
// (4) userSession(name) —— 用户/会话总线并请求服务名
Session s4 = Session::userSession("com.example.Calc");
// (5) peerSession(name, isServer) —— 点对点连接（无需总线守护进程）
Session s5 = Session::peerSession("unix:/tmp/dbus.sock", /*isServer=*/true);
// (6) createSession(type, name, isServer) —— 通用工厂
Session s6 = Session::createSession(SessionType::USER);

// ── 基本信息 ──────────────────────────────────────────────────
Session sess = Session::userSession("com.example.Calc");
// (7) type() —— 会话类型
SessionType tp = sess.type();
// (8) serviceName() —— 请求的服务名
std::string svc = sess.serviceName();
// (9) getFd() —— 底层连接文件描述符
int fd = sess.getFd();
// (10) process() —— 处理一批待处理事件（返回 sd-bus 返回码）
int prc = sess.process();
// (11) wait(timeoutUsec) —— 阻塞等待事件（微秒）
int w = sess.wait(1000);
// (12) flush() —— 刷新缓冲的输出消息
sess.flush();

// ── RegisterBuilder（链式注册）────────────────────────────────
// (13) registerBuilder(path, iface) —— 创建构建器
auto builder = sess.registerBuilder("/com/example/calc", "com.example.Calc");
// (14) addMethod(name, callable) —— 用可调用对象注册方法
builder.addMethod("add", [](int32_t a, int32_t b) { return a + b; });
// (15) addMethod(name, cls, memfn) —— 用成员函数注册方法
struct Calc { int32_t sub(int32_t a, int32_t b) { return a - b; } } calc;
builder.addMethod("sub", &calc, &Calc::sub);
// (16) addSignal<Args...>(name) —— 注册信号（2 个 int32_t 参数，与下方监听/发射一致）
builder.addSignal<int32_t, int32_t>("valueChanged");
// (17) addProperty<T>(name, init, writable) —— 注册属性（writable=false 只读）
builder.addProperty<int32_t>("counter", 0, /*writable=*/true);
// (18) commit() —— 提交并发布 vtable
Status st18 = builder.commit();

// ── 快捷注册方法 ──────────────────────────────────────────────
// (19) registerMethod(path, iface, name, callable)
Status st19 = sess.registerMethod("/com/example/calc", "com.example.Calc",
    "mul", [](int32_t a, int32_t b) { return a * b; });
// (20) registerMethod(path, iface, name, cls, memfn)
Status st20 = sess.registerMethod("/com/example/calc", "com.example.Calc",
    "sub2", &calc, &Calc::sub);
// (21) registerSignal<Args...>(path, iface, name)
Status st21 = sess.registerSignal<int32_t>("/com/example/calc",
    "com.example.Calc", "changed");
// (22) registerObject(path, iface, obj) —— 注册 DBUSXX_* 宏标注对象
//     需继承 MetaObject<MyObj>：
//     Status st22 = sess.registerObject("/com/example/calc",
//         "com.example.Calc", &obj);

// ── 远端调用 ──────────────────────────────────────────────────
// (23) callSync<Ret, TimeoutUsec>(service, path, iface, method, args...)
auto rep = sess.callSync<int32_t>("com.example.Calc", "/com/example/calc",
    "com.example.Calc", "add", 20, 22);
std::cout << rep.value();                       // 42
// (24) callAsync<Ret>(...) —— 异步，返回句柄
auto pend = sess.callAsync<int32_t>("com.example.Calc", "/com/example/calc",
    "com.example.Calc", "add", 1, 2);
pend.wait();
// (25) callAsync<Ret>(..., cb, args...) —— 异步，带回调
Status st25 = sess.callAsync<int32_t>("com.example.Calc", "/com/example/calc",
    "com.example.Calc", "add", [](Reply<int32_t> r) {
        std::cout << r.value();
    }, 3, 4);

// ── 信号 ──────────────────────────────────────────────────────
// (26) listenSignal(sender, path, iface, signal, callback)
Status st26 = sess.listenSignal("", "/com/example/calc", "com.example.Calc",
    "valueChanged", [](int32_t v, int32_t w) { std::cout << v << ", " << w; });
// (27) listenSignal(sender, path, iface, signal, cls, memfn)
Status st27 = sess.listenSignal("", "/com/example/calc", "com.example.Calc",
    "valueChanged", &calc, &Calc::sub);
// (28) emitSignal(path, iface, signal, args...)
Status st28 = sess.emitSignal("/com/example/calc", "com.example.Calc",
    "valueChanged", 7, 8);

// ── 本地属性 ──────────────────────────────────────────────────
int32_t cur = 0;
// (29) getLocalProperty(path, iface, name, out&)
Status st29 = sess.getLocalProperty("/com/example/calc", "com.example.Calc",
    "counter", cur);
// (30) setLocalProperty(path, iface, name, value)
Status st30 = sess.setLocalProperty("/com/example/calc", "com.example.Calc",
    "counter", 42);
// (31) onLocalPropertyChanged<T>(path, iface, name, cb)
Status st31 = sess.onLocalPropertyChanged<int32_t>(
    "/com/example/calc", "com.example.Calc", "counter",
    [](const int32_t& v) { std::cout << v; });

// ── 远端属性 ──────────────────────────────────────────────────
// (32) getRemoteProperty<Ret>(service, path, iface, prop)
auto rp = sess.getRemoteProperty<int32_t>("com.example.Calc",
    "/com/example/calc", "com.example.Calc", "counter");
// (33) setRemoteProperty<T>(service, path, iface, prop, value)
Status st33 = sess.setRemoteProperty<int32_t>("com.example.Calc",
    "/com/example/calc", "com.example.Calc", "counter", 9);
// (34) onRemotePropertyChanged(service, path, iface, prop, cb)
Status st34 = sess.onRemotePropertyChanged(
    "com.example.Calc", "/com/example/calc", "com.example.Calc",
    "counter", [](const int32_t& v) { std::cout << v; });
```

## 注意事项

- 一个 `Session` 是单线程的：serve 与 call 需要两条独立连接。
- 异步回调（`callAsync`、`listenSignal`、`onRemotePropertyChanged` 等）会在事件循环线程上被调用，不要在回调里阻塞；回调由库内部 RAII 管理，执行后自动释放。
- `listenSignal` / `onRemotePropertyChanged` 不提供取消订阅接口，订阅随 `Session` 销毁而清理，长生命周期进程请留意回调资源。
- `registerObject` 要求对象继承自 `MetaObject<T>`。
