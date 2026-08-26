# dbusxx

[English](README.en.md)

基于 systemd sd-bus 的 C++17 D-Bus 库。

## 文档

完整 API 参考，请见 **[文档总览（中文）](docs/cn/overview.md)**，或英文版 [Documentation Overview](docs/en/overview.en.md)。

## 介绍

dbusxx 是一个面向 Linux 的 C++17 D-Bus 库，让你用几行代码就能在不同进程之间通信——无论是暴露自己的服务，还是调用别人的接口。

用它你可以：

- **几行代码暴露服务**：继承 `Server<Derived>`，给成员函数加一个宏，方法、信号、属性就注册到总线上了；
- **像调本地函数一样调远端**：一个 `Client` 对象就代表远端服务，同步 `callSync` / 异步 `callAsync` 随意选；
- **直接传你想传的数据**：基础类型、`std::string`、容器，甚至自定义结构体，都能作为参数/返回值，自动编解码；
- **收发信号、读写属性**：类型安全的信号回调、属性 get/set 和变更监听都已封装好。

你只需要写业务代码——D-Bus 签名、消息、句柄、事件循环这些底层细节都由库处理。

适用场景：桌面/嵌入式应用的进程间通信、系统服务间的 IPC、与现有 D-Bus 服务（systemd、NetworkManager 等）交互。只要系统带 libsystemd（主流发行版都有），不依赖 systemd 环境也能用。

主要优势：

- **类型安全**：参数/返回值/信号/属性都是真实 C++ 类型，编译期校验，不用手写 D-Bus 签名
- **数据随心传**：基础类型、`std::string`、容器，甚至自定义结构体都能直接作为参数/返回值，自动编解码
- **写法直接**：服务端 `Server<Derived>` + 宏标注即可暴露接口；客户端一个 `Client` 对象就指向远端服务
- **覆盖完整**：系统/用户/点对点三种连接，方法、信号、属性（含远端属性）、事件循环、断线重连
- **资源省心**：资源全部自动管理，不泄漏
- **依赖少**：只需系统自带 libsystemd

## 特性

- 方法参数/返回值、信号、属性都用模板在编译期校验，支持基础类型、`std::string`、`std::vector`、`std::array`、`std::map`、`std::tuple`，以及任意**自定义聚合体结构体**（自动生成 `(...)` 签名，字段自动展开，嵌套/容器递归）
- 支持系统总线、用户总线、点对点（peer，不需要 bus daemon）三种连接
- 同步调用 `callSync` / 异步调用 `callAsync`，超时通过模板参数指定（微秒）
- 服务端用 `Server<Derived>` + 反射宏（`DBUSXX_METHOD` / `DBUSXX_SIGNAL` / `DBUSXX_PROPERTY_*`）注册，也可以直接用 `Session::registerMethod` 或 `RegisterBuilder`
- 信号 `emitSignal` / `listenSignal`，回调可以是任意可调用对象或成员函数
- 属性：本地属性支持注册、读写、变更回调；远端属性走 `Properties.Get/Set` 和 `PropertiesChanged` 监听
- `Looper` 事件循环（sd-event），支持跨线程 `post()` 和 `onReady` 初始化回调
- `Client` 是远端服务的代理，可以自管事件循环，也可以复用外部的 `Looper`
- RAII 管理资源，断线自动重连，错误统一用 `Status` / `StatusCode`

## 依赖

- CMake >= 3.15
- 支持 C++17 的编译器（GCC 9+ / Clang）
- systemd 开发库 **libsystemd >= 249**

Ubuntu/Debian 安装依赖：

```bash
sudo apt install cmake g++ pkg-config libsystemd-dev
```

## 构建与安装

```bash
# Debug（集成 ASan/UBSan，便于调试）
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# Release
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release

# 安装（建议使用 Release 构建）
sudo cmake --install build-release
```

默认安装到 `/usr/local`：

- 头文件：`/usr/local/include/dbusxx/`
- 动态库：`/usr/local/lib/libdbusxx.so`（版本化 `libdbusxx.so.1`）
- CMake 包：`/usr/local/lib/cmake/dbusxx/`（`find_package(dbusxx)` 可用）

## 用法

### 服务端

```cpp
#include <dbusxx/Server.hpp>

using namespace Dbusxx;

class CalcServer : public Server<CalcServer> {
public:
    CalcServer() : Server("com.example.Calc") {}

    // 设置后续标注所属的 路径 / 接口
    DBUSXX_PATH("/com/example/calc")
    DBUSXX_IFACE("com.example.Calc")

    int32_t add(int32_t a, int32_t b) { return a + b; }
    DBUSXX_METHOD(add)

    std::string greet(const std::string& name) { return "Hello, " + name + "!"; }
    DBUSXX_METHOD(greet)

    DBUSXX_SIGNAL(valueChanged, int32_t, int32_t)

    DBUSXX_PROPERTY_RO(version, std::string, std::string("1.0.0"))
    DBUSXX_PROPERTY_RW(counter, int32_t, 0)
};

int main() {
    CalcServer server;   // 绑定用户总线并请求唯一名 "com.example.Calc"
    server.run();        // 注册接口并进入事件循环（阻塞）
}
```

### 客户端

```cpp
#include <dbusxx/Client.hpp>
#include <dbusxx/Looper.hpp>
#include <dbusxx/Session.hpp>

#include <iostream>

using namespace Dbusxx;

int main() {
    // 自管模式：内部自带 Session + 事件循环线程
    Client c(SessionType::USER, "com.example.Calc",
             "/com/example/calc", "com.example.Calc");

    // 同步调用
    auto r = c.callSync<int32_t>("add", 20, 22);
    if (!r.isError()) {
        std::cout << "add(20,22) = " << r.value() << std::endl;
    }

    // 异步调用（回调签名为 void(Reply<Ret>)，回调在前、实参在后）
    (void)c.callAsync<int32_t>("add", [](Reply<int32_t> rep) {
        std::cout << "async add = " << rep.value() << std::endl;
    }, 1, 2);

    // 属性读写
    auto ver = c.getProperty<std::string>("version");
    std::cout << "version = " << ver.value() << std::endl;
    (void)c.setProperty<int32_t>("counter", 10);

    // 信号监听
    (void)c.listenSignal("valueChanged", [](int32_t oldV, int32_t newV) {
        std::cout << "valueChanged: " << oldV << " -> " << newV << std::endl;
    });

    // 外部 Looper 模式：复用已有 Session + Looper
    Session sess = Session::userSession();
    Looper looper(sess);
    Client c2(looper, "com.example.Calc", "/com/example/calc", "com.example.Calc");
    looper.run();   // 事件循环（阻塞）
}
```

### 直接用 Session

```cpp
#include <dbusxx/Session.hpp>
#include <dbusxx/Looper.hpp>

#include <chrono>
#include <iostream>
#include <thread>

using namespace Dbusxx;

// Session 是单线程的：注册方法需要事件循环来派发，调用需要能收回复，
// 两者必须使用两个独立连接（一个 serve、一个 call）。
int main() {
    // 连接 A：注册方法 + 事件循环（服务端）
    Session server = Session::userSession("com.example.Calc");
    (void)server.registerMethod("/com/example/calc", "com.example.Calc", "add",
        [](int32_t a, int32_t b) -> int32_t { return a + b; });

    Looper looper(server);
    std::thread t([&looper] { looper.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // 连接 B：调用远端方法（独立连接）
    Session client = Session::userSession();
    auto r = client.callSync<int32_t>(
        "com.example.Calc", "/com/example/calc", "com.example.Calc", "add",
        20, 22);
    std::cout << r.value() << std::endl;   // 42

    looper.stop();
    t.join();
    return 0;
}
```

### 自定义结构体

聚合体结构体可直接用作方法参数/返回值、信号参数，字段自动映射为 D-Bus 结构体，无需额外注册：

```cpp
#include <dbusxx/Server.hpp>
#include <dbusxx/Client.hpp>

#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace Dbusxx;

// 自定义结构体（聚合体，字段按声明顺序映射到 D-Bus 结构体）
struct Point {
    int32_t x;
    int32_t y;
};

struct Person {
    std::string              name;
    int32_t                  age;
    std::vector<std::string> tags;   // 字段本身可以是容器
};

class GeoServer : public Server<GeoServer> {
public:
    GeoServer() : Server("com.example.Geo") {}

    DBUSXX_PATH("/com/example/geo")
    DBUSXX_IFACE("com.example.Geo")

    Point addPoint(const Point& p, const Point& q) {
        return Point { p.x + q.x, p.y + q.y };
    }
    DBUSXX_METHOD(addPoint)

    Person echoPerson(const Person& p) { return p; }
    DBUSXX_METHOD(echoPerson)

    std::vector<Point> echoPoints(const std::vector<Point>& pts) { return pts; }
    DBUSXX_METHOD(echoPoints)
};

int main() {
    GeoServer server;
    std::thread serverThread([&server] { server.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    Client c(SessionType::USER, "com.example.Geo",
             "/com/example/geo", "com.example.Geo");

    // 结构体出入参
    auto r = c.callSync<Point>("addPoint", Point { 3, 4 }, Point { 5, 6 });
    std::cout << "addPoint = (" << r.value().x << ", " << r.value().y << ")\n"; // (8, 10)

    // 混合字段结构体 + 容器字段
    auto r2 = c.callSync<Person>("echoPerson",
        Person { "alice", 30, { "a", "b" } });
    std::cout << "echoPerson = " << r2.value().name << ", " << r2.value().age << "\n";

    // 结构体数组
    auto r3 = c.callSync<std::vector<Point>>("echoPoints",
        std::vector<Point> { { 1, 1 }, { 2, 2 } });
    std::cout << "echoPoints size = " << r3.value().size() << "\n";

    server.stop();
    serverThread.join();
    return 0;
}
```

## 核心类型

| 类型 | 作用 |
|---|---|
| `Session` | 连接管理、注册方法/信号/属性、同步/异步调用、信号收发、本地/远端属性 |
| `Client` | 远端服务代理，内部是 `Session` + `Looper` |
| `Server<Derived>` | 服务端（CRTP），把 `Session` + `Looper` + 反射注册捆在一起 |
| `Looper` | 事件循环：`run/stop/post/onReady` |
| `Message` | 消息，支持流式 `<<` / `>>` 读写 |
| `Reply<Ret>` | 同步调用的返回值（`value()/isError()/status()`） |
| `PendingReply<Ret>` | 异步调用的句柄（`setCallback` / `wait` / `waitFor` / `reply`） |
| `Status` / `StatusCode` | 错误码和状态 |
| `MetaObject<Derived>` | 反射元数据收集基类（供宏使用） |

## 支持的类型

方法参数/返回值、信号、属性都支持这些类型，编译期用 `isValidArg` 校验，运行期自动生成 D-Bus 签名。

### 基础类型

| C++ 类型 | D-Bus 签名 | 说明 |
|---|---|---|
| `int8_t` / `uint8_t` | `y` | 单字节 |
| `int16_t` | `n` | 16 位有符号整数 |
| `uint16_t` | `q` | 16 位无符号整数 |
| `int32_t` | `i` | 32 位有符号整数 |
| `uint32_t` | `u` | 32 位无符号整数 |
| `int64_t` | `x` | 64 位有符号整数 |
| `uint64_t` | `t` | 64 位无符号整数 |
| `bool` | `b` | 布尔 |
| `double` | `d` | 64 位浮点 |
| `float` | `d` | 序列化时按 `double` 适配 |
| `std::string` / `std::string_view` | `s` | UTF-8 字符串 |
| `const char*` / `char*` | `s` | C 风格字符串 |

### 容器

| C++ 类型 | D-Bus 签名 | 说明 |
|---|---|---|
| `std::vector<T>` | `a<sig(T)>` | 动态数组（如 `std::vector<int32_t>` → `ai`） |
| `std::array<T, N>` | `a<sig(T)>` | 定长数组 |
| `std::map<K, V>` / `std::unordered_map<K, V>` | `a{<sig(K)><sig(V)>}` | 字典（如 `std::map<std::string, int32_t>` → `a{si}`） |
| `std::tuple<Args...>` | 逐元素展开 | 仅 `Message` 流式 `read`/`write` 支持一次操作多值，**不能**作为方法参数/返回值类型 |

### 自定义结构体（聚合体）

任意满足以下条件的自定义 `struct` 可直接作为方法参数/返回值、信号参数，**无需任何注册**：

- 是**聚合体**（无用户提供的构造函数、无虚函数、无 private/protected 非静态数据成员）
- 所有成员本身也是受支持的类型（基础类型、容器、嵌套结构体）
- 成员数不超过 20

字段按声明顺序映射为 D-Bus 结构体 `(…)`，全部在编译期自动完成：

| C++ 类型 | D-Bus 签名 | 说明 |
|---|---|---|
| `struct Point { int32_t x; int32_t y; }` | `(ii)` | 两字段 |
| `struct Person { std::string name; int32_t age; bool vip; }` | `(sib)` | 混合字段 |
| `std::vector<Point>` | `a(ii)` | 结构体数组 |
| `struct Rect { Point a; Point b; }` | `((ii)(ii))` | 结构体套结构体 |

几点说明：

- 容器可以嵌套，比如 `std::vector<std::vector<int32_t>>` → `aai`
- `void` 表示无返回值/无参数（`Reply<void>` / `PendingReply<void>`）
- `std::string_view` 读写时按 `const char*` 处理，`float` 按 `double` 处理
- 不支持的 C++ 类型在编译期就 `static_assert` 报错，不会留到运行期

## 反射宏

| 宏 | 作用 |
|---|---|
| `DBUSXX_PATH(path)` | 设置后面标注的对象路径 |
| `DBUSXX_IFACE(iface)` | 设置后面标注的接口名 |
| `DBUSXX_METHOD(name)` | 把成员函数暴露成 D-Bus 方法 |
| `DBUSXX_SIGNAL(name, Types...)` | 声明信号 |
| `DBUSXX_PROPERTY_RO(name, Type, init)` | 只读属性 |
| `DBUSXX_PROPERTY_RW(name, Type, init)` | 读写属性 |

## 示例

`example/` 下有几个能直接跑的示例：

| 示例 | 说明 |
|---|---|
| `example_server` | `Server<Derived>` 的完整用法：方法、信号、属性、同步/异步、跨线程 emit |
| `example_session` | 直接用 `Session` 的完整流程 |
| `example_register` | `registerMethod` / `registerSignal` 支持的各种可调用类型 |
| `example_client_internal` | `Client` 自管模式（含自定义结构体往返测试 Step 8.5） |
| `example_client_external` | `Client` + 外部 `Looper` 模式 |
| `example_peer` | 点对点连接（peer server + client）全流程 |
| `example_install` | 验证安装产物能否被独立项目使用（`find_package(dbusxx)`） |

## 集成到自己的项目

```cmake
cmake_minimum_required(VERSION 3.15)
project(my_app CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(dbusxx REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE dbusxx)
```

## 许可证

[LGPL-3.0](./LICENSE)
