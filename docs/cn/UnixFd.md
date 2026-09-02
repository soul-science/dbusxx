# UnixFd 文件描述符

## 简介

D-Bus 支持在消息中传递 **Unix 文件描述符**（类型签名 `h`）。底层通过 `SCM_RIGHTS` 让内核把打开文件描述符从发送进程送到接收进程——接收端得到一个**指向同一打开文件**的新 fd 号。dbusxx 用 RAII 类型 `UnixFd` 封装它，用起来和普通值一样，但自动管理 `dup`/`close`，避免泄漏与双 close。

`UnixFd` 可直接作为**方法参数/返回值、信号参数**，也可作为**聚合体结构体的成员**（如 `struct { std::string tag; UnixFd data; }` → `(sh)`）。

> ⚠️ **`UnixFd` 不能用作属性（property）类型**。属性值是稳定、可比较、可缓存的数据；fd 每次 `dup`/复制都会产生新 fd、没有稳定的可比较值，语义与之冲突。传 fd 请用方法参数/返回值或信号。

## 类：`UnixFd`

| 方法 | 说明 |
| --- | --- |
| `UnixFd()` | 构造无效 fd（`get()` 返回 -1） |
| `UnixFd(int fd)` | 接管一个已打开的 fd（经 `fcntl(F_GETFD)` 校验有效性，无效记为 `INVALID_ARG`） |
| `UnixFd(const UnixFd&)` | **拷贝 = `dup`**：得到指向同一打开文件的新 fd |
| `UnixFd(UnixFd&&)` | 移动转移；源对象变为无效 |
| `operator=`（拷贝/移动） | 同上语义；先释放自己当前持有的 fd |
| `int get() const` | 底层 fd（无效为 -1） |
| `Status status() const` | fd 是否有效（`SUCCESS` / `INVALID_ARG`） |
| `int release()` | 放弃所有权并返回底层 fd（此后对象无效，不再 `close`） |
| `void reset(int fd = -1)` | 关闭当前 fd 并替换（或清空） |
| `bool operator==(const UnixFd&) const` | 两对象是否持有相同 fd 号 |

## 为什么不用裸 `int` / 直接传 fd？

裸 fd 在 D-Bus 里语义暧昧：发送方 / 接收方 / 消息析构，到底谁负责 `close`？`UnixFd` 把规则固定下来：

- **拷贝 = `dup`**：每次拷贝都产生一个需要单独 `close` 的新 fd，各自析构——绝无双 close；
- **移动 = 所有权转移**：源对象置空，由目标独占；
- **析构 = `close`**：不泄漏。

## API 示例

```cpp
#include <unistd.h>      // pipe / write / dup
#include <utility>       // std::move
#include <vector>        // std::vector<UnixFd>

#include <dbusxx/UnixFd.hpp>
#include <dbusxx/Client.hpp>
#include <dbusxx/Reply.hpp>

using namespace Dbusxx;

// ── 服务端 ───────────────────────────────────────────────
// 用 Server<Derived> 暴露"原样回传 fd"的方法（签名 h）：
// class MyServer : public Server<MyServer> {
// public:
//     MyServer() : Server("com.example.Svc") {}
//     DBUSXX_PATH("/svc")
//     DBUSXX_IFACE("com.example.Iface")
//     UnixFd echoFd(UnixFd fd) { return fd; }           // DBUSXX_METHOD(echoFd)
//     std::vector<UnixFd> echoFdList(std::vector<UnixFd> fds) { return fds; }
//                                                        // DBUSXX_METHOD(echoFdList) → ah
// };
// MyServer server;
// std::thread serverThread([&] { server.run(); });       // 事件循环线程

// ── 客户端（自管：内部自带 Session + 事件循环线程）───────
Client client(SessionType::USER, "com.example.Svc",
              "/svc", "com.example.Iface");

// 把管道读端发给服务端并收回
int pipefd[2];
::pipe(pipefd);

UnixFd in(pipefd[0]);                                  // 接管读端（析构时 close）
auto rep = client.callSync<UnixFd>("echoFd", in);      // 往返，拿到服务端回传的 fd
if (rep.isError()) { /* 处理错误 */ }

// rep.value() 返回的是"借用"的 UnixFd（随 rep 析构而 close）——
// 需要长期持有就拷贝一份：拷贝 = dup，各持一 fd、独立 close。
UnixFd owned = rep.value();
::write(owned.get(), "hi", 2);                          // 用底层 fd 读写该管道

// 批量传递：std::vector<UnixFd>（签名 ah）
UnixFd in2(::dup(owned.get()));
auto repBatch = client.callSync<std::vector<UnixFd>>(
    "echoFdList", std::vector<UnixFd>{ std::move(owned), std::move(in2) });

// 超出作用域：UnixFd / rep 各自析构 close，无需手动管理；
// 已交给 UnixFd 的 fd 请勿再手动 close（需要独占时用 release() 取回）。
```

> 注意：fd 一旦交给 `UnixFd`（或写进消息），其生命周期就由 `UnixFd`/D-Bus 管理；不要手动 `close` 已接管的 fd，除非先用 `release()` 取回所有权。
