# Server 服务端封装

> 对应头文件：`library/include/Server.hpp`，公开命名空间：`Dbusxx`

## 简介

`Server<Derived>`（CRTP）把 `Session`、`Looper` 与反射能力（`MetaObject`）打包成一个开箱即用的服务端。在 `run()` 时，它会基于派生类上的 `DBUSXX_*` 宏注册接口，然后持续服务直到被停止。

它是大多数服务端场景的首选入口：只需继承 `Server<Derived>`、用宏标注成员，再调用 `run()`。

## 模板类：`Server<Derived>`

```cpp
template<typename Derived>
class Server : public MetaObject<Derived> {
public:
    Server() = delete;
    explicit Server(SessionType aType, std::string_view aServiceName);
    explicit Server(std::string_view aServiceName);

    Server(Server&&) noexcept;
    Server& operator=(Server&&) noexcept;
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    void run();
    void stop();
    void forceStop();
    void post(std::function<void()> aTask);

    template<typename... Args>
    [[nodiscard]] Status emit(std::string_view aPath, std::string_view aIface,
        std::string_view aSignal, Args&&... aArgs);

    template<typename T>
    [[nodiscard]] Status getProperty(std::string_view aPath, std::string_view aIface,
        std::string_view aName, T& aValue);
    template<typename T>
    [[nodiscard]] Status setProperty(std::string_view aPath, std::string_view aIface,
        std::string_view aName, const T& aValue);
    template<typename T>
    [[nodiscard]] Status onPropertyChanged(std::string_view aPath, std::string_view aIface,
        std::string_view aName, std::function<void(const T&)>&& aCallback);

    [[nodiscard]] Status status() const;
    [[nodiscard]] SessionType type() const;

protected:
    Session& session();
    Looper& looper();
};
```

### 模板参数

| 参数 | 说明 |
| --- | --- |
| `Derived` | 派生类（CRTP 自类型），其上用 `DBUSXX_*` 宏标注要暴露的接口 |

### 构造函数

| 构造方式 | 说明 |
| --- | --- |
| `Server()` | 已删除，必须指定会话类型与名称 |
| `Server(SessionType, serviceName)` | 按类型创建服务（system/user/peer）；以**服务端角色**建立连接（peer 模式下作为接受端） |
| `Server(serviceName)` | 便捷构造，使用用户会话（`SessionType::USER`） |

`Server` 不可拷贝（拷贝删除），可移动。

## 生命周期

| 方法 | 说明 |
| --- | --- |
| `run()` | 注册标注的接口并运行事件循环（**阻塞**；注册在 `onReady` 中完成） |
| `stop()` | 优雅停止（若已出错则为 no-op） |
| `forceStop()` | 无条件停止事件循环 |
| `post(task)` | 投递任务到服务端循环线程执行 |

## 信号与属性

`Server` 的方法都是**线程安全的**：内部判断是否在循环线程上，必要时跨线程投递。

| 方法 | 说明 |
| --- | --- |
| `emit(path, iface, signal, args...)` | 发送信号（可跨线程调用） |
| `getProperty(path, iface, name, value&)` | 读取本地已注册属性 |
| `setProperty(path, iface, name, value)` | 写入本地属性 |
| `onPropertyChanged(path, iface, name, cb)` | 注册本地属性变更回调 |

## 查询

| 方法 | 说明 |
| --- | --- |
| `status()` | 服务端当前状态（错误优先） |
| `type()` | 绑定的会话类型 |

## 受保护访问器

| 方法 | 说明 |
| --- | --- |
| `session()` | 访问底层 `Session`（用于直接注册） |
| `looper()` | 访问底层 `Looper` |

## API 示例（逐项）

```cpp
#include <dbusxx/Server.hpp>

using namespace Dbusxx;

class CalcServer : public Server<CalcServer> {
public:
    // (1) Server(name) —— 便捷构造（用户会话，请求唯一名）
    CalcServer() : Server("com.example.Calc") {}

    DBUSXX_PATH("/com/example/calc")
    DBUSXX_IFACE("com.example.Calc")
    int32_t add(int32_t a, int32_t b) { return a + b; }
    DBUSXX_METHOD(add)
    DBUSXX_PROPERTY_RW(counter, int32_t, 0)
    DBUSXX_SIGNAL(valueChanged, int32_t, int32_t)
};

// (2) Server(SessionType, name) —— 显式指定会话类型
class SysServer : public Server<SysServer> {
public:
    SysServer() : Server(SessionType::SYSTEM, "com.example.Sys") {}
    DBUSXX_PATH("/com/example/sys")
    DBUSXX_IFACE("com.example.Sys")
    void ping() {}
    DBUSXX_METHOD(ping)
};

int main() {
    CalcServer server;             // (1) 构造（用户总线，请求唯一名）

    // (3) run() —— 注册接口并进入事件循环（阻塞）
    std::thread t([&server] { server.run(); });

    // (4) status() —— 当前状态（错误优先）
    //     注意：run() 刚启动时，onReady 里的接口注册可能尚未执行完，
    //     此处返回的通常是初始化前的初始状态，不代表最终注册结果。
    Status st = server.status();
    // (5) type() —— 会话类型
    SessionType tp = server.type();

    // (6) post(task) —— 投递任务到服务端循环线程（可跨线程）
    server.post([]() { std::cout << "task on loop thread"; });

    // (7) emit(path, iface, signal, args...) —— 发送信号（可跨线程）
    Status st7 = server.emit("/com/example/calc", "com.example.Calc",
        "valueChanged", 1, 2);

    // (8) getProperty(path, iface, name, out&) —— 读本地属性
    int32_t c = 0;
    Status st8 = server.getProperty("/com/example/calc", "com.example.Calc",
        "counter", c);

    // (9) setProperty(path, iface, name, value) —— 写本地属性
    Status st9 = server.setProperty("/com/example/calc", "com.example.Calc",
        "counter", 42);

    // (10) onPropertyChanged<T>(path, iface, name, cb) —— 本地属性变更回调
    Status st10 = server.onPropertyChanged<int32_t>(
        "/com/example/calc", "com.example.Calc", "counter",
        [](const int32_t& v) { std::cout << v; });

    // (11) stop() —— 优雅停止（已出错则为 no-op）
    server.stop();
    // (12) forceStop() —— 无条件停止（需要时取消注释）
    // server.forceStop();
    t.join();

    return 0;
}
```

## 注意事项

- `run()` 阻塞当前线程；若要退出，可在其他线程调用 `stop()` / `forceStop()`。
- 接口注册在 `onReady` 回调中完成（基于 `DBUSXX_PATH`/`DBUSXX_IFACE` 分组）。
- `emit`、`getProperty`、`setProperty`、`onPropertyChanged` 均可跨线程安全调用。
