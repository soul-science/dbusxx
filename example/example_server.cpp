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

#include <array>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <iostream>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using namespace SSDbus;

// ── 服务类 ────────────────────────────────────────────────────────────────

class DemoServer : public Server<DemoServer> {
public:
    DemoServer()
        : Server(ServiceInfo{"com.example.demo", "/com/example/demo",
                             "com.example.Demo"}, false /* session bus */) {}

    // ─ 基础类型方法 (echo 回显) ─

    int8_t testInt8(int8_t i) {
        std::cout << "[server] testInt8: " << +i << std::endl;
        return i;
    }
    SSDBUS_METHOD(testInt8)

    uint8_t testUint8(uint8_t i) {
        std::cout << "[server] testUint8: " << +i << std::endl;
        return i;
    }
    SSDBUS_METHOD(testUint8)

    int16_t testInt16(int16_t i) {
        std::cout << "[server] testInt16: " << i << std::endl;
        return i;
    }
    SSDBUS_METHOD(testInt16)

    uint16_t testUint16(uint16_t i) {
        std::cout << "[server] testUint16: " << i << std::endl;
        return i;
    }
    SSDBUS_METHOD(testUint16)

    int32_t testInt32(int32_t i) {
        std::cout << "[server] testInt32: " << i << std::endl;
        return i;
    }
    SSDBUS_METHOD(testInt32)

    uint32_t testUint32(uint32_t i) {
        std::cout << "[server] testUint32: " << i << std::endl;
        return i;
    }
    SSDBUS_METHOD(testUint32)

    int64_t testInt64(int64_t i) {
        std::cout << "[server] testInt64: " << i << std::endl;
        return i;
    }
    SSDBUS_METHOD(testInt64)

    uint64_t testUint64(uint64_t i) {
        std::cout << "[server] testUint64: " << i << std::endl;
        return i;
    }
    SSDBUS_METHOD(testUint64)

    float testFloat(float i) {
        std::cout << "[server] testFloat: " << i << std::endl;
        return i;
    }
    SSDBUS_METHOD(testFloat)

    double testDouble(double i) {
        std::cout << "[server] testDouble: " << i << std::endl;
        return i;
    }
    SSDBUS_METHOD(testDouble)

    bool testBool(bool i) {
        std::cout << "[server] testBool: " << (i ? "true" : "false") << std::endl;
        return i;
    }
    SSDBUS_METHOD(testBool)

    const char* testConstChars(const char* i) {
        std::cout << "[server] testConstChars: " << i << std::endl;
        return i;
    }
    SSDBUS_METHOD(testConstChars)

    std::string testString(const std::string& i) {
        std::cout << "[server] testString: " << i << std::endl;
        return i;
    }
    SSDBUS_METHOD(testString)

    std::string_view testStringView(std::string_view i) {
        std::cout << "[server] testStringView: " << i << std::endl;
        return i;
    }
    SSDBUS_METHOD(testStringView)

    void testVoid() {
        std::cout << "[server] testVoid" << std::endl;
    }
    SSDBUS_METHOD(testVoid)

    void testVector(std::vector<int> v) {
        std::cout << "[server] testVector: ";
        for (const auto& item : v) std::cout << item << " ";
        std::cout << std::endl;
    }
    SSDBUS_METHOD(testVector)

    void testMultiArgs(int i, std::string s, std::vector<double> v) {
        std::cout << "[server] testMultiArgs: i=" << i << ", s=" << s;
        std::cout << ", v=[ ";
        for (const auto& item : v) std::cout << item << " ";
        std::cout << "]" << std::endl;
    }
    SSDBUS_METHOD(testMultiArgs)

    // ─ 信号触发方法 ─

    // 同线程 emit: 由 D-Bus 调用触发，在事件循环线程内执行
    void triggerClear(int a, int b) {
        std::cout << "[server] triggerClear(" << a << ", " << b
                  << ") → emit from event loop thread" << std::endl;
        Status st = emit("clear", a, b);
        std::cout << "[server] emit result: " << st.message() << std::endl;
    }
    SSDBUS_METHOD(triggerClear)

    // ─ 关闭服务 ─

    void shutdown() {
        std::cout << "[server] shutdown → stopping looper..." << std::endl;
        stop();
    }
    SSDBUS_METHOD(shutdown)

    // ─ 信号声明 ─

    SSDBUS_SIGNAL(clear, int, int)

    // ─ 监听 DBus 总线信号 ─

    void onNameAcquired(const std::string& name) {
        std::cout << "[server] NameAcquired: " << name << std::endl;
    }
    SSDBUS_LISTEN(onNameAcquired,
        "org.freedesktop.DBus", "/org/freedesktop/DBus",
        "org.freedesktop.DBus", "NameAcquired")
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
    Session syncClient(false);
    Session asyncClient(false);

    const char* svc  = "com.example.demo";
    const char* path = "/com/example/demo";
    const char* iface = "com.example.Demo";

    // ③ 同步调用 — 基础类型
    std::cout << "\n=== Step 3: Sync calls (basic types) ===" << std::endl;
    {
        auto r = syncClient.callSync<int32_t, int32_t>(
            svc, path, iface, "testInt32", uint64_t(0), 42);
        TEST("testInt32 echo", !r.isError() && r.value() == 42);
    }
    {
        auto r = syncClient.callSync<std::string, std::string>(
            svc, path, iface, "testString", uint64_t(0),
            std::string("hello"));
        TEST("testString echo", !r.isError() && r.value() == "hello");
    }
    {
        auto r = syncClient.callSync<bool, bool>(
            svc, path, iface, "testBool", uint64_t(0), true);
        TEST("testBool echo", !r.isError() && r.value() == true);
    }
    {
        auto r = syncClient.callSync<double, double>(
            svc, path, iface, "testDouble", uint64_t(0), 3.14);
        TEST("testDouble echo", !r.isError() && r.value() == 3.14);
    }
    {
        auto r = syncClient.callSync<int64_t, int64_t>(
            svc, path, iface, "testInt64", uint64_t(0),
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
                                     uint64_t(0), std::vector<int>{1, 2, 3});
        TEST("testVector", !r.isError());
    }
    {
        auto r = syncClient.callSync(svc, path, iface, "testMultiArgs",
                                     uint64_t(0),
                                     100, std::string("multi"),
                                     std::vector<double>{1.1, 2.2});
        TEST("testMultiArgs", !r.isError());
    }

    // ⑤ 启动客户端事件循环（绑定 asyncClient，不与 syncClient 的 fd 冲突）
    std::cout << "\n=== Step 5: Starting client event loop ===" << std::endl;
    Looper clientLooper(asyncClient);
    std::thread clientThread([&clientLooper] {
        clientLooper.run();
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // ⑥ 信号监听
    std::cout << "\n=== Step 6: Signal listening ===" << std::endl;
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

    // ⑦ 同线程 emit — 用 syncClient 调用（无事件循环，不会竞争）
    std::cout << "\n=== Step 7: In-thread emit (via D-Bus method) ==="
              << std::endl;
    {
        signalReceived.reset();
        auto r = syncClient.callSync<void, int, int>(
            svc, path, iface, "triggerClear", uint64_t(0), 7, 8);
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
        Status stEmit = server.emit("clear", 99, 100);
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

        asyncClient.callAsync<std::string>(
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
