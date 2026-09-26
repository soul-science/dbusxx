# dxxcpp 使用手册

[English](../en/dxxcpp.en.md)

`dxxcpp` 是 dbusxx 的接口代码生成器：读入一份 `.dxx` 接口描述文件，生成可直接编译的 C++ 源码 —— 类型头、服务端骨架、客户端代理。

手写 `DBUSXX_METHOD` / `DBUSXX_SIGNAL` / `DBUSXX_PROPERTY_*` 宏也能暴露接口，接口变多之后用 `.dxx` 更省事：签名、对象路径、接口名、代理方法都由工具生成，不会再出现"服务端和客户端签名写歪了"这类问题。

- 目录：本手册 / [命令行](#1-命令行) / [dxx 语言](#2-dxx-语言) / [生成产物](#3-生成产物) / [使用生成的代码](#4-使用生成的代码) / [CMake 集成](#5-cmake-集成) / [规则与限制](#6-规则与限制) / [故障排查](#7-故障排查)
- 语法速查与完整正例：`tool/dxxcpp/tests/Sample.dxx`
- 反例合集（每条对应一条校验规则）：`tool/dxxcpp/tests/VerifyNegative.cpp`

---

## 0. 快速开始

```bash
# 构建（独立工程，不参与根 CMake）
cmake -S tool/dxxcpp -B tool/dxxcpp/build -DCMAKE_BUILD_TYPE=Release
cmake --build tool/dxxcpp/build -j
cmake --install tool/dxxcpp/build --prefix /usr/local   # 装出 <prefix>/bin/dxxcpp
```

写一份 `calc.dxx`：

```dxx
package com.example.calc;

struct Point {
    int32 x;
    int32 y;
};

using ConfigMap = map<string, string>;

interface Calculator {
    method add(int32 a, int32 b) -> int32;
    method translate(Point p, int32 dx) -> Point;

    @timeout(3000) method getConfig() -> ConfigMap;
    @deprecated method legacy(int32 code);
    @sync method syncOnly(int32 val) -> int32;

    signal valueChanged(int32 oldVal, int32 newVal);

    @readonly property version -> string{"1.0.0"};
    property counter -> int32{0};
};
```

生成：

```bash
dxxcpp calc.dxx -o gen
```

得到（`-o` 目录会被自动创建）：

```
gen/ComExampleCalcTypes.hpp       # 类型：namespace / struct / using / 属性初值
gen/CalculatorSkeleton.hpp/.cpp   # 服务端：<Iface>Interface 纯虚基类 + <Iface>Server
gen/CalculatorProxy.hpp/.cpp      # 客户端：<Iface>Proxy
```

<<<<<<< HEAD
生成的源码一律 `#include <dbusxx/...>`（按安装态布局），服务名由构建系统通过 `DBUSXX_SERVICE_NAME` 注入 —— 生成的头里带 `#error` 守卫，忘了定义会直接编译失败而不是静默连错总线。
=======
生成的源码一律 `#include <dbusxx/...>`（按安装态布局），服务名由构建系统通过 `DBUSXX_SERVICE_NAME` 注入。真正用到该宏的两个文件（`<Interface>Proxy.hpp` 与 `<Interface>Skeleton.cpp`）带 `#error` 守卫，忘了定义会直接编译失败而不是静默连错总线。
>>>>>>> 2fa84fb ([Doc] Add dxxcpp guidance and supplement README)

最省事的接法是让 CMake 帮忙：见 [CMake 集成](#5-cmake-集成)。

---

## 1. 命令行

```
dxxcpp [--dbus] <input.dxx> [-o <output-dir>]
```

| 选项 | 说明 |
|---|---|
| `--dbus` | 后端选择，目前只有 D-Bus（dbusxx）后端，且为默认值；写不写都一样 |
| `-o, --output-dir <dir>` | 产物输出目录，默认当前目录。也接受 `--output-dir=<dir>` 与 `-o<dir>` |
| `--list-outputs` | 只打印产物名（每行 `<role>:<name>`），**不写文件、不建目录** |
| `-h, --help` | 打印用法 |

**退出码**

| 码 | 含义 |
|---|---|
| `0` | 成功（含 `--help` / `--list-outputs`） |
| `1` | 输入有语法错误或语义错误，或文件读写失败 |
| `2` | 命令行用法错误（缺输入、未知选项、`-o` 缺参数、`-o ""`、多个输入文件等） |

诊断信息统一打成长度可点的 `文件:行:列: error: 消息` 形式，走 stderr。成功时每个落盘文件会打印一行 `wrote <path> (<n> bytes)`。

`--list-outputs` 的打印顺序与落盘顺序一致，供构建系统集成使用（`dxxcpp_generate_lib()` 即依赖它）。

---

## 2. dxx 语言

`.dxx` 是行式语法：每条声明以 `;` 结束，声明之间靠关键字区分。

### 2.1 文件结构

一个文件包含一个 `package`、若干类型声明，然后若干个 `interface`。类型声明必须写在接口之前。

```dxx
//! 包声明：决定对象路径与生成的 namespace
package com.example.calc;

//! 自定义类型
struct Point {
    int32 x;
    int32 y;
};

//! 类型别名
using ConfigMap = map<string, string>;

//! 接口（一个文件可以有多个）
interface Calculator {
    //! ...
};

interface Logger {
    method log(string message);
    signal logAdded(string message);
};
```

**包声明**

- 格式固定为 `a.b.c`（**至少两段**），每段必须是合法 C++ 标识符且不能是 C++ 关键字
- 一份 `.dxx` 只能有一个 `package`，且必须是第一条声明
- 由它推导出三样东西：

  | 推导物 | `com.example.calc` → |
  |---|---|
  | D-Bus 对象路径 | `/com/example/calc` |
  | C++ 命名空间 | `Com::Example::Calc` |
  | 类型头文件名 | `ComExampleCalcTypes.hpp` |
  | include guard 前缀 | `COM_EXAMPLE_CALC` |

- **D-Bus well-known name 不在 `.dxx` 里**：它由构建系统通过 `DBUSXX_SERVICE_NAME` 注入（见 [CMake 集成](#5-cmake-集成)）

### 2.2 基础类型

| `.dxx` | C++ | D-Bus 签名 |
|---|---|---|
| `int8` / `uint8` | `std::int8_t` / `std::uint8_t` | `y` |
| `int16` / `uint16` | `std::int16_t` / `std::uint16_t` | `n` / `q` |
| `int32` / `uint32` | `std::int32_t` / `std::uint32_t` | `i` / `u` |
| `int64` / `uint64` | `std::int64_t` / `std::uint64_t` | `x` / `t` |
| `float` | `float` | `d`（库侧按 `double` 适配） |
| `double` | `double` | `d` |
| `bool` | `bool` | `b` |
| `string` | `std::string` | `s` |
| `bytes` | `std::vector<std::uint8_t>` | `ay` |

### 2.3 容器

| 语法 | C++ | 说明 |
|---|---|---|
| `vector<T>` | `std::vector<T>` | 动态数组 |
| `array<T, N>` | `std::array<T, N>` | 定长数组，`N` 必须是正整数 |
| `map<K, V>` | `std::map<K, V>` | 字典 |

- 容器可以任意嵌套，如 `vector<vector<int32>>`、`map<string, vector<Point>>`
- `map<K, V>` 的 **`K` 必须能解析成基础类型**（`string`、`int32`、…）—— 结构体、容器不能当 key
- 模板参数个数写错会被明确拒绝（`vector<T> need 1 arg` / `array<T,N> need 2 args` / `map<K,V> need 2 args`）
- 类型嵌套深度上限 **64 层**（超过报错，避免深层嵌套把编译器和工具一起拖爆）
- 定长数组用 `array<T, N>`；**不要**想着写 `T[N]`，`.dxx` 里没有这个语法

### 2.4 结构体与别名

```dxx
struct Point {
    int32 x;
    int32 y;
};

using ConfigMap = map<string, string>;
using UserId = int32;
```

结构体生成的是**聚合体**（没有构造函数），因此：

- **字段按值包含**，所以只能引用**先声明**的结构体：`struct A { B b; }; struct B { ... };` 会被拒绝，必须把 `B` 写在前面
- 结构体之间**不能互相包含**（成环），别名同样不能成环
- **不允许空结构体**（至少要一个字段）——D-Bus 里 `()` 不是合法签名，库侧也用不了
- 字段名不能重复、必须是合法标识符
- 结构体的 `operator==` 由生成器逐字段补出来（成员函数不破坏聚合体）；库的属性 setter 需要它来比较新旧值

类型名（结构体名、别名名）在文件内必须唯一，且不能是 C++ 关键字 —— 另外**基础类型名与 `vector`/`array`/`map` 是保留名**，不能拿来做结构体/别名名。

别名只是**书写便利**：它会在 `Types.hpp` 里生成对应的 `using` 声明，但**使用处一律展开成底层类型** —— 上面那份 `.dxx` 写的是 `method getConfig() -> ConfigMap;`，而 Skeleton/Proxy 里呈现出来的都是 `std::map<std::string, std::string>`。

### 2.5 接口成员

#### 方法

```dxx
method add(int32 a, int32 b) -> int32;      // 有返回值
method notify(string msg);                  // 无返回值（void）
method getConfig() -> map<string, string>;  // 容器返回值
```

- `-> 返回类型` 省略即 `void`
- 参数写成 `类型 名字`，参数之间用 `,` 分隔，**不接受尾逗号**
- 参数名不能叫 `aCallback`（那是生成的异步回调形参名），除非该方法标了 `@sync`

#### 信号

```dxx
signal valueChanged(int32 oldVal, int32 newVal);
signal itemAdded(Point item);
```

- Skeleton 侧**只注册**信号（发出 `DBUSXX_SIGNAL(...)`），**不生成发送包装**：要发信号就在业务代码里自己调 `Server::emit(path, iface, signal, args...)`（线程安全，可跨线程）（**TODO**：计划生成按信号的类型化发送包装）
- Proxy 侧为每个信号生成一个监听方法 `on<首字母大写的信号名>`（见 [3.4 客户端代理](#34-客户端代理)）

#### 属性

```dxx
@readonly property version -> string{"1.0.0"};
property counter -> int32{0};
property metadata -> map<string, string>{};
property samples -> vector<int32>{1, 2, 3};
property origin -> Point{1, 2};
property history -> vector<Point>{{1, 2}, {3, 4}};
property untouched -> int32;                // 不写初值
```

- `@readonly` → 只读属性（`DBUSXX_PROPERTY_RO`），不写 → 读写属性（`DBUSXX_PROPERTY_RW`）
- 初值写在 `{...}` 里，**按原文透传**给生成代码的宏，里面的 `,` 由 `DBUSXX_PROPERTY_*` 的可变形参兜住；因此可以写多值、嵌套花括号
- 元素之间必须有 `,`，**不接受尾逗号**（与参数/字段列表保持一致）
- 不写初值 → 生成值初始化表达式（`std::int32_t{}`）
- 初值花括号嵌套上限 **64 层**
- 属性类型含 `,`（如 `map<string, string>`）时，生成器会自动用 `decltype(T{})` 包一层掩码 —— 这是用户手写该宏时必须自己注意的坑，生成器替你处理了

### 2.6 注解

注解写在声明前，只影响代码生成，不改变运行期语义。

| 注解 | 可用位置 | 说明 |
|---|---|---|
| `@readonly` | property | 只读属性 |
| `@deprecated` | method / signal | 标记废弃；Proxy 侧带 `[[deprecated]]`（Skeleton 侧只留注释，见下） |
| `@sync` | method | 只生成同步形态 |
| `@async` | method | 只生成两个异步形态 |
| `@timeout(毫秒)` | method | 调用超时，**正整数**、单位毫秒；生成时换算成微秒（`@timeout(3000)` → `callSync<T, 3000000>`） |

- `@sync` 与 `@async` **互斥**
- 放错位置（如给属性写 `@timeout`）、重复写、多写了值（如 `@deprecated(true)`）、写了不存在的注解，都会被拒绝
- Skeleton 侧的 `@deprecated` 只生成 `//! @deprecated` **注释**而不是 `[[deprecated]]` 属性 —— 因为 `DBUSXX_METHOD(&Self::f)` 要取成员地址，标了属性会让生成的头文件自身在 `-Wdeprecated-declarations` 下报警告。Proxy 侧是真属性，调用点才会收到警告

### 2.7 注释

行注释：从 `//` 到行尾，词法阶段丢弃，**整行注释与行尾注释都行**。

本项目的惯例是**文档注释写 `//!`**、普通注释写 `//`（规范与 `tests/*.dxx` 夹具都按这个来）；实现上两者等价。`.dxx` 没有块注释。

### 2.8 词法细节

- `;` 必须写；缺 `;` 时工具会跳到下一个可恢复点再继续，一次能报出多个错误
- 嵌套模板的 `>>` **不会**被当成右移合并，所以 `map<string, vector<int32>>` 可以正常闭合
- UTF-8 BOM 会被跳过（VS Code 用 "UTF-8 with BOM" 存盘也不会报错）
- 字符串字面量用双引号，数字字面量用于 `array<T, N>` 的 `N` 与 `@timeout(...)`

---

## 3. 生成产物

### 3.1 产物清单

| 产物 | `--list-outputs` 角色 | 内容 |
|---|---|---|
| `<Package>Types.hpp` | `types` | 命名空间、结构体（含逐字段 `operator==`）、别名、聚合体静态断言 |
| `<Interface>Skeleton.hpp` | `server` | `<Iface>Interface`（纯虚基类）+ `<Iface>Server`（CRTP + 反射宏）声明 |
| `<Interface>Skeleton.cpp` | `server` | `<Iface>Server` 的构造函数与方法转发定义 |
| `<Interface>Proxy.hpp` | `client` | `<Iface>Proxy` 声明 |
| `<Interface>Proxy.cpp` | `client` | `<Iface>Proxy` 构造函数与方法/监听定义 |

- **每个 interface 一份头 + 一份源**；类型头**按 package 共享一份**
- 头文件名来自 `.dxx` 自身（package / 接口名），与 `dxxcpp_generate_lib(LIB_PREFIX ...)` 的 `LIB_PREFIX` 无关
- 头文件之间用 `#include "本文件名"`（同目录相对包含），消费者按裸文件名 include 即可
- 产物是**声明与定义分离**的：Proxy/Skeleton 的调用体在 `.cpp` 里，因此可以编进静态库/共享库，不会被 header-only 的重复定义绊住
<<<<<<< HEAD
=======
- `DBUSXX_SERVICE_NAME` 的 `#error` 守卫只出现在**真正用到该宏的两个文件**里 —— `<Interface>Proxy.hpp` 与 `<Interface>Skeleton.cpp`；`<Package>Types.hpp` 和 `<Interface>Skeleton.hpp` 根本不提这个宏（后者只放 `DBUSXX_PATH`/`DBUSXX_METHOD` 这类注册宏，它们不引用服务名），因此也没有守卫
>>>>>>> 2fa84fb ([Doc] Add dxxcpp guidance and supplement README)

### 3.2 类型头

`<Package>Types.hpp`（每个 package 一份）：

```cpp
#ifndef COM_EXAMPLE_CALC_TYPES_HPP
#define COM_EXAMPLE_CALC_TYPES_HPP

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <type_traits>
#include <vector>

namespace Com::Example::Calc {
struct Point {
    std::int32_t x;
    std::int32_t y;

    bool operator==(const Point& aOther) const {
        return x == aOther.x
            && y == aOther.y;
    }
};

using ConfigMap = std::map<std::string, std::string>;

static_assert(std::is_aggregate_v<Point>, "Point must be an aggregate");
} // namespace Com::Example::Calc
#endif
```

- 结构体顺序 = `.dxx` 里的声明顺序（这也正是"必须先声明"的原因）
- `operator==` 写成**成员函数**，不破坏聚合体性质（`static_assert` 会验证这一点）
- 别名统一排在结构体之后

### 3.3 服务端骨架

`<Interface>Skeleton.hpp` / `.cpp` 里是**两个类**：

```cpp
#ifndef COM_EXAMPLE_CALC_CALCULATOR_SKELETON_HPP
#define COM_EXAMPLE_CALC_CALCULATOR_SKELETON_HPP

#include <dbusxx/Server.hpp>
#include <dbusxx/Session.hpp>
#include <memory>

#include "ComExampleCalcTypes.hpp"

namespace Com::Example::Calc {
class CalculatorInterface {
public:
    virtual ~CalculatorInterface() = default;
    virtual std::int32_t add(std::int32_t a, std::int32_t b) = 0;
    virtual Point translate(const Point& p, std::int32_t dx) = 0;
    virtual std::map<std::string, std::string> getConfig() = 0;
    virtual void legacy(std::int32_t code) = 0;
    virtual std::int32_t syncOnly(std::int32_t val) = 0;
};

class CalculatorServer final : public Dbusxx::Server<CalculatorServer> {
public:
    explicit CalculatorServer(std::unique_ptr<CalculatorInterface> aIface);

    DBUSXX_PATH("/com/example/calc")
    DBUSXX_IFACE("com.example.calc.Calculator")

    std::int32_t add(std::int32_t a, std::int32_t b);
    DBUSXX_METHOD(add)

    Point translate(const Point& p, std::int32_t dx);
    DBUSXX_METHOD(translate)

    std::map<std::string, std::string> getConfig();
    DBUSXX_METHOD(getConfig)

    //! @deprecated
    void legacy(std::int32_t code);
    DBUSXX_METHOD(legacy)

    std::int32_t syncOnly(std::int32_t val);
    DBUSXX_METHOD(syncOnly)

    DBUSXX_SIGNAL(valueChanged, std::int32_t, std::int32_t)

    DBUSXX_PROPERTY_RO(version, std::string, {"1.0.0"})

    DBUSXX_PROPERTY_RW(counter, std::int32_t, {0})

private:
    std::unique_ptr<CalculatorInterface> mIface;
};

} // namespace Com::Example::Calc
#endif
```

要点：

- 你只需要实现 `<Iface>Interface` 的**全部**纯虚函数（漏一个该类就是抽象类，`make_unique` 会编不过），`<Iface>Server` 负责把调用转发给 `mIface`
- 参数/返回值的传递方式按类型自动选：标量按值、`string`/容器/结构体按 `const&`
- `@sync` 与 `@async` **不影响服务端**：服务端一律是普通同步成员函数
- 信号只有 `DBUSXX_SIGNAL` 注册，**没有** `emitXxx` 包装 → 自己调 `Server::emit(path, iface, signal, args...)`（**TODO**：计划生成发送包装）
- 属性直接在 Skeleton 类上注册（`DBUSXX_PROPERTY_RO/RW`），服务端可以 `getLocalProperty` / `setLocalProperty` 读写

`.cpp` 里是构造函数与方法定义（节选）：

```cpp
#include "CalculatorSkeleton.hpp"

#include <memory>
#include <utility>

#ifndef DBUSXX_SERVICE_NAME
#error "DBUSXX_SERVICE_NAME must be defined (e.g. -DDBUSXX_SERVICE_NAME=\"com.example.app\")"
#endif

namespace Com::Example::Calc {
CalculatorServer::CalculatorServer(std::unique_ptr<CalculatorInterface> aIface)
    : Dbusxx::Server<CalculatorServer>(DBUSXX_SERVICE_NAME)
    , mIface(std::move(aIface)) {}

std::int32_t CalculatorServer::add(std::int32_t a, std::int32_t b) {
    return mIface->add(a, b);
}
}
```

> ⚠️ `DBUSXX_IFACE` 用的是 `package + 接口名`（`com.example.calc.Calculator`），而 `DBUSXX_PATH` 用的是 package 展开的路径（`/com/example/calc`）—— 一个 interface 类型对应一个路径，**没有**按接口再分一层子路径。

### 3.4 客户端代理

`<Interface>Proxy.hpp` 声明如下（定义在 `.cpp` 里）。下面是节选，省掉了 `//! Sync/Async` 注释行与部分方法：

```cpp
class CalculatorProxy {
public:
    explicit CalculatorProxy();

    CalculatorProxy(const CalculatorProxy&) = delete;
    CalculatorProxy& operator=(const CalculatorProxy&) = delete;
    CalculatorProxy(CalculatorProxy&&) = default;
    CalculatorProxy& operator=(CalculatorProxy&&) = default;

    [[nodiscard]] Dbusxx::Reply<std::int32_t> add(std::int32_t a, std::int32_t b);

    [[nodiscard]] Dbusxx::PendingReply<std::int32_t> addAsync(std::int32_t a, std::int32_t b);
    [[nodiscard]] Dbusxx::Status addAsync(
        std::function<void(Dbusxx::Reply<std::int32_t>)> aCallback,
        std::int32_t a, std::int32_t b);

    [[nodiscard]] Dbusxx::Status onValueChanged(
        std::function<void(std::int32_t oldVal, std::int32_t newVal)> aCallback);
};
```

**方法形态**（由注解决定生成几个）

| 注解 | 生成的函数 |
|---|---|
| 无 | `<name>` → `Dbusxx::Reply<T>`（`callSync`）<br>`<name>Async(args...)` → `Dbusxx::PendingReply<T>`（`callAsync`）<br>`<name>Async(aCallback, args...)` → `Dbusxx::Status` |
| `@sync` | 只有 `<name>` |
| `@async` | 只有两个 `<name>Async`（**不生成**裸 `<name>`） |

- 两个异步重载**共用** `<name>Async` 这一个名字，靠第二参数区分：句柄版返回 `PendingReply<T>`，回调版返回 `Status`
- `void` 返回值 → `Dbusxx::Reply<void>` / `Dbusxx::PendingReply<void>`
- 所有方法都带 `[[nodiscard]]`；`@deprecated` 的方法额外带 `[[deprecated]]`（调用点报警告，`-Werror=deprecated-declarations` 下直接编译失败）
- `@timeout` 用法同前：`mClient.callSync<T, 微秒>("name", ...)`；无返回值时生成 `callSync<void, 微秒>(...)`

**信号监听**

每个信号生成 `on` + 首字母大写的监听方法，内部转调 `Client::listenSignal`：

```cpp
Dbusxx::Status onValueChanged(std::function<void(std::int32_t, std::int32_t)> aCallback);
```

`@deprecated` 的信号照常生成监听，但带 `[[deprecated]]`。

> ⚠️ **监听是永久的，没有取消接口**（**TODO**：计划提供可取消句柄）：注册后一直生效到 Proxy 析构。回调在**事件循环线程**上、注册之后的任意时刻被调用，因此闭包只能捕获**生命周期覆盖 Proxy 的对象**（文件级 static、顶层对象，或声明在 Proxy 之前的局部变量）。按引用捕获块内局部变量，出块后回调再触发就是 *stack-use-after-scope*。

**属性没有生成访问器**（**TODO**：计划生成 get/set 访问器）

`.dxx` 里的 `property` 只在 Skeleton 侧注册；Proxy 侧**不生成** `getVersion()` / `setCounter()` 这类访问器，也没有暴露内部的 `Client`。客户端要读写属性，请直接用库的 `Client`：

```cpp
Dbusxx::Client c(Dbusxx::SessionType::USER, DBUSXX_SERVICE_NAME,
                 "/com/example/calc", "com.example.calc.Calculator");
auto ver = c.getProperty<std::string>("version");
(void)c.setProperty<std::int32_t>("counter", 10);
```

`.cpp` 里的构造定义写死了服务名、路径与接口名：

```cpp
CalculatorProxy::CalculatorProxy()
    : mClient(Dbusxx::SessionType::USER, DBUSXX_SERVICE_NAME,
        "/com/example/calc", "com.example.calc.Calculator") {}
```

- 服务名取自 `DBUSXX_SERVICE_NAME`，**不是** Proxy 的构造参数：一个 Proxy 库只服务一个服务名，与服务端对称；客户端侧不需要自己写宏（由构建系统注入）
- 连接类型固定为 `SessionType::USER`；要用系统总线/点对点，请直接用库的 `Client`/`Session`

---

## 4. 使用生成的代码

### 4.1 服务端

```cpp
#include "CalculatorSkeleton.hpp"

#include <map>
#include <memory>
#include <string>

using namespace Com::Example::Calc;

// 实现 <Iface>Interface 的全部纯虚函数（5 个方法与 §0 的 .dxx 一一对应）
class CalcImpl : public CalculatorInterface {
public:
    std::int32_t add(std::int32_t a, std::int32_t b) override { return a + b; }

    Point translate(const Point& p, std::int32_t dx) override {
        return Point { p.x + dx, p.y };
    }

    std::map<std::string, std::string> getConfig() override { return {}; }
    void legacy(std::int32_t code) override { (void)code; }
    std::int32_t syncOnly(std::int32_t val) override { return val; }
};

int main() {
    CalculatorServer server(std::make_unique<CalcImpl>());
    server.run();          // 注册接口 + 进入事件循环（阻塞）
    return 0;
}
```

发信号（自己调，没有生成的包装；**TODO**：计划生成发送包装）：

```cpp
(void)server.emit("/com/example/calc", "com.example.calc.Calculator",
                  "valueChanged", 1, 2);
```

### 4.2 客户端

```cpp
#include "CalculatorProxy.hpp"

#include <iostream>

using namespace Com::Example::Calc;

int main() {
    CalculatorProxy proxy;

    auto r = proxy.add(20, 22);
    if (!r.isError()) {
        std::cout << r.value() << "\n";       // 42
    }

    // 异步（回调版）
    (void)proxy.addAsync([](Dbusxx::Reply<std::int32_t> aRep) {
        std::cout << "async " << aRep.value() << "\n";
    }, 1, 2);

    // 信号监听：捕获的对象必须活过 proxy
    (void)proxy.onValueChanged([](std::int32_t aOld, std::int32_t aNew) {
        std::cout << aOld << " -> " << aNew << "\n";
    });

    return 0;
}
```

### 4.3 目录与 include

产物是**平铺**在一个目录里的，且生成的头之间用相对名互相包含，所以编译时把生成目录加进 include 路径即可：

```cmake
target_include_directories(my_app PRIVATE ${GEN_DIR})
```

`#include` 时用裸文件名（`"CalculatorProxy.hpp"`）；同一个生成目录里包含同一份 `<Package>Types.hpp`，**一个 `.dxx` 必须独占一个输出目录**（详见下节）。

---

## 5. CMake 集成

安装 `dbusxx` 时会把 `dxxcppGenerator.cmake` 一起装进包目录，`find_package(dbusxx)` 后即可用 `dxxcpp_generate_lib()`。

### 5.1 基本用法

```cmake
cmake_minimum_required(VERSION 3.15)
project(hello_service CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(dbusxx REQUIRED)

dxxcpp_generate_lib(
    LIB_PREFIX hello
    INPUT      hello.dxx
    SERVICE    com.example.hello
)

add_executable(hello_service src/service.cpp)
target_link_libraries(hello_service PRIVATE hello_dxx_server)
```

一次调用建出**三个目标**：

| 目标 | 类型 | 内容 |
|---|---|---|
| `<LIB_PREFIX>_dxx_types` | `INTERFACE` | 类型头 + 两个 include 根 + `DBUSXX_SERVICE_NAME="<SERVICE>"` + 依赖 `dbusxx` |
| `<LIB_PREFIX>_dxx_server` | `STATIC` | `<Iface>Skeleton.hpp/.cpp`（服务端实现用它） |
| `<LIB_PREFIX>_dxx_client` | `SHARED` | `<Iface>Proxy.hpp/.cpp`（客户端用它） |

后两者以 `PUBLIC` 链 `_types`，所以链上 `_server` / `_client` 就自动拿到了生成目录（build 树）或安装目录（install 树）的 include 路径、服务名宏与 `dbusxx` 依赖。

**参数**

| 参数 | 必填 | 说明 |
|---|---|---|
| `LIB_PREFIX <prefix>` | ✔ | 目标名/包名/安装目录前缀，一次调用内唯一 |
| `INPUT <file.dxx>` | ✔ | 接口描述文件（相对路径会转成绝对路径） |
| `SERVICE <name>` | ✔ | D-Bus well-known name，做成 `DBUSXX_SERVICE_NAME` 宏 |
| `OUTPUT_DIR <dir>` | ✘ | 产物目录，默认 `${CMAKE_CURRENT_BINARY_DIR}/dxx_<文件名去扩展名>` |
| `INSTALL_CLIENT <on\|off>` | ✘ | 默认 `on`；`off` 表示只构建、不安装 |

**生成时机**：配置阶段先跑一次 `dxxcpp --list-outputs` 问出产物名，再用 `add_custom_command` 在**构建时**真正生成；`.dxx` 被加进 `CMAKE_CONFIGURE_DEPENDS`，所以改了 `.dxx` 下次构建会自动重配置 + 重新生成，不需要手动 `cmake` 一遍。

**`OUTPUT_DIR` 的独占规则**：生成规则是目录级的，所以

- 同一个 `.dxx` + 同一个目录 → 复用已有规则（可以再建别的 `LIB_PREFIX`/`SERVICE`）
- 同一个目录被**另一个** `.dxx` 占用 → 报错（否则两个 interface 会往同一份 `<Package>Types.hpp` 里写）
- 同一个 `.dxx` 换个目录再声明一次 → 报错（换个构建目录即可绕开，见错误消息）

**工具发现顺序**：`-DDXXCPP_EXE=<path>` → 由 `dbusxx_DIR` 推出的 `<prefix>/bin/dxxcpp` → `PATH` 里的 `dxxcpp`。都没有会直接报错提示。

### 5.2 发布给下游

默认（`INSTALL_CLIENT` 为 `on`）安装树会得到：

```
lib/lib<prefix>_dxx_client.so
include/<prefix>_dxx/<Package>Types.hpp
include/<prefix>_dxx/<Iface>Proxy.hpp
lib/cmake/<prefix>_dxx/Config.cmake      # find_dependency(dbusxx CONFIG)
lib/cmake/<prefix>_dxx/Targets.cmake
lib/cmake/<prefix>_dxx/Targets-noconfig.cmake
```

下游项目这样用：

```cmake
find_package(dbusxx CONFIG REQUIRED)
find_package(hello_dxx CONFIG REQUIRED)

add_executable(client_app main.cpp)
target_link_libraries(client_app PRIVATE hello_dxx_client)
```

下游**不需要**自己定义 `DBUSXX_SERVICE_NAME` —— 服务名挂在 `hello_dxx_types` 上，会随 target 传播。

> **服务端从不发布**：安装树里没有 Skeleton 头、也没有 `.a`。谁要实现同一接口，就自己拿 `.dxx` 重新生成骨架 —— 服务端与客户端共享的只有接口定义（`.dxx`），不是实现。
>
> 另外 `_types` 是 `INTERFACE` 目标、不落盘，但**必须进导出集**：否则 `_client` 以 `PUBLIC` 链接它就构成"导出目标依赖未导出目标"，CMake 在 generate 阶段直接报错。

### 5.3 不用 helper 时

想自己接构建系统的话，用 `--list-outputs` 拿产物名，再照 `dxxcpp_generate_lib()` 的写法注册 `add_custom_command`。要点：

- `.dxx` 与 `dxxcpp` 可执行文件都要进 `DEPENDS`
- 每个 `.dxx` 独占一个输出目录
- 消费者的 include 路径要同时包含**生成目录**（供 `"<Iface>Proxy.hpp"` 这类裸名 include）与 **dbusxx 的安装 include 根**（供 `<dbusxx/...>`）

---

## 6. 规则与限制

工具在生成前会做完整的语法 + 语义检查，**只要有错就一个文件都不产出**。

### 6.1 硬性约束

| 约束 | 说明 |
|---|---|
| `package` 至少两段 | `a.b.c` 形式，段必须是合法标识符且非 C++ 关键字 |
| 每条声明以 `;` 结束 | 缺失会报错并跳到下一个可恢复点继续检查 |
| 结构体先声明后使用 | 字段是按值包含的成员 |
| 结构体不可为空 | 至少一个字段 |
| 结构体/别名不能成环 | 别名链、结构体互包含都会被拒 |
| 类型嵌套 ≤ 64 层 | `vector<vector<...>>` 之类的深度 |
| 初值花括号嵌套 ≤ 64 层 | `{{1,2},{3,4}}` 之类的深度 |
| 初值不接受尾逗号 | 与参数/字段列表一致 |
| 生成的名字必须唯一 | 见下 |

### 6.2 成员名与生成名

同一个 interface 里，**方法 / 信号 / 属性共用一个成员名空间**，重名会报 `duplicate member`：

```dxx
interface I {
    method f(int32 v) -> int32;
    property f -> int32;                // duplicate member 'f'
};
```

此外 Proxy 会为每个成员生成名字，撞名会让 Proxy 编不过，所以工具提前拦截：

- 方法 `X` → 生成 `X`（除非 `@async`）与 `XAsync`（除非 `@sync`）
- 信号 `S` → 生成 `onS`（首字母大写）
- **属性不参与**（Proxy 里没有属性的名字）

会报 `'<name>' is generated twice in <I>: by <ownerA> and <ownerB>` 的情形：

```dxx
interface I {
    method f(int32 v) -> int32;
    method fAsync(int32 v) -> int32;     // 'fAsync' is generated twice
};

interface I {
    method onValueChanged(int32 v) -> int32;
    signal valueChanged(int32 v);        // 'onValueChanged' is generated twice
};

interface I {
    signal value(int32 v);
    signal Value(int32 v);               // 首字母大写后同为 'onValue' → generated twice
};
```

下面这两个是**合法的**（工具做过精度处理）：

```dxx
interface I {
    method f(int32 v) -> int32;
    property fAsync -> int32;            // 属性不进 Proxy，不冲突
};

interface I {
    @sync method g(int32 aCallback) -> int32;   // @sync 不生成回调重载，参数可叫 aCallback
};
```

### 6.3 命名与关键字

- 类型名、接口名、成员名、字段名、参数名必须是合法 C++ 标识符
- 不能是 C++ 关键字（`new`、`class` 等）。生成代码里对**字段名/参数名**做了一层转义（`new` → `new_`），但**类型名/接口名/成员名**这类"必须落成标识符"的名字是**直接拒绝**的
- 参数名不能是 `aCallback`（生成的异步回调形参），除非该方法标了 `@sync`

---

## 7. 故障排查

| 现象 | 原因 / 处理 |
|---|---|
| `error: no input .dxx file`（退出码 2） | 忘了给输入文件 |
| `error: cannot open '...'`（1） | 输入文件不存在或不可读 |
| `error: multiple input files`（2） | 一次只能处理一个 `.dxx` |
| `error: '<flag>' doesn't take a value`（2） | 给 `--dbus` / `--list-outputs` / `--help` 写了 `=` 值 |
| `file:line:col: error: ...` + `has syntax error(s)` / `has semantic error(s)`（1） | 按行列修 `.dxx`；语义检查会一次报多条 |
| 生成的代码报 `#error "DBUSXX_SERVICE_NAME must be defined"` | 没通过构建系统定义服务名：用 `dxxcpp_generate_lib()`，或自己加 `-DDBUSXX_SERVICE_NAME="com.example.app"` |
| `unknown argument(s): ...` | `dxxcpp_generate_lib()` 传了未支持/拼错的参数（如已删除的 `INSTALL_SERVER`） |
| `target 'dbusxx' is missing` | 忘了在 `dxxcpp_generate_lib()` 之前 `find_package(dbusxx)` |
| `dxxcpp not found` | 装到别的前缀了：`-DDXXCPP_EXE=<path>`，或让 `dbusxx` 与 `dxxcpp` 装在同一个前缀 |
| `OUTPUT_DIR '...' is already used by '...'` | 两个 `.dxx` 共用一个目录 → 各自给 `OUTPUT_DIR` |
| `INPUT '...' was generated in '...'` | 同一个 `.dxx` 在另一个 binary dir 里已声明过生成规则 → 换个 `OUTPUT_DIR`，或换构建目录 |
| `'...' declares no interface` | 只有类型、没有 `interface`；`dxxcpp_generate_lib()` 需要至少一个接口才能建出服务/客户端库 |

排查手法：先单独跑一遍工具看诊断，再交给 CMake：

```bash
dxxcpp --list-outputs path/to/foo.dxx     # 只看工具怎么看这份输入
dxxcpp path/to/foo.dxx -o /tmp/gen        # 单独生成一份，方便直接编译验证
```

---

## 8. 参考

- 语法正例：`tool/dxxcpp/tests/Sample.dxx`
- 反例合集（每条对应一条校验规则、含最小复现与期望错误消息）：`tool/dxxcpp/tests/VerifyNegative.cpp`
- 生成器源码：`tool/dxxcpp/src/`（`Lexer` → `Parser` → `Sema` → `Codegen`）
- CMake 辅助函数源码：`library/cmake/dxxcppGenerator.cmake`
- 库 API：见仓库根目录 `README.md` 与 `docs/cn/overview.md`
