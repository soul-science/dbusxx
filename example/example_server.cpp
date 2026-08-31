/****************************************************************************
 * example_server.cpp
 * 展示 Server<Derived> 完整用法:
 *   同线程 emit / 跨线程 emit / 同步调用 / 异步调用 / 信号监听
 *
 * 架构: DemoServer(服务端线程) ←→ Session(客户端, main 线程)
 ****************************************************************************/

#include "Server.hpp"
#include "Session.hpp"
#include "Reply.hpp"
#include "PendingReply.hpp"
#include "UnixFd.hpp"

#include <array>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <fcntl.h>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unistd.h>
#include <vector>

using namespace Dbusxx;

// ── 服务类 ────────────────────────────────────────────────────────────────

class DemoServer : public Server<DemoServer> {
public:
    DemoServer()
        : Server("com.example.demo") {}

    // ─ 基础类型方法 (echo 回显) ─

    DBUSXX_PATH("/com/example/demo")
    DBUSXX_IFACE("com.example.Demo")
    int8_t testInt8(int8_t i) {
        std::cout << "[server] testInt8: " << +i << std::endl;
        return i;
    }
    DBUSXX_METHOD(testInt8)

    uint8_t testUint8(uint8_t i) {
        std::cout << "[server] testUint8: " << +i << std::endl;
        return i;
    }
    DBUSXX_METHOD(testUint8)

    int16_t testInt16(int16_t i) {
        std::cout << "[server] testInt16: " << i << std::endl;
        return i;
    }
    DBUSXX_METHOD(testInt16)

    uint16_t testUint16(uint16_t i) {
        std::cout << "[server] testUint16: " << i << std::endl;
        return i;
    }
    DBUSXX_METHOD(testUint16)

    int32_t testInt32(int32_t i) {
        std::cout << "[server] testInt32: " << i << std::endl;
        return i;
    }
    DBUSXX_METHOD(testInt32)

    uint32_t testUint32(uint32_t i) {
        std::cout << "[server] testUint32: " << i << std::endl;
        return i;
    }
    DBUSXX_METHOD(testUint32)

    int64_t testInt64(int64_t i) {
        std::cout << "[server] testInt64: " << i << std::endl;
        return i;
    }
    DBUSXX_METHOD(testInt64)

    uint64_t testUint64(uint64_t i) {
        std::cout << "[server] testUint64: " << i << std::endl;
        return i;
    }
    DBUSXX_METHOD(testUint64)

    float testFloat(float i) {
        std::cout << "[server] testFloat: " << i << std::endl;
        return i;
    }
    DBUSXX_METHOD(testFloat)

    double testDouble(double i) {
        std::cout << "[server] testDouble: " << i << std::endl;
        return i;
    }
    DBUSXX_METHOD(testDouble)

    bool testBool(bool i) {
        std::cout << "[server] testBool: " << (i ? "true" : "false") << std::endl;
        return i;
    }
    DBUSXX_METHOD(testBool)

    const char* testConstChars(const char* i) {
        std::cout << "[server] testConstChars: " << i << std::endl;
        return i;
    }
    DBUSXX_METHOD(testConstChars)

    std::string testString(const std::string& i) {
        std::cout << "[server] testString: " << i << std::endl;
        return i;
    }
    DBUSXX_METHOD(testString)

    std::string_view testStringView(std::string_view i) {
        std::cout << "[server] testStringView: " << i << std::endl;
        return i;
    }
    DBUSXX_METHOD(testStringView)

    void testVoid() {
        std::cout << "[server] testVoid" << std::endl;
    }
    DBUSXX_METHOD(testVoid)

    void testVector(std::vector<int> v) {
        std::cout << "[server] testVector: ";
        for (const auto& item : v) std::cout << item << " ";
        std::cout << std::endl;
    }
    DBUSXX_METHOD(testVector)

    void testMultiArgs(int i, std::string s, std::vector<double> v) {
        std::cout << "[server] testMultiArgs: i=" << i << ", s=" << s;
        std::cout << ", v=[ ";
        for (const auto& item : v) std::cout << item << " ";
        std::cout << "]" << std::endl;
    }
    DBUSXX_METHOD(testMultiArgs)

    // ─ Unix fd 方法 ─

    // echo 回传收到的 fd（move-only 出入参，签名 'h'）
    UnixFd echoFd(UnixFd fd) {
        std::cout << "[server] echoFd: fd=" << fd.get() << std::endl;
        return fd;
    }
    DBUSXX_METHOD(echoFd)

    // 批量 fd：std::vector<UnixFd> → 'ah'
    std::vector<UnixFd> echoFdList(std::vector<UnixFd> fds) {
        std::cout << "[server] echoFdList: size=" << fds.size() << std::endl;
        return fds;
    }
    DBUSXX_METHOD(echoFdList)

    // ─ Map 读写方法 ─

    // 读取 map: echo 回显 int map
    std::map<std::string, int32_t> testMapRead(
        const std::map<std::string, int32_t>& m) {
        std::cout << "[server] testMapRead: { ";
        for (const auto& [k, v] : m)
            std::cout << k << ":" << v << " ";
        std::cout << "}" << std::endl;
        return m;
    }
    DBUSXX_METHOD(testMapRead)

    // 写入 map: 接收 string→string map
    void testMapWrite(const std::map<std::string, std::string>& m) {
        std::cout << "[server] testMapWrite: { ";
        for (const auto& [k, v] : m)
            std::cout << k << ":\"" << v << "\" ";
        std::cout << "}" << std::endl;
    }
    DBUSXX_METHOD(testMapWrite)

    // 嵌套 map: map<string, vector<int>>
    void testMapNested(
        const std::map<std::string, std::vector<int>>& m) {
        std::cout << "[server] testMapNested: { ";
        for (const auto& [k, v] : m) {
            std::cout << k << ":[";
            for (auto x : v) std::cout << x << ",";
            std::cout << "] ";
        }
        std::cout << "}" << std::endl;
    }
    DBUSXX_METHOD(testMapNested)

    // ─ 信号触发方法 ─

    // 同线程 emit: 由 D-Bus 调用触发，在事件循环线程内执行
    void triggerClear(int a, int b) {
        std::cout << "[server] triggerClear(" << a << ", " << b
                  << ") → emit from event loop thread" << std::endl;
        Status st = emit("/com/example/demo","com.example.Demo",
            "clear", a, b);
        std::cout << "[server] emit result: " << st.message() << std::endl;
    }
    DBUSXX_METHOD(triggerClear)

    // 触发携带 fd 的信号（fdReady）
    void triggerFdSignal(UnixFd fd) {
        std::cout << "[server] triggerFdSignal: fd=" << fd.get() << std::endl;
        //! fd 是 move-only，跨线程 emit 需要把所有权移进信号参数
        Status st = emit("/com/example/demo","com.example.Demo",
            "fdReady", std::move(fd));
        if (st.isError()) {
            //! 发送失败必须显式暴露：避免信号丢失/fd 状态被静默吞没
            std::cout << "[server] emit fdReady FAILED: "
                      << st.message() << std::endl;
            return;
        }
        std::cout << "[server] emit fdReady result: " << st.message() << std::endl;
    }
    DBUSXX_METHOD(triggerFdSignal)

    // ─ 关闭服务 ─

    void shutdown() {
        std::cout << "[server] shutdown → stopping looper..." << std::endl;
        stop();
    }
    DBUSXX_METHOD(shutdown)

    // ─ 信号声明 ─

    DBUSXX_SIGNAL(clear, int, int)

    // 携带 UnixFd 的信号（签名 'h'）
    DBUSXX_SIGNAL(fdReady, UnixFd)

    // ─ 属性 ─
    // RO: 只读属性，外部只能 Get 不能 Set
    DBUSXX_PROPERTY_RO(serverName, std::string, std::string("demo-server"))
    // RW: 读写属性，Get / Set 均可
    DBUSXX_PROPERTY_RW(version, int32_t, 1)
    DBUSXX_PROPERTY_RW(description, std::string, std::string("demo"))

    //! 注册本地属性变更监听（供外部 main 调用）
    void listenVersion() {
        Status st = session().onLocalPropertyChanged<int32_t>(
            "/com/example/demo","com.example.Demo","version",
            [this](const int32_t& v) {
                std::cout << "[server] version changed: " << v << std::endl;
                mVerChanged = true;
                mNewVer = v;
            });
        if (st.isError()) {
            std::cout << "[server] listenVersion FAILED: " << st.message() << std::endl;
        }
    }
    bool mVerChanged { false };
    int32_t mNewVer { 0 };
};

// ── C++17 线程同步辅助（替代 C++20 atomic::wait）──────────────────────

struct SyncFlag {
    void set() {
        {
            std::lock_guard<std::mutex> lock(m);
            flag = true;
        }
        cv.notify_one();
    }

    void wait() {
        std::unique_lock<std::mutex> lock(m);
        cv.wait(lock, [this] { return flag; });
    }

    void reset() {
        std::lock_guard<std::mutex> lock(m);
        flag = false;
    }

    bool get() const { return flag; }

private:
    std::mutex m;
    std::condition_variable cv;
    bool flag { false };
};

// ── 测试宏 ────────────────────────────────────────────────────────────────

static int gPassed = 0;
static int gFailed = 0;

#define TEST(name, expr)                                                      \
    do {                                                                      \
        std::cout << "  [" << (name) << "] ";                                 \
        if (expr) {                                                           \
            std::cout << "PASSED";                                            \
            ++gPassed;                                                        \
        } else {                                                              \
            std::cout << "FAILED";                                            \
            ++gFailed;                                                        \
        }                                                                     \
        std::cout << std::endl;                                               \
    } while (0)

// ⚠️ fdReady 信号监听是永久的（存活到 asyncClient 销毁），回调捕获对象必须
//    覆盖整个生命周期，故用文件级 static（与 clear 信号的 main 顶层变量同理）。
static SyncFlag gFdSignalFlag;
static bool gFdSignalReadOk = false;
static int gFdSignalFd = -1;

// ── main ──────────────────────────────────────────────────────────────────

int main() {
    DemoServer server;

    // 捕获 SIGINT
    std::signal(SIGINT, [](int) {
        std::cout << "\n[SIGINT] exiting..." << std::endl;
        _exit(0);
    });

    // ① 在子线程启动服务
    std::cout << "=== Step 1: Starting server in background thread ==="
              << std::endl;
    std::thread serverThread([&server] {
        server.run();
    });

    // 等待服务就绪（服务注册在 run() 内同步完成，sleep 确保线程已启动）
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::cout << "[main] server should be ready." << std::endl;

    // ② 创建客户端 Session
    //  syncClient: 纯 callSync，不绑事件循环，避免与 sd_bus_call 竞争 fd
    //  asyncClient: 绑事件循环，用于收信号 + callAsync
    std::cout << "\n=== Step 2: Creating client sessions ===" << std::endl;
    Session syncClient = Session::userSession();
    Session asyncClient = Session::userSession();

    const char* svc  = "com.example.demo";
    const char* path = "/com/example/demo";
    const char* iface = "com.example.Demo";

    // ③ 同步调用 — 基础类型
    std::cout << "\n=== Step 3: Sync calls (basic types) ===" << std::endl;
    {
        auto r = syncClient.callSync<int32_t>(
            svc, path, iface, "testInt32", 42);
        TEST("testInt32 echo", !r.isError() && r.value() == 42);
    }
    {
        auto r = syncClient.callSync<std::string>(
            svc, path, iface, "testString",
            std::string("hello"));
        TEST("testString echo", !r.isError() && r.value() == "hello");
    }
    {
        auto r = syncClient.callSync<bool>(
            svc, path, iface, "testBool", true);
        TEST("testBool echo", !r.isError() && r.value() == true);
    }
    {
        auto r = syncClient.callSync<double>(
            svc, path, iface, "testDouble", 3.14);
        TEST("testDouble echo", !r.isError() && r.value() == 3.14);
    }
    {
        auto r = syncClient.callSync<int64_t>(
            svc, path, iface, "testInt64",
            int64_t(-1234567890123LL));
        TEST("testInt64 echo",
             !r.isError() && r.value() == int64_t(-1234567890123LL));
    }
    {
        auto r = syncClient.callSync(svc, path, iface, "testVoid");
        TEST("testVoid no-error", !r.isError());
    }

    // ④ 同步调用 — 复合类型
    std::cout << "\n=== Step 4: Sync calls (compound types) ===" << std::endl;
    {
        auto r = syncClient.callSync(svc, path, iface, "testVector",
                                     std::vector<int>{1, 2, 3});
        TEST("testVector", !r.isError());
    }
    {
        auto r = syncClient.callSync(svc, path, iface, "testMultiArgs",
                                     100, std::string("multi"),
                                     std::vector<double>{1.1, 2.2});
        TEST("testMultiArgs", !r.isError());
    }

    // ④.⑤ Map 读写测试
    std::cout << "\n=== Step 4.5: Map read/write ===" << std::endl;
    {
        // 读取 map: echo 回显
        std::map<std::string, int32_t> inputMap{
            {"score", 100}, {"rank", 3}, {"count", 42}};
        auto r = syncClient.callSync<std::map<std::string, int32_t>>(
            svc, path, iface, "testMapRead", inputMap);
        TEST("testMapRead echo", !r.isError() && r.value() == inputMap);

        // 写入 map
        std::map<std::string, std::string> strMap{
            {"key1", "val1"}, {"key2", "val2"}};
        auto r2 = syncClient.callSync(
            svc, path, iface, "testMapWrite", strMap);
        TEST("testMapWrite", !r2.isError());

        // 嵌套 map
        std::map<std::string, std::vector<int>> nestedMap{
            {"a", {1, 2, 3}}, {"b", {4, 5}}};
        auto r3 = syncClient.callSync(
            svc, path, iface, "testMapNested", nestedMap);
        TEST("testMapNested", !r3.isError());
    }

    // ④.⑥ 远程属性读取 — getRemoteProperty (RO + RW)
    std::cout << "\n=== Step 4.6: getRemoteProperty (RO + RW) ===" << std::endl;
    {
        // RO: serverName
        auto r = syncClient.getRemoteProperty<std::string>(
            svc, path, iface, "serverName");
        TEST("getRemoteProperty RO string",
            !r.isError() && r.value() == "demo-server");

        // RW: version
        auto r2 = syncClient.getRemoteProperty<int32_t>(
            svc, path, iface, "version");
        TEST("getRemoteProperty RW int32_t",
            !r2.isError() && r2.value() == 1);

        // RW: description
        auto r3 = syncClient.getRemoteProperty<std::string>(
            svc, path, iface, "description");
        TEST("getRemoteProperty RW string",
            !r3.isError() && r3.value() == "demo");

        // 不存在的属性
        auto r4 = syncClient.getRemoteProperty<int32_t>(
            svc, path, iface, "nonexistent");
        TEST("getRemoteProperty nonexistent", r4.isError());
    }

    // ④.⑦ 远程属性写入 — setRemoteProperty (RW 成功, RO 失败)
    std::cout << "\n=== Step 4.7: setRemoteProperty (RW OK, RO fail) ===" << std::endl;
    {
        // RW: 写入 version
        auto st = syncClient.setRemoteProperty<int32_t>(
            svc, path, iface, "version", 99);
        TEST("setRemoteProperty RW int32_t", st.isSuccess());
        auto r = syncClient.getRemoteProperty<int32_t>(
            svc, path, iface, "version");
        TEST("getRemoteProperty after set RW",
            !r.isError() && r.value() == 99);

        // RW: 写入 description
        auto st2 = syncClient.setRemoteProperty<std::string>(
            svc, path, iface, "description", std::string("updated"));
        TEST("setRemoteProperty RW string", st2.isSuccess());
        auto r2 = syncClient.getRemoteProperty<std::string>(
            svc, path, iface, "description");
        TEST("getRemoteProperty after set RW",
            !r2.isError() && r2.value() == "updated");

        // RO: 尝试写入 serverName 应失败
        auto st3 = syncClient.setRemoteProperty<std::string>(
            svc, path, iface, "serverName", std::string("hack"));
        TEST("setRemoteProperty RO should fail", st3.isError());

        // RO: 确认 serverName 未被修改
        auto r3 = syncClient.getRemoteProperty<std::string>(
            svc, path, iface, "serverName");
        TEST("getRemoteProperty RO unchanged",
            !r3.isError() && r3.value() == "demo-server");

        // 不存在的属性
        auto st4 = syncClient.setRemoteProperty<int32_t>(
            svc, path, iface, "nonexistent", 42);
        TEST("setRemoteProperty nonexistent", st4.isError());
    }

    // ④.⑧ 本地属性变更监听（服务端侧）— 旧 session() API
    std::cout << "\n=== Step 4.8: Local property change listener ===" << std::endl;
    {
        server.listenVersion();

        auto st = syncClient.setRemoteProperty<int32_t>(
            svc, path, iface, "version", 77);
        TEST("setRemoteProperty trigger", st.isSuccess());

        // 等待属性变更回调（在事件循环线程中执行）
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        TEST("onLocalPropertyChanged fired",
            server.mVerChanged && server.mNewVer == 77);
    }

    // ④.⑨ 服务端直接读写属性 — Server::getProperty / setProperty / onPropertyChanged
    std::cout << "\n=== Step 4.9: Server direct property R/W ===" << std::endl;
    {
        // 读
        int32_t v = 0;
        TEST("Server::getProperty version",
            server.getProperty("/com/example/demo",
                "com.example.Demo", "version", v).isSuccess()
                && v == 77);

        std::string desc;
        TEST("Server::getProperty description",
            server.getProperty("/com/example/demo",
                "com.example.Demo", "description", desc).isSuccess()
                && desc == "updated");

        // 写
        TEST("Server::setProperty description",
            server.setProperty("/com/example/demo",
                "com.example.Demo", "description",
                std::string("server-side-set")).isSuccess());

        // 验证
        TEST("Server::getProperty after set",
            server.getProperty("/com/example/demo",
                "com.example.Demo", "description", desc).isSuccess()
                && desc == "server-side-set");

        // 注册变更监听
        SyncFlag srvPropFlag;
        std::string newDesc;
        TEST("Server::onPropertyChanged register",
            server.onPropertyChanged<std::string>(
                "/com/example/demo", "com.example.Demo", "description",
                [&](const std::string& val) {
                    newDesc = val;
                    srvPropFlag.set();
                }).isSuccess());

        // 通过远端修改触发
        Status stRemote = syncClient.setRemoteProperty<std::string>(
            svc, path, iface, "description", std::string("remote-set"));
        TEST("Server::setRemoteProperty trigger", stRemote.isSuccess());
        if (stRemote.isSuccess()) {
            srvPropFlag.wait();
            TEST("Server::onPropertyChanged fired", newDesc == "remote-set");
        }
    }

    // ⑤ 信号监听 — 在事件循环启动前注册（避免线程竞争）
    std::cout << "\n=== Step 5: Signal listening ===" << std::endl;
    SyncFlag signalReceived;
    int sigA = 0, sigB = 0;

    Status stListen = asyncClient.listenSignal(
        svc, path, iface, "clear",
        [&](int a, int b) {
            std::cout << "  [client] signal 'clear' received: a=" << a
                    << ", b=" << b << std::endl;
            sigA = a;
            sigB = b;
            signalReceived.set();
        });
    TEST("listenSignal clear", stListen.isSuccess());

    // fdReady 信号监听（与 clear 一样，须在事件循环启动前注册）
    Status stFd = asyncClient.listenSignal(
        svc, path, iface, "fdReady",
        [](UnixFd fd) {
            std::cout << "  [client] signal 'fdReady' received: fd="
                      << fd.get() << std::endl;
            char buf[8] = {0};
            gFdSignalReadOk = (::read(fd.get(), buf, 5) == 5
                && std::string(buf) == "hello");
            gFdSignalFd = fd.get();
            gFdSignalFlag.set();
        });
    TEST("listenSignal fdReady", stFd.isSuccess());

    // ⑥ 启动客户端事件循环
    std::cout << "\n=== Step 6: Starting client event loop ===" << std::endl;
    Looper clientLooper(asyncClient);
    std::thread clientThread([&clientLooper] {
        clientLooper.run();
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // ⑦ 同线程 emit — 用 syncClient 调用（无事件循环，不会竞争）
    std::cout << "\n=== Step 7: In-thread emit (via D-Bus method) ==="
              << std::endl;
    {
        signalReceived.reset();
        auto r = syncClient.callSync(
            svc, path, iface, "triggerClear", 7, 8);
        TEST("triggerClear call", !r.isError());

        signalReceived.wait();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        TEST("signal received", signalReceived.get());
        TEST("signal args match", sigA == 7 && sigB == 8);
    }

    // ⑧ 跨线程 emit — main 线程直接调 server.emit()
    std::cout << "\n=== Step 8: Cross-thread emit (from main thread) ==="
              << std::endl;
    {
        signalReceived.reset();

        // server.emit() 自动检测到不是事件循环线程，走 post 路径
        Status stEmit = server.emit("/com/example/demo","com.example.Demo",
            "clear", 99, 100);
        TEST("cross-thread emit posted", stEmit.isSuccess());

        signalReceived.wait();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        TEST("cross-thread signal received", signalReceived.get());
        TEST("cross-thread args match", sigA == 99 && sigB == 100);
    }

    // ⑨ 异步调用测试
    std::cout << "\n=== Step 9: Async call ===" << std::endl;
    {
        SyncFlag asyncDone;
        std::string asyncResult;

        (void)asyncClient.callAsync<std::string>(
            svc, path, iface, "testString",
            [&](Reply<std::string> rep) {
                asyncResult = rep.value();
                asyncDone.set();
            },
            std::string("async-hello"));

        asyncDone.wait();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        TEST("async call received", asyncDone.get());
        TEST("async call echo", asyncResult == "async-hello");
    }

    // ⑨.⑤ Unix fd 传输测试（方法 + 信号）
    std::cout << "\n=== Step 9.5: Unix fd passing ===" << std::endl;
    std::cout << "  sig UnixFd         = " << getSignature<UnixFd>() << std::endl;
    std::cout << "  sig vector<UnixFd> = "
              << getSignature<std::vector<UnixFd>>() << std::endl;
    {
        // 单 fd 往返：pipe 读端 → echoFd → 返回一个新 fd（'h'）
        int pipefd[2];
        if (::pipe(pipefd) == 0) {
            // 读端设为非阻塞，避免测试失败时 read 挂死
            ::fcntl(pipefd[0], F_SETFL,
                ::fcntl(pipefd[0], F_GETFL, 0) | O_NONBLOCK);

            auto r = syncClient.callSync<UnixFd>(
                svc, path, iface, "echoFd", UnixFd(pipefd[0]));
            TEST("echoFd call ok", !r.isError());
            TEST("echoFd returns a new fd", !r.isError()
                && r.value().get() >= 0 && r.value().get() != pipefd[0]);

            // 功能验证：向写端写，从返回的 fd 读出（证明指向同一 pipe）
            TEST("echoFd write", ::write(pipefd[1], "hello", 5) == 5);
            char buf[8] = {0};
            TEST("echoFd read via returned fd",
                ::read(r.value().get(), buf, 5) == 5
                && std::string(buf) == "hello");

            ::close(pipefd[1]);
        } else {
            TEST("echoFd pipe create", false);
        }
    }
    {
        // 批量 fd 往返：两个 pipe 读端 → echoFdList → 返回两个新 fd（'ah'）
        int pa[2], pb[2];
        if (::pipe(pa) == 0 && ::pipe(pb) == 0) {
            ::fcntl(pa[0], F_SETFL, ::fcntl(pa[0], F_GETFL, 0) | O_NONBLOCK);
            ::fcntl(pb[0], F_SETFL, ::fcntl(pb[0], F_GETFL, 0) | O_NONBLOCK);

            std::vector<UnixFd> in;
            in.push_back(UnixFd(pa[0]));
            in.push_back(UnixFd(pb[0]));
            auto r = syncClient.callSync<std::vector<UnixFd>>(
                svc, path, iface, "echoFdList", in);
            TEST("echoFdList ok", !r.isError() && r.value().size() == 2);
            TEST("echoFdList distinct fds", !r.isError()
                && r.value()[0].get() >= 0 && r.value()[1].get() >= 0
                && r.value()[0].get() != r.value()[1].get());

            // 功能验证：往 pa 写端写，从返回的 fd[0] 读出
            TEST("echoFdList write", ::write(pa[1], "AAA", 3) == 3);
            char buf[4] = {0};
            TEST("echoFdList read via returned fd",
                ::read(r.value()[0].get(), buf, 3) == 3
                && std::string(buf) == "AAA");

            ::close(pa[1]);
            ::close(pb[1]);
        } else {
            TEST("echoFdList pipe create", false);
        }
    }
    // fdReady 信号：服务端 emit fdReady，客户端回调内读取
    gFdSignalFlag.reset();
    gFdSignalReadOk = false;
    gFdSignalFd = -1;

    int pipefd[2];
    if (::pipe(pipefd) == 0) {
        ::fcntl(pipefd[0], F_SETFL,
            ::fcntl(pipefd[0], F_GETFL, 0) | O_NONBLOCK);

        TEST("fdSignal write", ::write(pipefd[1], "hello", 5) == 5);
        auto r = syncClient.callSync(
            svc, path, iface, "triggerFdSignal", UnixFd(pipefd[0]));
        TEST("triggerFdSignal call", !r.isError());

        gFdSignalFlag.wait();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        TEST("fdReady signal received", gFdSignalFlag.get());
        TEST("fdReady fd valid", gFdSignalFd >= 0);
        TEST("fdReady data read inside callback", gFdSignalReadOk);
        ::close(pipefd[1]);
    } else {
        TEST("fdSignal pipe create", false);
    }

    // ⑩ 关闭服务 — 用 syncClient（无事件循环，不会竞争）
    std::cout << "\n=== Step 10: Shutdown ===" << std::endl;
    {
        auto r = syncClient.callSync(svc, path, iface, "shutdown");
        TEST("shutdown call", !r.isError());
    }

    serverThread.join();
    std::cout << "[main] server thread joined." << std::endl;

    // 关闭客户端事件循环
    clientLooper.stop();
    clientThread.join();
    std::cout << "[main] client thread joined." << std::endl;

    // ── 测试结果汇总 ──
    std::cout << "\n========================================" << std::endl;
    std::cout << "  Results: " << gPassed << " passed, "
              << gFailed << " failed" << std::endl;
    std::cout << "========================================" << std::endl;

    return gFailed > 0 ? 1 : 0;
}
