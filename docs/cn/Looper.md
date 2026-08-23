# Looper 事件循环

> 对应头文件：`library/include/Looper.hpp`，公开命名空间：`Dbusxx`

## 简介

`Looper` 驱动一个 D-Bus `Session` 的事件循环（底层基于 sd-event）。它负责派发收到的消息，并在**所属线程**上执行通过 `post()` 提交的任务。`Session` 需要事件循环才能派发已注册的方法与信号。

## 类：`Looper`

```cpp
class Looper {
public:
    Looper() = default;
    explicit Looper(Session& aSession);

    void run();
    void stop();
    void post(std::function<void()> aTask);

    template<typename Callback>
    void onReady(Callback&& aCallback);

    [[nodiscard]] bool isOwnerThread() const;
    [[nodiscard]] Status status() const;
    Session* session() const;
};
```

### 构造函数

| 构造方式 | 说明 |
| --- | --- |
| `Looper()` | 构造空（未绑定）事件循环 |
| `Looper(Session&)` | 构造驱动指定会话的事件循环 |

### 成员方法

| 方法 | 说明 |
| --- | --- |
| `run()` | 运行事件循环（阻塞调用线程） |
| `stop()` | 优雅地请求事件循环停止 |
| `post(task)` | 提交任务，在事件循环线程上执行（可跨线程调用） |
| `onReady(callback)` | 注册回调，在循环就绪时（或 peer 连接被接受时）执行一次；回调应返回 `void` 或 `Status`（否则编译期报错） |
| `isOwnerThread()` | 调用者是否就是拥有该循环的线程 |
| `status()` | 返回循环当前状态 |
| `session()` | 返回该循环驱动的会话指针（可能为 null） |

## API 示例（逐项）

```cpp
#include <dbusxx/Session.hpp>
#include <dbusxx/Looper.hpp>

using namespace Dbusxx;

Session sess = Session::userSession("com.example.Calc");
(void)sess.registerMethod("/com/example/calc", "com.example.Calc", "add",
    [](int32_t a, int32_t b) -> int32_t { return a + b; });

// (1) Looper(Session&) —— 构造并绑定会话
Looper looper(sess);

// (2) Looper() —— 空构造（未绑定，通常不直接用）
[[maybe_unused]] Looper empty;

// (3) onReady(callback) —— 循环就绪时执行一次（回调须返回 void 或 Status）
looper.onReady([]() -> Status {
    std::cout << "loop ready" << std::endl;
    return Status(StatusCode::SUCCESS);
});

// (4) run() —— 运行事件循环（阻塞调用线程，通常放单独线程）
std::thread t([&looper] { looper.run(); });

// (5) isOwnerThread() —— 调用者是否循环所属线程
std::cout << looper.isOwnerThread();   // 主线程上通常为 false

// (6) post(task) —— 投递任务到循环线程（可跨线程）
looper.post([]() {
    std::cout << "posted task" << std::endl;
});

// (7) status() —— 循环当前状态
Status st = looper.status();
std::cout << st.message();

// (8) session() —— 所驱动会话的指针（未绑定则为 null）
Session* s = looper.session();

// (9) stop() —— 优雅停止循环
looper.stop();
t.join();
```

## 相关链接

- `Looper` 是 `Client`（外部事件循环模式）与 `Server` 的核心部件，详见 [Client.md](Client.md)、[Server.md](Server.md)。
- `onReady` 在 `Server::run()` 中被用于初始化（注册接口）。

## 注意事项

- `run()` 是阻塞调用，通常放在专门线程或主线程中执行。
- `post()` 是线程安全的，可在任意线程投递任务，任务会在循环线程串行执行。
- 回调与任务均在循环线程执行，避免在其中长时间阻塞。
