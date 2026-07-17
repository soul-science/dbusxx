# C++ 知识点笔记 (ssdbus 项目)

---

## 1. 函数模板重载决议 (Overload Resolution)

### 1.1 问题场景

两个 `callAsync` 重载并存：

```cpp
// 重载 ①：无回调，返回 PendingReply
template<typename Ret, typename... Args>
PendingReply<Ret> callAsync(sv×4, const Args&... aArgs);

// 重载 ②：有回调，返回 Status（回调参数为 std::function）
template<typename Ret, typename... Args>
Status callAsync(sv×4, std::function<void(Reply<Ret>)>&& cb, const Args&...);
```

调用时：

```cpp
session.callAsync<std::string>("svc", "/path", "iface", "method",
    [](Reply<std::string> aRep) { /* ... */ });
```

### 1.2 为什么会选错重载？

C++ 重载决议只看**参数匹配度**，不看返回类型：

| | 重载 ① | 重载 ② |
|---|--------|--------|
| 匹配方式 | lambda → `const Args&...` 精确匹配 | lambda → `std::function<...>` 需要隐式转换 |
| 优劣 | ✅ 精确匹配（更好） | ❌ 用户定义转换（更差） |
| 返回类型 | `PendingReply<Ret>` | `Status` |

编译器不看 `Status st = ...`，只看参数 → 选 ① → 返回 `PendingReply` → 赋值给 `Status` → **编译报错**。

### 1.3 解决方案：模板参数代替 std::function

```cpp
// ✅ 新：Callback 作为独立模板参数，lambda 精确匹配
template<typename Ret, typename Callback, typename... Args>
Status callAsync(sv×4, Callback&& aCallback, const Args&... aArgs) {
    static_assert(std::is_invocable_r_v<void, Callback, Reply<Ret>>,
        "callback must be callable as: void(Reply<Ret>)");
    // ...
}
```

**原理**：`Callback&&` 是转发引用（forwarding reference），lambda 传入时 `Callback` 被推导为 lambda 的闭包类型，**完全精确匹配**。此时两个重载中 lambda 都是精确匹配，但重载 ② 模板参数更特化（`Callback` 单个参数 vs `Args...` 参数包），编译器会选择 ②。

### 1.4 委托调用时的陷阱

带默认值的重载委托给完整重载时：

```cpp
// ❌ 错误：只指定 Ret，0 → int，可能匹配到无回调重载
return callAsync<Ret>(
    svc, path, iface, method, 0, std::forward<Callback>(cb), args...);

// ✅ 正确：全指定模板参数 + 显式类型转换
return callAsync<Ret, Callback, Args...>(
    svc, path, iface, method, static_cast<uint64_t>(0),
    std::forward<Callback>(cb), args...);
```

`static_cast<uint64_t>(0)` 确保数字字面量在重载决议时不会被推导为 `int`，避免匹配到 `const Args&...` 分支。

---

## 2. static_assert 校验回调签名

### 2.1 类型萃取工具对比

| 工具 | 功能 | 头文件 |
|------|------|--------|
| `std::is_invocable_v<F, Args...>` | F 能否以 Args... 调用 | `<type_traits>` |
| `std::is_invocable_r_v<R, F, Args...>` | 能否调用且返回 R | `<type_traits>` |
| `std::invoke_result_t<F, Args...>` | 获取返回类型 | `<type_traits>` |
| `std::is_void_v<T>` | T 是否为 void | `<type_traits>` |

### 2.2 is_invocable_v 的漏洞

```cpp
// ❌ is_invocable_v 不检查返回类型 → 下面这个会通过编译！
static_assert(std::is_invocable_v<decltype([](Reply<int>) -> int { return 0; }), Reply<int>>);
// 通过！但回调不应该返回 int
```

### 2.3 正确的写法

```cpp
static_assert(std::is_invocable_r_v<void, Callback, Reply<Ret>>,
    "callAsync callback must be callable as: void(Reply<Ret>)");
```

等价于同时检查：
1. `Callback` 能接受 `Reply<Ret>` 参数
2. 返回类型是 `void`（或可隐式转为 `void`）

编译错误时的消息示例：
```
error: static assertion failed: callAsync callback must be callable as: void(Reply<Ret>)
```

---

## 3. Use-After-Free 与 enable_shared_from_this

### 3.1 完整调用链

```
sd-bus 事件触发
  ┌─ ReplyAsyncHandler::onReply(self) ────────────────┐
  │  self 是裸指针，指向 ReplyAsyncHandler              │
  │  self->mCallback(self)                             │
  │   ┌─ PendingReply::setCallback 的 lambda ──────┐   │
  │   │  Reply<Ret> rep(...); aCallback(rep);      │   │
  │   │   ┌─ Session 的 lambda ──────────────────┐ │   │
  │   │   │  cb(aRep);       // 用户回调          │ │   │
  │   │   │  mReps.erase(it); // 从列表移除       │ │   │
  │   │   │  delete rep;     // 💥 释放 PendingReply │ │   │
  │   │   │    → mHandler (shared_ptr) 析构       │ │   │
  │   │   │    → ref count: 1 → 0                │ │   │
  │   │   │    → ReplyAsyncHandler 被 delete      │ │   │
  │   │   └──────────────────────────────────────┘ │   │
  │   └────────────────────────────────────────────┘   │
  │  self->isFinished = true;  // 💥 self 指向已释放内存 │
  └────────────────────────────────────────────────────┘
```

### 3.2 为什么不是简单的「换个顺序」能解决？

问题本质：`PendingReply` 持有 `ReplyAsyncHandler` 的最后一个 `shared_ptr`。当 `PendingReply` 被 `delete` 时，`ReplyAsyncHandler` 必然跟着析构——但调用栈还在 `onReply` 内部！

### 3.3 enable_shared_from_this 内部机制

```cpp
struct ReplyAsyncHandler
    : public std::enable_shared_from_this<ReplyAsyncHandler> {
    // 内部有一个 weak_ptr<ReplyAsyncHandler>，由 shared_ptr 构造函数自动初始化
};
```

`shared_from_this()` 等价于 `weak_ptr.lock()`——如果对象由 `shared_ptr` 管理，返回一个新的 `shared_ptr`，引用计数 +1。

### 3.4 修复后的调用链

```
onReply(self)
  auto guard = self->shared_from_this();  // ref count: 1 → 2
  self->mCallback(self)
    → ... → delete rep
      → PendingReply 析构
      → mHandler 析构 → ref count: 2 → 1 （ReplyAsyncHandler 仍存活）
  self->isFinished = true;  // ✅ self 存活！guard 持有引用
  return 0;
  // guard 析构 → ref count: 1 → 0 → ReplyAsyncHandler 正确释放
```

### 3.5 关键约束

| 条件 | 违反后果 |
|------|----------|
| 对象必须由 `shared_ptr` 管理 | `std::bad_weak_ptr` 异常 |
| 不在构造/析构中调用 | 此时 `weak_ptr` 未就绪或已失效 |
| CRTP 模板参数正确 | 编译错误 |

### 3.6 替代方案对比

| 方案 | 核心思想 | 优点 | 缺点 |
|------|---------|------|------|
| `enable_shared_from_this` | 回调栈内延长 ReplyAsyncHandler 生命周期 | 改动最小 | 依赖 shared_ptr 管理 |
| 延迟删除 | 不在回调内 delete，标记后在 process() 统一清理 | 不依赖 shared_from_this | 内存释放有延迟 |
| 全 shared_ptr | mReps 存 shared_ptr，等引用计数归零自然释放 | 零手动 delete | 需配合 guard 解决同样问题 |

---

## 4. RAII 与异常安全

### 4.1 问题

回调中手动管理资源：

```cpp
rep->setCallback([this, rep] (Reply<Ret> aRep) {
    cb(aRep);                   // 💥 用户回调可能抛异常
    mReps.erase(it);            // 不会执行 → 悬空指针
    delete rep;                 // 不会执行 → 内存泄漏
});
```

### 4.2 解决方案：RAII Scope Guard

```cpp
rep->setCallback([this, cb, repPtr] (Reply<Ret> aRep) {
    struct Clear {
        std::vector<std::shared_ptr<void>>& reps;
        void* ptr;
        ~Clear() {
            auto it = std::find_if(reps.begin(), reps.end(),
                [this](const auto& p) { return p.get() == ptr; });
            if (it != reps.end()) reps.erase(it);
            // shared_ptr 自动管理，析构时自动释放
        }
    } clear{mReps, repPtr};

    cb(aRep);  // 即使抛异常，clear 析构保证清理
});
```

**原理**：局部对象 `clear` 在栈展开（正常返回或异常传播）时必然调用析构函数，保证 `mReps.erase` 一定执行。

### 4.3 Lambda 内定义 struct

C++ 允许在 lambda 体内定义局部类型（C++14+），`Clear` 只在当前 lambda 作用域可见，不会污染外部命名空间。

### 4.4 注意：lambda 捕获 this 用于成员访问

```cpp
// Clear::ptr 是成员变量，lambda 内不能直接按名捕获
[this](const auto& p) { return p.get() == ptr; }  // ✅ this->ptr
// 不能用 [ptr]  —— ptr 不是局部变量
```

---

## 5. delete void* 是未定义行为

### 5.1 为什么？

```cpp
void* ptr = new PendingReply<std::string>(...);
delete ptr;  // ❌ UB
```

`delete` 需要知道：
1. **调用哪个析构函数** — `void*` 丢失了类型信息，编译器不知道要调 `~PendingReply<string>()`
2. **释放多少内存** — 不知道对象大小，可能释放错误

对于数组 `delete[] void*` 同样 UB。

### 5.2 解决方案

```cpp
// 方案 A：static_cast 恢复类型
delete static_cast<PendingReply<Ret>*>(ptr);

// 方案 B：直接存带类型的指针
struct Clear {
    PendingReply<Ret>* rep;  // 保留类型
    ~Clear() { delete rep; }
};
```

### 5.3 用 shared_ptr 彻底避免

```cpp
std::vector<std::shared_ptr<void>> mReps;
// erase 后 shared_ptr 析构 → 自动调正确的 delete，零手动管理
```

---

## 6. 防御性编程：push_back 先于 setCallback

### 6.1 问题

```cpp
// ❌ 顺序
rep->setCallback(...);   // 先注册
mReps.push_back(rep);    // 后添加

// 单线程事件循环中安全，但多线程或未来改动可能：
// onReply 在 setCallback 后立即触发 → Clear 查找 mReps → 找不到 rep → 直接 delete
// 然后 push_back 加入悬空指针
```

### 6.2 修复

```cpp
// ✅ 顺序
mReps.push_back(rep);    // 先添加
rep->setCallback(...);   // 后注册

// 最坏情况：回调同步触发 → Clear 在 mReps 中能找到 → erase 正确清理
```

这是一种**零成本**的防御性习惯——不需要任何额外代码，只是换一下顺序。

---

## 7. 拷贝/移动构造 vs 赋值函数：何时 = default

### 7.1 核心判断

| 成员类型 | 拷贝/移动语义 | = default 可行？ |
|----------|:----------:|:---------------:|
| `int`, `bool`, `double` 等基本类型 | ✅ 逐位拷贝 | ✅ |
| `std::string`, `std::vector` | ✅ 自带深拷贝 | ✅ |
| `std::shared_ptr<T>` | ✅ 引用计数自管理 | ✅ |
| `std::unique_ptr<T>` | ❌ 不可拷贝 | 拷贝需 `= delete` |
| 裸指针 `T*` | ❌ 只拷贝地址 | ❌ 需手动深拷贝 |
| 文件句柄 `int fd` | ❌ 语义不正确 | ❌ 需手动处理 |

### 7.2 Reply 的实际情况

```cpp
class Reply : public Message {
    // Message 只有 shared_ptr<MessagePrivate>，= default 安全
protected:
    Ret mValue {};      // 值类型，= default 逐位拷贝
    bool mIsErr {false}; // bool，= default 安全
};

// ✅ 全部可以 = default
Reply(const Reply&) = default;
Reply(Reply&&) noexcept = default;
Reply& operator=(const Reply&) = default;
Reply& operator=(Reply&&) = default;
```

### 7.3 反例：裸指针深拷贝

```cpp
class Buffer {
    char* mData;
    size_t mSize;
public:
    Buffer(const Buffer& other)           // 深拷贝构造
        : mSize(other.mSize), mData(new char[other.mSize]) {
        memcpy(mData, other.mData, mSize);
    }

    Buffer(Buffer&& other) noexcept       // 移动构造（有副作用）
        : mSize(other.mSize), mData(other.mData) {
        other.mData = nullptr;  // 副作用：置空源对象
        other.mSize = 0;
    }

    // ❌ = default 只拷贝指针 → 两个对象指向同一块内存 → double free
    Buffer& operator=(const Buffer& other) {
        if (this != &other) {
            delete[] mData;              // 释放旧资源
            mSize = other.mSize;
            mData = new char[mSize];      // 分配新资源
            memcpy(mData, other.mData, mSize);
        }
        return *this;
    }

    ~Buffer() { delete[] mData; }
};
```

### 7.4 规则概要

- 如果手动写了拷贝/移动构造（因为逻辑需要），赋值也要相应处理
- 如果手动写只是因为一些「额外逻辑」（如日志），而所有成员都自带正确语义，可以考虑在函数体内做完逻辑后调用 `std::swap` 或其他成员函数来复用默认语义
- 转换构造函数（从其他类型构造）不影响拷贝/移动的 = default 决策

---

## 8. 成员初始化顺序

### 8.1 C++ 标准规定

成员初始化**严格按声明顺序**执行，**无视初始化列表的书写顺序**。

```cpp
class Demo {
    int a = 10;   // ① 声明在前
    int b;        // ② 声明在后

    Demo() : b(a * 2) {}  // 实际上 b 在 a 之后初始化
    // 执行顺序：a=10 → b=20
};
```

### 8.2 为什么初始化列表容易踩坑

```cpp
class Bad {
    bool mIsErr;     // ① 声明在前 → 先初始化
    int mValue;      // ② 声明在后 → 后初始化

    // 初始化列表写的是 mIsErr 在前，看起来像先设 mIsErr
    Bad() : mIsErr(read(mValue)) {}
    // 实际先执行 mIsErr(read(mValue))
    // 此时 mValue 还是未初始化状态 → 读脏数据 → UB
};
```

### 8.3 编译器警告

GCC/Clang 加 `-Wreorder`（通常默认开启）会对此类问题发出警告：

```
warning: 'Bad::mIsErr' will be initialized after 'Bad::mValue'
```

### 8.4 安全做法：构造函数体

```cpp
class Safe {
    bool mIsErr {false};  // 声明顺序随意
    int mValue {};

    Safe() : mIsErr(false), mValue(0) {  // 所有成员先默认初始化
        mIsErr = read(mValue);           // ✅ 此时 mValue 已初始化
    }
};
```

构造函数体内赋值 vs 初始化列表初始化的区别：
- 初始化列表：一次初始化
- 构造函数体：先初始化列表（或默认初始化），再赋值（多一次赋值操作）
- 对于 `bool`、`int` 等基本类型，多一次赋值的开销可忽略不计
- 换来的是声明顺序的完全解耦，不会因成员重排引入 UB

### 8.5 初始化 ≠ 内存分配

- **内存分配**：整个对象一次性分配，所有成员字节从一开始就存在
- **初始化**：构造函数逐步执行每个成员的初始化器
- **UB 根源**：并不是「没内存」，而是该成员还未开始其生命周期（初始化器尚未执行）

---

## 9. 局部变量 vs 成员变量的初始化时机

### 9.1 局部变量

```cpp
{
    int a;          // 默认初始化（对于 int 是垃圾值）
    int b = a * 2;  // 读垃圾值 → UB
}
```

但如果 `a` 只被写不被读：

```cpp
{
    int a;
    read(a);  // read 拿引用只往里写 → 覆盖垃圾值 → OK
    int b = a; // a 已被赋值 → OK
}
```

### 9.2 成员变量

```cpp
struct S {
    int b;
    int a;
    S() : b(read(a)) {}  // a 未初始化 → 读其地址写数据 → UB
};
```

即使 `read` 只写不读，C++ 视角下 `a` 的生命周期还没开始，传递引用本身就是 UB。

### 9.3 关键区别

| | 局部变量 | 成员变量（初始化列表） |
|---|---|---|
| 声明在前已初始化？ | 是（已执行到该行） | 不一定（取决于声明顺序） |
| 可依赖声明顺序解决问题？ | 是（代码就是顺序） | 否（初始化顺序是声明顺序，不是书写顺序） |
| 安全策略 | 代码顺序直观 | 移到构造函数体 |

## 10. explicit operator bool 的设计考量

### 10.1 为什么加 explicit

```cpp
class Status {
    explicit operator bool() const { return isSuccess(); }
};
```

不加 `explicit`，`Status` 可以在任意需要 `bool` 的语境中隐式转换：

```cpp
Status s(StatusCode::FAIL);

// 😱 这些都能编译，但语义完全错误：
int x = s + 5;       // s 隐式转 false(0), x = 5
bool y = s == 0;     // FAIL == 0 → true? 不对
std::cout << s;      // 输出 0，而不是错误信息

// 更隐蔽的：重载决议走错
void foo(int);
void foo(Status);
foo(s);              // 不加 explicit：走 foo(int)，因为 bool→int 优先
```

### 10.2 加了 explicit 后允许什么

C++ 标准规定 `explicit operator bool` 在**上下文转换**中自动调用：

```cpp
if (s)     { }   // ✅ 允许
!s;              // ✅ 允许
s ? a : b;       // ✅ 允许
while (!s) { }   // ✅ 允许
bool b = s;      // ❌ 禁止，必须显式 s.isSuccess()
int x = s + 5;   // ❌ 禁止
```

### 10.3 标准库先例

`std::optional`、`std::shared_ptr`、`std::function` 的 `operator bool` 都是 `explicit`。

### 10.4 即使有 explicit，也可以考虑不加 operator bool

如果类已有 `isSuccess()` / `isError()` 方法，`operator bool` 反而引入歧义——看到 `if (status)` 要想到底是 "status 为真" 还是 "status 成功"。直接写 `if (status.isSuccess())` 语义更明确，也避免重载决议的潜在风险。

---

## 11. 分层架构：平台映射放在 Adaptor 层

### 11.1 初始设计（反例）

最开始 `fromErrno()` 和 `makeStatus()` 计划放在 `Status.hpp` 中：

```cpp
// ❌ Status.hpp — 杂糅了平台依赖
#ifndef SSDBUS_DBUS_RETURN_STATUS_HPP
#define SSDBUS_DBUS_RETURN_STATUS_HPP

#include <cerrno>   // ← 平台相关头文件

namespace SSDbus {

enum class StatusCode : uint8_t {
    SUCCESS = 0,
    NOT_FOUND,        // 对应 ENOENT
    ACCESS_DENIED,    // 对应 EACCES
    TIMEOUT,          // 对应 ETIMEDOUT
    // ...
};

// ❌ Status.hpp 里直接出现 POSIX 宏 EINVAL、ENOENT...
inline StatusCode fromErrno(int aErrno) {
    switch (aErrno) {
        case 0:          return StatusCode::SUCCESS;
        case EINVAL:     return StatusCode::INVALID_ARG;
        case ENOENT:     return StatusCode::NOT_FOUND;
        case EACCES:     return StatusCode::ACCESS_DENIED;
        case ETIMEDOUT:  return StatusCode::TIMEOUT;
        // ...
    }
}

inline Status makeStatus(int aRet) {
    if (aRet >= 0) return Status(StatusCode::SUCCESS);
    return Status(fromErrno(-aRet));
}

}
#endif
```

### 11.2 为什么这是问题

`Status.hpp` 本应是一个**纯类型定义文件**，但引入了 `<cerrno>` 后：

1. **平台绑定**：`EINVAL`、`ENOENT` 等是 POSIX 宏，在 Windows 上不存在或值不同。`Status.hpp` 直接被污染为 POSIX-only
2. **依赖方向倒置**：上层业务代码（`Session.hpp`、`Method.hpp`）只想要 `StatusCode::NOT_FOUND`，但 `#include "Status.hpp"` 顺带拉入了 `<cerrno>` 和几十个 `E*` 宏
3. **替换底层库的代价**：假设将来从 sd-bus 切换到 GDBus，errno 语义可能完全不同（GDBus 用 `GError` 的 domain+code），得改 `Status.hpp`——但 `Status.hpp` 作为「公共类型」被几十个文件包含，改动影响面巨大
4. **单测困难**：想测「收到 TIMEOUT 时的处理逻辑」，必须构造一个合法的 errno 值，而不仅仅是 `Status(StatusCode::TIMEOUT)`

### 11.3 正确的分层

```cpp
// ============ Status.hpp ============
// 纯类型定义，零平台依赖，#include 链极短
#ifndef SSDBUS_DBUS_RETURN_STATUS_HPP
#define SSDBUS_DBUS_RETURN_STATUS_HPP

#include <string>    // 仅用于 message()

namespace SSDbus {

enum class StatusCode : uint8_t {
    SUCCESS = 0,
    INVALID_ARG,      // 参数无效
    NOT_FOUND,        // 服务/对象/接口不存在
    ACCESS_DENIED,    // 权限不足
    NAME_EXISTS,      // 总线名已被占用
    NOT_CONNECTED,    // 未连接到总线
    CONN_RESET,       // 连接被重置
    BUSY,             // 资源忙
    TIMEOUT,          // 调用超时
    NO_MEMORY,        // 内存不足
    NO_REPLY,         // 对方未回复
    IO_ERROR,         // I/O 错误
    MSG_TOO_LONG,     // 消息超长
    LIMIT_EXCEEDED,   // 超出限制
    PROTOCOL_ERROR,   // 协议错误
    TYPE_MISMATCH,    // D-Bus 类型不匹配
    DISCONNECTED,     // 对端断开
    NO_METHOD,        // 无效方法/请求描述符
    UNKNOWN_ERROR     // 未知错误（兜底）
};

inline constexpr const char* statusMessage(StatusCode aCode) { /* ... */ }

class Status {
public:
    Status() = default;
    /* implicit */ Status(StatusCode aCode) : mCode(aCode) {}

    StatusCode code() const { return mCode; }
    bool isSuccess() const { return mCode == StatusCode::SUCCESS; }
    bool isError()   const { return mCode != StatusCode::SUCCESS; }
    std::string message() const { return statusMessage(mCode); }

private:
    StatusCode mCode = StatusCode::SUCCESS;
};

} // namespace SSDbus
#endif
```

```cpp
// ============ RawAdaptor.hpp ============
// 平台适配层：唯一允许出现 #include <cerrno> 的地方
#ifndef SSDBUS_RAW_ADAPTOR_HPP
#define SSDBUS_RAW_ADAPTOR_HPP

#include <cerrno>         // ← 只有 Adaptor 层需要
#include <systemd/sd-bus.h>

#include "Status.hpp"     // 依赖 Status.hpp（单向）

namespace SSDbus {
namespace Adaptor {
namespace RawError {

// ① 平台映射函数：POSIX errno → 项目 StatusCode
inline StatusCode fromErrno(int aErrno) {
    switch (aErrno) {
        case 0:           return StatusCode::SUCCESS;
        case EINVAL:      return StatusCode::INVALID_ARG;
        case ENOENT:      return StatusCode::NOT_FOUND;
        case EACCES:      return StatusCode::ACCESS_DENIED;
        case EADDRINUSE:  return StatusCode::NAME_EXISTS;
        case ENOTCONN:    return StatusCode::NOT_CONNECTED;
        case ECONNRESET:  return StatusCode::CONN_RESET;
        case EBUSY:       return StatusCode::BUSY;
        case ETIMEDOUT:   return StatusCode::TIMEOUT;
        case ENOMEM:      return StatusCode::NO_MEMORY;
        case ENOMSG:      return StatusCode::NO_REPLY;
        case EBADMSG:     return StatusCode::PROTOCOL_ERROR;
        case EIO:         return StatusCode::IO_ERROR;
        case EMSGSIZE:    return StatusCode::MSG_TOO_LONG;
        case E2BIG:       return StatusCode::LIMIT_EXCEEDED;
        case EPROTO:      return StatusCode::PROTOCOL_ERROR;
        case ENOTSUP:     return StatusCode::PROTOCOL_ERROR;
        case ENXIO:       return StatusCode::TYPE_MISMATCH;
        case EBADR:       return StatusCode::NO_METHOD;
        case EBADRQC:     return StatusCode::TYPE_MISMATCH;
        default:          return StatusCode::UNKNOWN_ERROR;
    }
}

// ② sd-bus 约定封装：int ret → Status
//    sd-bus 函数返回 >= 0 表示成功，< 0 表示负 errno
inline Status makeStatus(int aRet) {
    if (aRet >= 0) return Status(StatusCode::SUCCESS);
    return Status(fromErrno(-aRet));
}

} // namespace RawError

// ③ Adaptor 层所有封装函数直接使用 makeStatus
namespace RawBus {
    Status flushBus(RawBusPtr aBus) {
        if (!aBus) return Status(StatusCode::INVALID_ARG);
        return RawError::makeStatus(sd_bus_flush(aBus));
    }

    Status sendMessage(RawBusPtr aBus, RawBusMessagePtr aMsg,
                       std::string_view aDestination) {
        if (!aBus || !aMsg) return Status(StatusCode::INVALID_ARG);
        return RawError::makeStatus(
            sd_bus_send_to(aBus, aMsg, aDestination.data(), nullptr));
    }
    // ...
}
```

```cpp
// ============ 上层业务代码 ============
// 只与 Status / StatusCode 打交道，完全不碰 errno

// SessionPrivate.hpp
Status setInfo(ServiceInfo aInfo) {
    Status st = Adaptor::RawBus::setUniqueName(mRawBus.get(), aInfo.name, 0);
    if (st.isError()) {
        std::cerr << "setUniqueName failed: " << st.message() << std::endl;
        return st;  // 透传 Adaptor 层返回的 Status，包含精确的 StatusCode
    }
    mInfo = aInfo;
    return Status(StatusCode::SUCCESS);
}

// Method.hpp — callSync
int ret = Adaptor::RawBus::callSync(...);
repMsg.setStatus(Adaptor::RawError::makeStatus(ret));
// ↑ 在 Adaptor 层完成 int→Status 转换，上层直接拿到 Status 对象
```

### 11.4 架构图

```
┌────────────────────────────────────────────────────┐
│  业务层: Session, Method, Reply, ...                │
│  只 #include "Status.hpp"                          │
│  只使用 Status / StatusCode                        │
│  绝不出现 errno / EINVAL / <cerrno>                │
└──────────────┬─────────────────────────────────────┘
               │ 依赖（单向）
               ▼
┌────────────────────────────────────────────────────┐
│  适配层: Adaptor::RawError, Adaptor::RawBus, ...    │
│  #include "Status.hpp" + <cerrno> + <sd-bus.h>    │
│  fromErrno()  — 平台映射（唯一的 errno 消费点）     │
│  makeStatus() — sd-bus 返回值约定 → Status         │
└──────────────┬─────────────────────────────────────┘
               │ 依赖（单向）
               ▼
┌────────────────────────────────────────────────────┐
│  纯类型层: Status.hpp                              │
│  enum class StatusCode { ... }                    │
│  class Status { code(), isSuccess(), ... }        │
│  const char* statusMessage()                      │
│  零平台依赖，零外部宏污染                           │
└────────────────────────────────────────────────────┘
```

### 11.5 替换底层库时的改动对比

| 场景 | 放在 Status.hpp | 放在 Adaptor 层 |
|---|---|---|
| 从 sd-bus → GDBus | 改 `Status.hpp`（影响所有文件重编译） | 只改 `RawAdaptor.hpp` 中的 `fromErrno()` |
| 从 Linux → Windows | `Status.hpp` 编译失败（缺少 E* 宏） | 改 `fromErrno()`，用 Windows 错误码替代 |
| 新增自定义错误码 | 在 `StatusCode` enum 加一项 + `fromErrno` 加映射 | 完全相同，但改动点在两个文件、职责清晰 |
| 单测构造错误场景 | `fromErrno(EINVAL)` — 需要真实 errno | `Status(StatusCode::INVALID_ARG)` — 直接构造 |

### 11.6 设计原则：依赖方向由不稳定指向稳定

```
不稳定（易变）                      稳定（不变）
┌──────────────┐     依赖      ┌──────────────┐
│ 平台适配层     │ ──────────► │  纯类型层     │
│ <cerrno>     │              │  StatusCode   │
│ sd-bus API   │              │  Status       │
└──────────────┘              └──────────────┘
```

- `<cerrno>` 和 sd-bus API 可能在换平台/换库时变化 → **不稳定**
- `StatusCode` 是项目内部语义定义，由项目自己控制 → **稳定**
- 依赖方向必须从不稳定指向稳定（依赖倒置原则的体现）
- 如果反向（稳定依赖不稳定），则平台/库的每次变化都会震到核心类型层

---

## 12. 逗号折叠表达式读取 tuple 时状态丢失

### 12.1 问题

```cpp
template<typename... Args>
Status read(std::tuple<Args...>& aVals) {
    Status status;
    [&]<size_t... Idx>(std::index_sequence<Idx...>) {
        ((status = read(std::get<Idx>(aVals))), ...);
    }(std::make_index_sequence<sizeof...(Args)>{});
    return status;  // ❌ 只返回最后一个元素的结果
}
```

逗号折叠 `,` 会依次执行每个元素，但 `status` 每次都被覆盖——如果第 1 个元素失败，第 2 个成功，最终返回 `SUCCESS`，第一个错误被静默吞掉。

### 12.2 解决方案：逻辑与折叠（短路）

```cpp
template<typename... Args>
Status read(std::tuple<Args...>& aVals) {
    Status status;
    [&]<size_t... Idx>(std::index_sequence<Idx...>) {
        ((status = read(std::get<Idx>(aVals))).isSuccess() && ...);
    }(std::make_index_sequence<sizeof...(Args)>{});
    return status;  // ✅ 第一个失败时停止，保留其错误
}
```

`&&` 折叠会短路——第一个 `isSuccess() == false` 之后不再继续读后续元素，`status` 保留第一个遇到的错误。

### 12.3 与可变参数版本的对比

可变参数版使用了 early return，没有这个问题：

```cpp
template<typename First, typename... Rests>
Status read(First& aFirst, Rests&... aRests) {
    auto res = read(aFirst);
    if (res.isError()) return res;  // ← early return，不读后续
    return read(aRests...);
}
```

tuple 版需要显式用 `&&` 折叠来达到相同效果。

---

## 13. listenSignal 的 void* 回调传递与修正

### 13.1 初始问题

`listenSignal` 需要对接 sd-bus 的 C API（`sd_bus_match_signal`），其回调签名是：

```cpp
int (*handler)(sd_bus_message*, void* userdata, sd_bus_error*);
```

这是一个**纯函数指针**，不能捕获 lambda。用户数据通过 `void*` 传递。

### 13.2 最初尝试及其 bug

```cpp
// ❌ 版本 A：模板 + 直接传 callback 对象
template<typename Callback>
Status listenSignal(..., Callback&& aCallback) {
    sd_bus_match_signal(..., handler, aCallback);  // ❌ Callback&& 不能隐式转 void*
}

// ❌ 版本 B：改为普通函数指针
Status listenSignal(..., void(*aCallback)(MessagePrivate&)) {
    sd_bus_match_signal(..., handler, &aCallback);    // ❌ 传了局部变量的地址
    // 函数返回后 &aCallback 是悬空指针！
}
```

**三个关键 bug：**

| Bug | 描述 |
|-----|------|
| `&aCallback` 悬空 | `aCallback` 是栈上的参数，函数返回后地址失效，信号到达时访问 → UB |
| `aUsr` 解引用方式错误 | 传 `&aCallback`（二级指针），lambda 里当一级指针 cast |
| 未调用 callback | lambda cast 完就 `return 0`，callback 从未执行 |

### 13.3 正确做法：传函数指针本身

```cpp
// ✅ 对于普通函数指针
Status listenSignal(..., void(*aCallback)(MessagePrivate&)) {
    sd_bus_match_signal(...,
        [] (RawBusMessagePtr aMsg, void* aUsr, RawBusErrorPtr) -> int {
            auto callback = reinterpret_cast<void(*)(MessagePrivate&)>(aUsr);
            MessagePrivate message(RawMessageSharePtr(aMsg, false));
            callback(message);    // ✅ 实际调用
            return 0;
        },
        reinterpret_cast<void*>(aCallback)  // ✅ 传函数指针值，不是其地址
    );
}
```

**原理**：reinterpret_cast 在 POSIX 系统上可以在 `void*` 和函数指针之间互转。函数本身在 `.text` 段，地址恒有效，不存在生命周期问题。

---

## 14. function_traits — 编译期萃取可调用对象的参数类型

### 14.1 动机

用户调用 `listenSignal` 时不希望手动指定信号参数类型：

```cpp
// ❌ 啰嗦
listenSignal<int32_t, std::string>(session, ..., [](int32_t v, std::string s) {});

// ✅ 自动推导
listenSignal(session, ..., [](int32_t v, std::string s) {});
```

需要从 lambda 类型中自动萃取出 `Args... = int32_t, std::string`。

### 14.2 完整实现

```cpp
// ========== 主模板：分发入口 ==========
// 匹配任意类型 F，自动转发到 &F::operator() 的类型
template<typename F>
struct function_traits : function_traits<decltype(&F::operator())> {};

// ========== 终点特化：函数指针 ==========
template<typename Ret, typename... Args>
struct function_traits<Ret(*)(Args...)> {
    using return_type = Ret;
    using args_tuple  = std::tuple<Args...>;
    static constexpr size_t arity = sizeof...(Args);
};

// ========== 中间特化：成员函数指针（剥掉类类型和修饰符） ==========
template<typename C, typename Ret, typename... Args>
struct function_traits<Ret(C::*)(Args...)> : function_traits<Ret(*)(Args...)> {};

template<typename C, typename Ret, typename... Args>
struct function_traits<Ret(C::*)(Args...) const> : function_traits<Ret(*)(Args...)> {};

template<typename C, typename Ret, typename... Args>
struct function_traits<Ret(C::*)(Args...) const noexcept> : function_traits<Ret(*)(Args...)> {};

// ========== 中间特化：std::function ==========
template<typename Ret, typename... Args>
struct function_traits<std::function<Ret(Args...)>> : function_traits<Ret(*)(Args...)> {};
```

### 14.3 推导链路

```
function_traits<decltype(lambda)>
    │  主模板: 取 &lambda::operator() 的类型
    └──▶ function_traits<void (Lambda::*)(int, string) const>
              │  匹配成员函数指针 const 特化 → 剥掉 Lambda:: 和 const
              └──▶ function_traits<void(*)(int, string)>
                        │  终点：产出 args_tuple = tuple<int, string>
```

### 14.4 在 listenSignal 中的使用 — tuple 展开技巧

```cpp
template<typename Callback>
Status listenSignal(..., Callback&& aCallback) {
    using traits = function_traits<std::decay_t<Callback>>;
    using args_tuple = typename traits::args_tuple;

    // 通过标签调度将 tuple 展开回参数包
    auto impl = [&]<typename... Args>(std::tuple<Args...>*) {
        using Handler = SignalHandler<Callback, Args...>;
        // ... 创建 Handler，Args... 可用于 message.read(tuple<Args...>)
    };

    return impl(static_cast<args_tuple*>(nullptr));
}
```

`static_cast<args_tuple*>(nullptr)` 是一个**标签调度**手法：不传实际数据，只传类型信息，触发泛型 lambda 的 `std::tuple<Args...>` 推导，从而把 tuple 里的类型重新展开为模板参数包。

### 14.5 不支持的情况

| 情况 | 原因 |
|---|---|
| 泛型 lambda `[](auto x){}` | `operator()` 是模板函数，`decltype` 无法取 |
| `operator()` 重载的函数对象 | `&F::operator()` 歧义 |
| C 风格可变参数 `(...)` | 模板参数包无法匹配 |

---

## 15. 模板继承 vs 运行时类继承：方向相反

### 15.1 运行时继承：从内到外构造

```
构造顺序:  Parent ──────▶ Child
           (先)           (后)

class Parent { Parent() { /* 1️⃣ */ } };
class Child : Parent { Child() : Parent() { /* 2️⃣ */ } };
```

父类先初始化，子类后初始化。地基 → 房子。

### 15.2 模板继承：从外到内"计算"

```
匹配顺序:  主模板 ──────▶ 中间特化 ──────▶ 终点特化
           (先触发)       (转发)          (最终产出)

function_traits<Lambda>
    │  主模板匹配 → 计算父类 = function_traits<成员函数指针类型>
    └──▶ function_traits<void (Lambda::*)(int) const>
              │  特化匹配 → 计算父类 = function_traits<void(*)(int)>
              └──▶ function_traits<void(*)(int)>  → 最终产出
```

外层的模板**决定**内层（父类）是什么，然后内层再决定更内层。像一个递归类型函数 `f(g(h(x)))`。

### 15.3 本质差异

| | 运行时继承 | 模板继承 |
|---|---|---|
| **方向** | 内 → 外（Parent 先，Child 后） | 外 → 内（外层决定内层的模板参数） |
| **父类是谁** | 写死的 `: Parent` | **计算出来的** `: Traits<transform(T)>` |
| **匹配机制** | 构造函数链 | 模板特化匹配 |
| **比喻** | 盖楼：地基 → 一楼 | 管道：`f(g(h(x)))`，外层决定传给里层什么 |

---
---

## 16. sd_bus vtable 字符串生命周期

### 16.1 核心事实

```c
// sd_bus_add_object_vtable 的 man 文档明确指出：
// "The vtable and all referenced strings must remain valid
//  for the entire lifetime of the bus object — they are NOT copied."
```

sd-bus **只存指针，不拷贝字符串**。vtable 数组和其中引用的所有 string 必须与 slot 同生共死。

### 16.2 错误模式

```cpp
// ❌ 函数内注册，vtable 和字符串全在栈上
void badRegister(sd_bus* bus) {
    sd_bus_slot* slot;
    sd_bus_add_object_vtable(bus, &slot, "/path", "iface",
        (sd_bus_vtable[]){
            SD_BUS_VTABLE_START(0),
            SD_BUS_METHOD("method", "ii", "i", callback, 0),  // "ii"/"i" 是字面量，安全
            SD_BUS_VTABLE_END
        }, userdata);
}   // ← vtable 数组是栈上复合字面量，函数返回后失效
    // slot 仍存活，但指向已释放的栈内存 → use-after-free
```

### 16.3 正确模式：VTableContext 自持字符串

```cpp
struct VTableContext {
    std::unique_ptr<sd_bus_vtable[]> vtable;
    Slot slot;
    std::string func;    // ← SD_BUS_METHOD(func.c_str(), ...) 的生命线
    std::string input;
    std::string output;
};

// commit() 时用 ctx 的字符串构建 vtable：
ctx->func   = std::move(entry.func);
ctx->input  = std::move(entry.input);
ctx->output = std::move(entry.output);

auto item = SD_BUS_METHOD(
    ctx->func.c_str(),      // ← 指向 ctx->func，同生共死
    ctx->input.c_str(),
    ctx->output.c_str(), callback, 0);

ctx->vtable.reset(new sd_bus_vtable[3]{
    SD_BUS_VTABLE_START(0), item, SD_BUS_VTABLE_END});

sd_bus_add_object_vtable(bus, &rawSlot, path, iface, ctx->vtable.get(), data);
```

---

## 17. std::move 后指针悬空

### 17.1 问题

```cpp
std::string src = "hello";
auto* ptr      = src.c_str();  // 指向 src 的内部 buffer
auto  dst      = std::move(src);  // 搬走 src 的内容
// ptr 仍然指向 src 的内部 buffer，但 src 已处于未定义状态
// 对于 std::string，move 后通常是空串或长度 0
```

### 17.2 在 VTable 场景中

```cpp
// ❌ 搬走后 item 的指针指向被搬空的内存
ctx->input  = std::move(entry.input);   // entry.input 被搬空
ctx->output = std::move(entry.output);

// entry.item 中的字符串指针在 move 前指向 entry.input/entry.output
// move 后这些指针悬空
auto item = entry.item;  // ← 内部 c_str() 已悬空
```

### 17.3 正确做法

```cpp
// ✅ 拷贝（或用 ctx 的字符串重新构建 SD_BUS_METHOD）
ctx->input  = entry.input;     // 拷贝，不搬
ctx->output = entry.output;

// 或：先搬，再用 ctx 的字符串重建 vtable 条目
ctx->input  = std::move(entry.input);
auto item   = SD_BUS_METHOD(func, ctx->input.c_str(), ctx->output.c_str(), ...);
                                   // ^^^ 指向 ctx，ctx 常驻
```

---

## 18. SSO 短字符串优化与悬空指针不漏

### 18.1 SSO 原理

`std::string` 对长度 ≤ 15（GCC）的字符串使用栈上内联存储（Small String Optimization）：

```cpp
std::string s("ii");   // 长度 2，内联存储在 s 对象内部
auto* ptr = s.c_str(); // 指向 s 内部栈内存
// s 析构后栈内存被回收，但数据不会立即被覆写
```

### 18.2 为什么 D-Bus 签名常"刚好不炸"

D-Bus 签名如 `"y"`, `"i"`, `"s"`, `"ii"` 全是 1-3 字节，走 SSO。
VTableEntry 析构后，栈上 SSO 缓冲区未被覆写，sd-bus 仍能读到"正确"数据——**纯属侥幸**。

### 18.3 ASAN 能抓到

```bash
$ cmake -DCMAKE_CXX_FLAGS="-fsanitize=address -g" && make
$ ./test
==170299==ERROR: AddressSanitizer: heap-use-after-free
READ of size 9 at 0x51c000000890 thread T0
    #3 /lib/libsystemd.so.0  ← sd-bus 内部读取 vtable 字符串
```

SSO 只是延迟了问题的暴露时间，ASAN 能在任何情况下抓到。

---

## 19. D-Bus method/signal 唯一命名约束

### 19.1 协议限制

D-Bus 规范：同一 `(path, interface, member)` 三元组只能有唯一签名。`sd_bus_add_object_vtable` 对同名注册会返回 `-EEXIST` 或覆盖。

```cpp
// ❌ D-Bus 不支持
.addSignal<int>("clear")     // 签名 i
.addSignal<int, int>("clear") // 签名 ii  ← sd-bus 拒绝/覆盖前一个

// ✅ 必须拆名
.addSignal<int>("clear_one")
.addSignal<int, int>("clear_two")
```

### 19.2 Map key 策略

不要用 `name + "_" + input` 做 key——同名不同签本来就不合法：

```cpp
// ❌ 多余
std::string regName = aName.data() + std::string("_") + input;

// ✅ 直接用 name
std::string regName = aName.data();
```

---

## 20. 匿名类型作为 auto 返回值的 API 隐藏

### 20.1 问题

希望提供链式 API 但不想暴露 `VTableRegistrar` 类型给用户。

### 20.2 方案：内嵌 struct + auto 返回

```cpp
class Session {
    struct RegisterBuilder {      // struct 定义在 Session 内部
        template<typename Func>
        RegisterBuilder& addMethod(std::string_view aName, Func aFunc) { ... }
        Status commit() { ... }
    private:
        // 不暴露的实现细节
    };

public:
    auto registerBuilder() { return RegisterBuilder{...}; }
};
```

### 20.3 效果

```cpp
auto builder = session.registerBuilder();  // 用户拿到的类型是 Session::RegisterBuilder
builder.addMethod("add", ...).commit();    // 只能链式调用，看不到内部
```

| | 放外面 | 放里面（推荐） |
|---|---|---|
| 名称空间 | `::RegisterBuilder` 污染全局 | `Session::RegisterBuilder` 有归属 |
| include | 需额外头文件 | 随 `Session.hpp` 一起 |
| IDE 提示 | 无关类型建议 | 仅在 Session 上下文出现 |

---

## 21. shared_ptr 捕获替代 this 捕获

### 21.1 问题

```cpp
// ❌ 回调中以 [this] 捕获，Session 拷贝/move 后 this 指向错误对象
rep->setCallback([this, ...](Reply<Ret> aRep) {
    mReps.erase(find(mReps.begin(), mReps.end(), repPtr));
    // 如果 Session 被拷贝，this->mReps 是副本的 mReps，repPtr 在旧 Session
});
```

### 21.2 修复：所有回调数据用 shared_ptr 捕获

```cpp
// ✅ mReps 改为 shared_ptr<vector>，lambda 通过 shared_ptr 访问
class Session {
    std::shared_ptr<std::vector<std::shared_ptr<void>>> mRepsPtr;

    void setupCallback(auto& rep) {
        mRepsPtr->push_back(rep);
        rep->setCallback(
            [RepsPtr = mRepsPtr, key = std::shared_ptr<void>(rep),
             cb = std::move(cb)](Reply<Ret> aRep) {
                struct Clear {
                    std::vector<std::shared_ptr<void>>& reps;
                    std::shared_ptr<void> entry;
                    ~Clear() {
                        reps.erase(std::find(reps.begin(), reps.end(), entry));
                    }
                } clear{*RepsPtr, key};
                cb(aRep);
            }
        );
    }
};
```

### 21.3 原则

| 捕获方式 | 安全性 | 适用场景 |
|----------|:------:|---------|
| `[this]` | ❌ 拷贝/move 不安全 | 对象生命周期确定的局部范围 |
| `[shared_ptr]` | ✅ 引用计数保护 | 异步回调、跨生命周期访问 |
| `[weak_ptr]` | ✅ 可检测失效 | 需要知道对象是否已析构的场景 |

### 21.4 weak_ptr 示例：事件循环避免悬空

```cpp
// 场景：Session 可能提前析构，但 EventLoop 仍持有对其的引用
class DbusEventLoop {
    std::weak_ptr<Session> mWeakSession;

public:
    explicit DbusEventLoop(Session& aSession)
        : mWeakSession(aSession.shared_from_this()) {}  // Session 需继承 enable_shared_from_this

    void run() {
        while (true) {
            auto session = mWeakSession.lock();  // 尝试获取 shared_ptr
            if (!session) {
                // Session 已析构，安全退出
                std::cout << "Session destroyed, exiting loop" << std::endl;
                return;
            }

            // session 是 shared_ptr，在此作用域内保证存活
            session->process();
            session->wait(100);
        }
    }
};
```

### 21.5 三者对比

```cpp
// [this] — 最简单的，也最危险
session->onEvent([this] { mData = 42; });
session.reset();  // ❌ this 悬空
// ...

// [shared_ptr] — 安全但不能检测析构
auto self = shared_from_this();
session->onEvent([self] { self->mData = 42; });
session.reset();
// 回调在 self 析构前正常执行 ✅

// [weak_ptr] — 能检测析构并决定执行/跳过
std::weak_ptr<Session> weakSelf = shared_from_this();
session->onEvent([weakSelf] {
    if (auto self = weakSelf.lock()) {
        self->mData = 42;   // ✅ Session 存活，执行
    } else {
        // Session 已析构，跳过 ✅
    }
});
session.reset();
```


---

## 22. 类型萃取：检测 `std::vector`

使用模板偏特化检测一个类型是否为 `std::vector<...>`：

```cpp
template<typename T>
struct isVector : std::false_type {};

template<typename T, typename Alloc>
struct isVector<std::vector<T, Alloc>> : std::true_type {};

template<typename T>
inline constexpr bool isVectorV = isVector<T>::value;
```

偏特化匹配任意 `T` 和 `Alloc`，在 `if constexpr` 中使用：

```cpp
if constexpr (isVectorV<rawType>) {
    using ElemType = typename rawType::value_type;  // vector 原生提供 value_type
    // ...
}
```

> `typename` 不可省略：`rawType::value_type` 是依赖名称，编译器需要 `typename` 明确它是类型。

---

## 23. D-Bus 签名系统设计演变

### 23.1 原始设计（单 `char`）

```cpp
template<typename T> struct DbusTypeSignature { static constexpr char value = '\0'; };
template<> struct DbusTypeSignature<int32_t> { static constexpr char value = 'i'; };
// ...
```

问题：`vector<int>` 的 D-Bus 签名是 `"ai"`（数组+元素类型），单 `char` 无法表达两层信息。之前的 `DbusTypeSignature<vector<T>>::value = 'a'` 丢失了元素类型签名。

### 23.2 最终设计：分离职责

```cpp
// 基础类型 → 单 char（只给底层 sd-bus API 用）
template<typename T> struct BasicSignature { ... };
template<> struct BasicSignature<int32_t> { static constexpr char value = 'i'; };

// 完整签名字符串（含容器递归）
template<typename T>
std::string getSignature() {
    using R = std::decay_t<T>;
    if constexpr (std::is_same_v<R, void>) {
        return "";        // void 返回空串，不是 "\0"
    } else if constexpr (isVectorV<R>) {
        return "a" + getSignature<typename R::value_type>();  // 递归
    } else {
        return std::string(1, BasicSignature<R>::value);
    }
}
```

设计要点：
- `BasicSignature<T>` 只负责基础类型 → 单 `char`，不碰容器
- `getSignature<T>()` 返回完整签名字符串，递归展开 `vector<vector<int>>` → `"aai"`
- 去掉 `DbusTypeSignature`，避免和 `BasicSignature` 混淆
- `PropertyWrapper::BasicSignature()` 改为 `signature()`，避免与 struct 同名

---

## 24. `openContainer` 参数 bug：`char` vs `const char*`

**原始代码（有 bug）**：

```cpp
Status openContainer(RawBusMessagePtr aMsg, char aType, char aInType) {
    sd_bus_message_open_container(aMsg, aType, &aInType);  // &aInType 是单个 char 的地址
}
```

`sd_bus_message_open_container` 的第三个参数是 `const char*`（期望以 `\0` 结尾的 C 字符串如 `"i"`），传入 `&aInType` 是栈上单字符的指针，后面字节不确定——未定义行为。

**修复**：

```cpp
Status openContainer(RawBusMessagePtr aMsg, char aType, const char* aInType) {
    sd_bus_message_open_container(aMsg, aType, aInType);  // 直接传
}
```

调用侧：`openContainer(msg, 'a', getSignature<ElemType>().c_str())` — 用 `.c_str()` 取 `std::string` 的内部 C 字符串。

---

## 25. D-Bus 容器读写模式

### 25.1 写（服务端构造消息）

```cpp
// openContainer → 逐元素 write → closeContainer
st = Adaptor::RawMessage::openContainer(msg, 'a', elemSig.c_str());
for (const auto& elem : vec) {
    st = write(elem);        // 递归调用自身
}
st = Adaptor::RawMessage::closeContainer(msg);
```

### 25.2 读（客户端解析消息）

```cpp
// enterContainer → while(!isEnd) → 逐元素 read → exitContainer
st = Adaptor::RawMessage::enterContainer(msg, 'a', elemSig.c_str());
while (!Adaptor::RawMessage::isEnd(msg, false)) {  // false = 仅本层容器
    ElemType elem{};
    st = read(elem);
    aVal.push_back(std::move(elem));
}
st = Adaptor::RawMessage::exitContainer(msg);
```

关键点：
- `isEnd(msg, false)` 的 `false` 意思是只判断当前容器层级是否结束，不检查外层
- 循环内 `ElemType elem{}` 值初始化，避免残留旧数据
- 用 `std::move` 避免不必要的拷贝

---

## 26. `FuncTrait` 函数萃取（重点）

### 26.1 核心机制

通过模板偏特化从各种可调用对象中提取返回值类型和参数类型：

```cpp
// 主模板：F 如果是 lambda/functor，取其 operator()
template<typename F>
struct FuncTrait : FuncTrait<decltype(&F::operator())> {};

// 函数指针特化
template<typename Ret, typename... Args>
struct FuncTrait<Ret(*)(Args...)> {
    using RetType = Ret;
    using ArgsTuple = std::tuple<Args...>;
    static constexpr size_t argSize = sizeof...(Args);
};

// 成员函数指针 → 继承函数指针的结果
template<typename Cls, typename Ret, typename... Args>
struct FuncTrait<Ret(Cls::*)(Args...)> : FuncTrait<Ret(*)(Args...)> {};

template<typename Cls, typename Ret, typename... Args>
struct FuncTrait<Ret(Cls::*)(Args...) const> : FuncTrait<Ret(*)(Args...)> {};
// ... noexcept 版本同理

// std::function 特化
template<typename Ret, typename... Args>
struct FuncTrait<std::function<Ret(Args...)>> : FuncTrait<Ret(*)(Args...)> {};
```

### 26.2 工作流程（以 lambda 为例）

1. 传入 `Func` = lambda 类型 → 匹配主模板 `FuncTrait<F>`
2. `FuncTrait<F> : FuncTrait<decltype(&F::operator())>` → `decltype(&F::operator())` 是成员函数指针 `Ret(Cls::*)(Args...) const`
3. 成员函数指针匹配 `FuncTrait<Ret(Cls::*)(Args...) const>` → 继承 `FuncTrait<Ret(*)(Args...)>`
4. `FuncTrait<Ret(*)(Args...)>` 提供 `RetType`、`ArgsTuple`、`argSize`

### 26.3 `decltype(&F::operator())` 的关键作用

这是萃取 lambda 类型的"支点"——lambda 没有标准方式直接获取参数类型，但它的 `operator()` 是普通成员函数，`decltype` 能拿到精确的函数指针类型。通过偏特化匹配成员函数指针，就能间接获取 lambda 的 `Ret` 和 `Args...`。

---

## 27. `MethodWrapper` + `FuncTrait` 配合使用

### 27.1 整体结构

```cpp
template<typename Func>
struct MethodWrapper {
    using traits = Method::FuncTrait<std::decay_t<Func>>;
    using ArgsTuple = typename traits::ArgsTuple;
    using Ret = typename traits::RetType;

    static std::string input() {
        // 从 ArgsTuple 展开，得到签名字符串如 "isai"
        auto impl = [&]<typename... Args>(std::tuple<Args...>*) {
            return getArgsString<Args...>();
        };
        return impl(static_cast<ArgsTuple*>(nullptr));
    }
};
```

### 27.2 `static_cast<ArgsTuple*>(nullptr)` 技巧

这是"将类型参数包传递给泛型 lambda"的惯用法：

```cpp
// ArgsTuple = std::tuple<int32_t, std::string, std::vector<int>>
// 无法直接写 impl<int32_t, std::string, std::vector<int>>()
// 所以用空指针标记类型，让编译器从 tuple 推导 Args...
auto impl = [&]<typename... Args>(std::tuple<Args...>*) { ... };
impl(static_cast<std::tuple<int32_t, std::string, std::vector<int>>*>(nullptr));
// 编译器推导出 Args... = int32_t, std::string, std::vector<int>
```

### 27.3 `onCall` 中的 `ArgTypeAdaptor` 作用

```cpp
// 读消息时需要用适配过的类型（如 float→double, string_view→const char*）
std::tuple<typename ArgTypeAdaptor<std::decay_t<Args>>::type...> tpl;
message.read(tpl);  // tpl 类型可能是 tuple<int32_t, double, const char*>

// 调用实际函数时直接 apply 到原始 tpl
std::apply(self->func, tpl);
```

`ArgTypeAdaptor` 确保消息层的读写类型一致（底层 sd-bus 没有 `float`，统一用 `double`），而 `std::apply` 会做隐式转换。

### 27.4 `read` 的 `std::index_sequence` 折叠表达式

```cpp
template<typename... Args>
Status read(std::tuple<Args...>& aVals) {
    Status status;
    [&]<size_t... Idx>(std::index_sequence<Idx...>) {
        ((status = read(std::get<Idx>(aVals))).isSuccess() && ...);
    }(std::make_index_sequence<sizeof...(Args)>{});
    return status;
}
```

这是"短路读取"模式：用 `&&` 折叠，任意一个 `read` 失败后后续不再执行。

---

## 28. 其他注意事项

### 28.1 `string_view::data()` 与 map key

`std::string_view::data()` 不保证 `\0` 结尾。当用作 map 的 key（`map<const char*, ...>`）时存在风险。实际中如果 `string_view` 来自字符串字面量则安全，但不应依赖此行为。

### 28.2 头文件依赖顺序

`MessagePrivate.hpp` 直接使用了 `isVectorV`、`BasicSignature`、`getSignature`，必须显式 `#include "DbusArgs.hpp"`，不能依赖间接引入。否则在其他 include 顺序下编译失败。

### 28.3 `float` 转 `double` 的设计

D-Bus 协议只有 `double` 类型（签名 `'d'`），没有 `float`。所以：
- `BasicSignature<float>::value = 'd'`（和 double 相同）
- `ArgTypeAdaptor<float>::type = double`（读写时统一转 double）
- `MessagePrivate::write` 中对 `float` 显式 `static_cast<double>` 后再 `appendBasic`

### 28.4 分层设计原则

| 层 | 文件 | 职责 |
|---|---|---|
| 底层适配 | `RawAdaptor.hpp` → `RawMessage` | sd-bus C API 的薄封装，只提供原子操作 |
| 高层逻辑 | `MessagePrivate.hpp` → `MessagePrivate` | 类型分发（`if constexpr`），组合底层操作 |

vector 的读写逻辑放在 `MessagePrivate` 而非 `RawAdaptor`，因为它是"组合底层原子操作"的高层逻辑，不应污染适配层。`RawMessage` 只提供 `openContainer`/`closeContainer`/`appendBasic` 等积木。

---

## 29. `isSpecializationOf` 无法匹配 `std::array`：类型参数 vs 非类型参数

### 29.1 问题

尝试用统一的"是否为某模板特化"萃取同时支持 `std::vector` 和 `std::array`：

```cpp
template<typename T, template<typename...> class Template>
struct isSpecializationOf : std::false_type {};

template<template<typename...> class Template, typename... Args>
struct isSpecializationOf<Template<Args...>, Template> : std::true_type {};

// vector 可以
inline constexpr bool isVectorV = isSpecializationOf<T, std::vector>::value;  // ✅

// array 不行
inline constexpr bool isArrayV = isSpecializationOf<T, std::array>::value;    // ❌
```

### 29.2 根因：非类型模板参数

两个容器的模板签名本质不同：

```cpp
template<typename T, typename Alloc>   // 全类型参数
class vector;

template<typename T, std::size_t N>    // 类型 + 非类型参数
class array;
```

`template<typename...>` 只能匹配类型参数包（type parameter pack）。`std::size_t N` 是非类型参数（non-type parameter），不在 `typename...` 的匹配空间内。偏特化 `Template<Args...>` 对 `std::array<int, 3>` 匹配失败 → 始终走 `false_type`。

> C++20 引入 `template<auto...>` 可以混合匹配类型和非类型参数，但 C++17 做不到。

### 29.3 解决方案：独立 trait + 统一入口

放弃万能萃取，回归简单偏特化：

```cpp
// vector — 独立特化
template<typename T>
struct isVector : std::false_type {};
template<typename T, typename Alloc>
struct isVector<std::vector<T, Alloc>> : std::true_type {};
template<typename T>
inline constexpr bool isVectorV = isVector<T>::value;

// array — 独立特化（std::size_t N 是非类型参数，直接匹配）
template<typename T>
struct isArray : std::false_type {};
template<typename T, std::size_t N>
struct isArray<std::array<T, N>> : std::true_type {};
template<typename T>
inline constexpr bool isArrayV = isArray<T>::value;

// 统一入口（给 getSignature / write / read 用）
template<typename T>
inline constexpr bool isContainerV = isVectorV<T> || isArrayV<T>;
```

### 29.4 读写逻辑完全复用

`std::array<T, N>::value_type` = `T`，和 `vector` 一致：

```cpp
// getSignature 中
else if constexpr (isContainerV<R>) {
    return "a" + getSignature<typename R::value_type>();  // 复用
}

// MessagePrivate::write 中
else if constexpr (isContainerV<rawType>) {
    // openContainer / for-elem / closeContainer — 完全相同的逻辑
}
```

### 29.5 设计教训

| 万能萃取 | 独立 trait |
|---|---|
| 一个 template 匹配一切 | 每种容器各一个偏特化 |
| 碰到非类型参数就失效 | `size_t N` 直接写在偏特化里 |
| 抽象泄漏（std::array 需要 C++20 才能统一） | 零抽象，零泄漏 |
| 可删掉 `isSpecializationOf`，它只服务了 1 个场景 | — |