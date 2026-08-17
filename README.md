# dbusxx

基于 systemd sd-bus 的 C++17 D-Bus 库。

## 介绍

dbusxx 是面向 Linux 的现代 C++ D-Bus 库，目标是在现代 C++ 中提供表达力强、易于使用的 API。它在 systemd 的 sd-bus——一套简洁的 C 语言 D-Bus 实现——之上增加了一层抽象。

dbusxx 的设计避开了直接操作 sd-bus C API 的繁琐与易错之处，选择了一条简单而强大的路线：以类型安全、直观友好的方式封装 D-Bus 的常用操作，从设计上规避了消息、vtable、句柄等底层细节带来的常见问题。

dbusxx 虽然基于 sd-bus，但并不局限于 systemd 环境——sd-bus 本身是通用的 D-Bus 库，只要系统提供 libsystemd（主流发行版均有），非 systemd 环境下同样可以使用。

主要优势：

- **类型安全**：参数/返回值/信号/属性都是真实 C++ 类型，编译期校验，不用手写 D-Bus 签名
- **写法直接**：服务端 `Server<Derived>` + 宏标注即可暴露接口；客户端一个 `Client` 对象就指向远端服务
- **覆盖完整**：系统/用户/点对点三种连接，方法、信号、属性（含远端属性）、事件循环、断线重连
- **资源省心**：sd-bus 句柄全部 RAII 管理，不泄漏
- **依赖少**：只依赖 libsystemd

## 特性

- 方法参数/返回值、信号、属性都用模板在编译期校验，支持基础类型、`std::string`、`std::vector`、`std::array`、`std::map`、`std::tuple`
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

using namespace Dbusxx;

int main() {
    Session s = Session::userSession("com.example.Calc");

    // 注册方法（任意可调用对象）
    (void)s.registerMethod("/com/example/calc", "com.example.Calc", "add",
        [](int32_t a, int32_t b) -> int32_t { return a + b; });

    // 调用远端方法
    auto r = s.callSync<int32_t>(
        "com.example.Calc", "/com/example/calc", "com.example.Calc", "add",
        20, 22);
    std::cout << r.value() << std::endl;
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
| `PendingReply<Ret>` | 异步调用的句柄（`setCallback` / `wait` / `reply`） |
| `Status` / `StatusCode` | 错误码和状态 |
| `MetaObject<Derived>` | 反射元数据收集基类（供宏使用） |

## 支持的类型

方法参数/返回值、信号、属性都支持这些类型，编译期用 `isValidArgs` 校验，运行期自动生成 D-Bus 签名。

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
| `example_client_internal` | `Client` 自管模式 |
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

[GPL-2.0](./LICENSE)
