# Client 远端服务代理

> 对应头文件：`library/include/Client.hpp`，公开命名空间：`Dbusxx`

## 简介

`Client` 封装一个会话加一个事件循环，面向一个固定的远端（服务名、路径、接口）提供类型安全的调用、信号与属性访问。

它有两种用法：

- **自管模式**（四参构造）：`Client` 内部自带 `Session` 与事件循环（含独立线程池），开箱即用；
- **外部事件循环模式**（两参构造）：复用外部传入的 `Looper`。

`Client` 的所有方法都是线程安全的：内部会判断是否在循环线程上，必要时通过 `post()` 投递到循环线程再同步等待结果。

## 类：`Client`

```cpp
class Client {
public:
    Client() = default;
    explicit Client(SessionType aType, std::string aService,
        std::string aPath, std::string aInterface);
    explicit Client(Looper& aLooper, std::string aService,
        std::string aPath, std::string aInterface);

    ~Client() = default;
    Client(Client&&) noexcept;
    Client& operator=(Client&&) noexcept;
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    template<typename Ret=void, uint64_t TimeoutUsec=0, typename... Args>
    [[nodiscard]] Reply<Ret> callSync(std::string_view aMethod, const Args&... aArgs);

    template<typename Ret=void, uint64_t TimeoutUsec=0, typename... Args>
    [[nodiscard]] PendingReply<Ret> callAsync(std::string_view aMethod, const Args&... aArgs);

    template<typename Ret=void, uint64_t TimeoutUsec=0, typename Callback, typename... Args>
    [[nodiscard]] Status callAsync(std::string_view aMethod, Callback&& aCallback, const Args&... aArgs);

    template<typename Callback>
    [[nodiscard]] Status listenSignal(std::string_view aSignal, Callback&& aCallback);
    template<typename Cls, typename Ret, typename... Args>
    [[nodiscard]] Status listenSignal(std::string_view aSignal, Cls* aCls, Ret(Cls::*aFunc)(Args...));

    template<typename Ret>
    [[nodiscard]] Reply<Ret> getProperty(std::string_view aProp);
    template<typename T>
    [[nodiscard]] Status setProperty(std::string_view aProp, const T& aValue);
    template<typename Callback>
    [[nodiscard]] Status onPropertyChanged(std::string_view aProp, Callback&& aCallback);
};
```

## 构造函数

| 构造方式 | 说明 |
| --- | --- |
| `Client()` | 构造空（非法）客户端 |
| `Client(SessionType, service, path, interface)` | **自管模式**：创建自己的会话与事件循环线程 |
| `Client(Looper&, service, path, interface)` | **外部事件循环模式**：复用已有 `Looper`（及其 `Session`） |

`Client` 不可拷贝（拷贝删除），可移动。

## 方法调用

### 同步调用

```cpp
template<typename Ret=void, uint64_t TimeoutUsec=0, typename... Args>
[[nodiscard]] Reply<Ret> callSync(std::string_view aMethod, const Args&... aArgs);
```

阻塞直到收到回复。`Ret` 为返回类型（默认 `void`），`TimeoutUsec` 为可选超时（微秒，0 表示默认）。

### 异步调用（返回句柄）

```cpp
template<typename Ret=void, uint64_t TimeoutUsec=0, typename... Args>
[[nodiscard]] PendingReply<Ret> callAsync(std::string_view aMethod, const Args&... aArgs);
```

返回 `PendingReply<Ret>`，可 `wait()` 或 `setCallback()`。

### 异步调用（带回调）

```cpp
template<typename Ret=void, uint64_t TimeoutUsec=0, typename Callback, typename... Args>
[[nodiscard]] Status callAsync(std::string_view aMethod, Callback&& aCallback, const Args&... aArgs);
```

回调签名为 `void(Reply<Ret>)`，位于实参的最前面。

## 信号

```cpp
template<typename Callback>
[[nodiscard]] Status listenSignal(std::string_view aSignal, Callback&& aCallback);
template<typename Cls, typename Ret, typename... Args>
[[nodiscard]] Status listenSignal(std::string_view aSignal, Cls* aCls, Ret(Cls::*aFunc)(Args...));
```

订阅远端信号：回调版本（回调参数类型即信号参数类型）或成员函数版本。

## 属性

```cpp
template<typename Ret>
[[nodiscard]] Reply<Ret> getProperty(std::string_view aProp);
template<typename T>
[[nodiscard]] Status setProperty(std::string_view aProp, const T& aValue);
template<typename Callback>
[[nodiscard]] Status onPropertyChanged(std::string_view aProp, Callback&& aCallback);
```

- `getProperty`：通过 `Properties.Get` 读取远端属性。
- `setProperty`：通过 `Properties.Set` 写入远端属性。
- `onPropertyChanged`：监听远端属性的 `PropertiesChanged` 变更。

## API 示例（逐项）

```cpp
#include <dbusxx/Client.hpp>
#include <dbusxx/Looper.hpp>

using namespace Dbusxx;

// (1) Client() —— 空构造（非法，不可用）
[[maybe_unused]] Client empty;

// (2) Client(SessionType, service, path, iface) —— 自管模式
Client c(SessionType::USER, "com.example.Calc",
         "/com/example/calc", "com.example.Calc");

// (3) callSync<Ret, TimeoutUsec>(method, args...) —— 同步调用
auto r = c.callSync<int32_t>("add", 20, 22);
if (!r.isError()) {
    std::cout << r.value();               // 42
}

// (4) callAsync<Ret>(method, args...) —— 异步，返回句柄
auto pend = c.callAsync<int32_t>("add", 1, 2);
pend.wait();
std::cout << pend.reply().value();        // 3

// (5) callAsync<Ret>(method, cb, args...) —— 异步，带回调
Status st5 = c.callAsync<int32_t>("add", [](Reply<int32_t> rep) {
    std::cout << rep.value();
}, 1, 2);

// (6) getProperty<Ret>(prop) —— 读远端属性
auto ver = c.getProperty<std::string>("version");
std::cout << ver.value();

// (7) setProperty<T>(prop, value) —— 写远端属性
Status st7 = c.setProperty<int32_t>("counter", 10);

// (8) onPropertyChanged(prop, cb) —— 监听远端属性变更
Status st8 = c.onPropertyChanged("counter", [](const int32_t& v) {
    std::cout << v;
});

// (9) listenSignal(signal, cb) —— 订阅信号（回调版）
Status st9 = c.listenSignal("valueChanged", [](int32_t oldV, int32_t newV) {
    std::cout << oldV << " -> " << newV;
});

// (10) listenSignal(signal, cls, memfn) —— 订阅信号（成员函数版）
struct Sink { void onChanged(int32_t a, int32_t b) {} } sink;
Status st10 = c.listenSignal("valueChanged", &sink, &Sink::onChanged);

// (11) Client(Looper&, service, path, iface) —— 外部事件循环模式
Session sess = Session::userSession();
Looper looper(sess);
Client c2(looper, "com.example.Calc", "/com/example/calc", "com.example.Calc");
looper.run();   // 由外部循环驱动
```

## 注意事项

- `Client` 所有公开方法都是线程安全的，可在任意线程调用。
- 回调（异步调用、信号、属性变更）在事件循环线程上执行，避免在其中阻塞。
- 自管模式的 `Client` 有自己的异步线程池；构造后即可使用，无需手动驱动。
