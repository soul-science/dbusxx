/****************************************************************************
 * example_client_internal.cpp
 * 展示 Client 类用法（自管模式：内部创建 Looper + Session + thread）
 *   callSync / callAsync / listenSignal / 信号触发 / 跨线程 emit
 *
 * 启动方式: 先启动 example_server，再启动本程序
 *   或: 本程序会自动启动内嵌 server 做测试（需 session bus）
 ****************************************************************************/

#include "Client.hpp"
#include "Server.hpp"

#include <chrono>
#include <condition_variable>
#include <csignal>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace Dbusxx;

// ── 服务端（同 example_server.cpp，供 Client 测试）─────────────────────

class DemoServer : public Server<DemoServer> {
public:
    DemoServer()
        : Server("com.example.demo") {}

    DBUSXX_PATH("/com/example/demo")
    DBUSXX_IFACE("com.example.Demo")
    int32_t testInt32(int32_t i) {
        std::cout << "[server] testInt32: " << i << std::endl;
        return i;
    }
    DBUSXX_METHOD(testInt32)

    std::string testString(const std::string& i) {
        std::cout << "[server] testString: " << i << std::endl;
        return i;
    }
    DBUSXX_METHOD(testString)

    void testVoid() {
        std::cout << "[server] testVoid" << std::endl;
    }
    DBUSXX_METHOD(testVoid)

    void testMultiArgs(int i, std::string s) {
        std::cout << "[server] testMultiArgs: i=" << i << ", s=" << s << std::endl;
    }
    DBUSXX_METHOD(testMultiArgs)

    void triggerClear(int a, int b) {
        std::cout << "[server] triggerClear(" << a << ", " << b << ")" << std::endl;
        (void)emit("/com/example/demo", "com.example.Demo",
            "clear", a, b);
    }
    DBUSXX_METHOD(triggerClear)

    void shutdown() {
        std::cout << "[server] shutdown" << std::endl;
        stop();
    }
    DBUSXX_METHOD(shutdown)

    DBUSXX_PROPERTY_RO(serverName, std::string, std::string("demo-server"))
    DBUSXX_PROPERTY_RW(version, int32_t, 1)
    DBUSXX_PROPERTY_RW(description, std::string, std::string("demo"))

    DBUSXX_SIGNAL(clear, int, int)

    //! 注册本地属性变更监听
    void listenVersion() {
        [[maybe_unused]] auto st = session().onLocalPropertyChanged<int32_t>(
            "/com/example/demo", "com.example.Demo", "version",
            [this](const int32_t& v) {
                std::cout << "[server] version property changed: " << v << std::endl;
                mVerChanged = true;
                mNewVer = v;
            });
    }

    bool mVerChanged { false };
    int32_t mNewVer { 0 };
};

// ── 同步辅助 ────────────────────────────────────────────────────────────

struct SyncFlag {
    void set() {
        { std::lock_guard<std::mutex> lock(m); flag = true; }
        cv.notify_one();
    }
    void wait() {
        std::unique_lock<std::mutex> lock(m);
        cv.wait(lock, [this] { return flag; });
    }
    void reset() { std::lock_guard<std::mutex> lock(m); flag = false; }
    bool get() const { return flag; }

private:
    std::mutex m;
    std::condition_variable cv;
    bool flag { false };
};

static int gPassed = 0, gFailed = 0;

// ⚠️ onPropertyChanged 注册的监听是永久的（存活到 Client 销毁），
//    回调捕获的对象必须覆盖整个 Client 生命周期。
//    若用块内局部变量，事件循环线程可能在出作用域后仍触发回调
//    → stack-use-after-scope。故提升为文件级 static 保证绝对安全。
static SyncFlag gPropChanged;
static std::string gChangedValue;

#define TEST(name, expr)                                                      \
    do {                                                                      \
        std::cout << "  [" << (name) << "] ";                                 \
        if (expr) { ++gPassed; std::cout << "PASSED"; }                      \
        else       { ++gFailed; std::cout << "FAILED"; }                      \
        std::cout << std::endl;                                               \
    } while (0)

// ── main ─────────────────────────────────────────────────────────────────

int main() {
    DemoServer server;

    std::signal(SIGINT, [](int) {
        std::cout << "\n[SIGINT] exiting..." << std::endl;
        _exit(0);
    });

    // ① 启动服务
    std::cout << "=== Step 1: Starting server ===" << std::endl;
    std::thread serverThread([&server] { server.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // ② 创建 Client（自管模式：内部管理 Session + Looper + thread）
    std::cout << "\n=== Step 2: Creating Client (self-contained) ==="
              << std::endl;
    Client c(SessionType::USER, "com.example.demo", "/com/example/demo", "com.example.Demo");
    // 等待 async 连接的 Hello 握手完成（syncSession 不受影响）
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // ③ 同步调用 — 基础类型
    std::cout << "\n=== Step 3: sync calls (basic) ===" << std::endl;
    {
        auto r = c.callSync<int32_t>("testInt32", 42);
        TEST("testInt32 echo", !r.isError() && r.value() == 42);
    }
    {
        auto r = c.callSync<std::string>("testString", std::string("hello"));
        TEST("testString echo", !r.isError() && r.value() == "hello");
    }
    {
        auto r = c.callSync("testVoid");
        TEST("testVoid", !r.isError());
    }

    // ④ 同步调用 — 复合参数
    std::cout << "\n=== Step 4: sync calls (compound) ===" << std::endl;
    {
        auto r = c.callSync("testMultiArgs", 100, std::string("multi"));
        TEST("testMultiArgs", !r.isError());
    }

    // ④.⑤ 远程属性读取 — getProperty (RO + RW)
    std::cout << "\n=== Step 4.5: getProperty (RO + RW) ===" << std::endl;
    {
        // RO: serverName
        auto r = c.getProperty<std::string>("serverName");
        TEST("getProperty RO string",
            !r.isError() && r.value() == "demo-server");

        // RW: version
        auto r2 = c.getProperty<int32_t>("version");
        TEST("getProperty RW int32_t", !r2.isError() && r2.value() == 1);

        // RW: description
        auto r3 = c.getProperty<std::string>("description");
        TEST("getProperty RW string",
            !r3.isError() && r3.value() == "demo");

        // 不存在的属性
        auto r4 = c.getProperty<int32_t>("nonexistent");
        TEST("getProperty nonexistent", r4.isError());
    }

    // ④.⑥ 远程属性写入 — setProperty (RW OK, RO fail)
    std::cout << "\n=== Step 4.6: setProperty (RW OK, RO fail) ===" << std::endl;
    {
        // RO: 尝试写入 serverName 应失败
        auto st = c.setProperty<std::string>("serverName", std::string("hack"));
        TEST("setProperty RO should fail", st.isError());
        // RO: 确认值未变
        auto r = c.getProperty<std::string>("serverName");
        TEST("getProperty RO unchanged",
            !r.isError() && r.value() == "demo-server");

        // RW: 正常写入 version
        auto st2 = c.setProperty<int32_t>("version", 99);
        TEST("setProperty RW int32_t", st2.isSuccess());
        auto r2 = c.getProperty<int32_t>("version");
        TEST("getProperty RW after set", !r2.isError() && r2.value() == 99);

        // RW: 正常写入 description
        auto st3 = c.setProperty<std::string>("description", std::string("updated"));
        TEST("setProperty RW string", st3.isSuccess());
        auto r3 = c.getProperty<std::string>("description");
        TEST("getProperty RW after set", !r3.isError() && r3.value() == "updated");

        // 不存在的属性
        auto st4 = c.setProperty<int32_t>("nonexistent", 42);
        TEST("setProperty nonexistent", st4.isError());
    }

    // ④.⑦ 本地属性变更监听
    std::cout << "\n=== Step 4.7: local property changed ===" << std::endl;
    server.listenVersion();
    {
        auto st = c.setProperty<int32_t>("version", 77);
        TEST("setProperty v=77", st.isSuccess());
        TEST("onLocalPropertyChanged fired", server.mVerChanged && server.mNewVer == 77);
    }

    // ④.⑧ 远程属性变更监听 — onPropertyChanged
    // 回调捕获对象见文件级 static（gPropChanged/gChangedValue），
    // 原因：此监听是永久的，事件循环线程可能在注册处出作用域后仍触发。
    std::cout << "\n=== Step 4.8: remote property changed ===" << std::endl;
    {
        Status st2 = c.onPropertyChanged("description",
            [&](const std::string& v) {
                std::cout << "  [client] description changed: " << v << std::endl;
                gChangedValue = v;
                gPropChanged.set();
            });
        TEST("onPropertyChanged register", st2.isSuccess());

        auto st = c.setProperty<std::string>("description", std::string("remote-changed"));
        TEST("setProperty trigger", st.isSuccess());
        // 信号在 mAsyncPtr 的事件循环中派发，等待一下
        gPropChanged.wait();
        TEST("onPropertyChanged fired", gChangedValue == "remote-changed");
    }

    // ⑤ 信号监听
    std::cout << "\n=== Step 5: Signal listening ===" << std::endl;
    SyncFlag signalReceived;
    int sigA = 0, sigB = 0;

    Status st = c.listenSignal("clear", [&](int a, int b) {
        std::cout << "  [client] signal clear: a=" << a << ", b=" << b << std::endl;
        sigA = a; sigB = b; signalReceived.set();
    });
    TEST("listenSignal clear", st.isSuccess());

    // ⑥ 触发信号（服务端同线程 emit）
    std::cout << "\n=== Step 6: Trigger signal ===" << std::endl;
    {
        signalReceived.reset();
        auto r = c.callSync("triggerClear", 7, 8);
        TEST("triggerClear call", !r.isError());

        signalReceived.wait();
        TEST("signal received", signalReceived.get());
        TEST("signal args match", sigA == 7 && sigB == 8);
    }

    // ⑦ 跨线程 emit — main 线程直接调 server.emit()
    std::cout << "\n=== Step 7: Cross-thread emit ===" << std::endl;
    {
        signalReceived.reset();
        Status stEmit = server.emit(
            "/com/example/demo", "com.example.Demo",
            "clear", 99, 100);
        TEST("cross-thread emit posted", stEmit.isSuccess());

        signalReceived.wait();
        TEST("cross-thread signal received", signalReceived.get());
        TEST("cross-thread args match", sigA == 99 && sigB == 100);
    }

    // ⑧ 异步调用
    std::cout << "\n=== Step 8: Async call ===" << std::endl;
    {
        SyncFlag asyncDone;
        std::string asyncResult;

        [[maybe_unused]] auto st = c.callAsync<std::string>(
            "testString",
            [&](Reply<std::string> rep) {
                asyncResult = rep.value();
                asyncDone.set();
            },
            std::string("async-hello"));

        asyncDone.wait();
        TEST("async call received", asyncDone.get());
        TEST("async call echo", asyncResult == "async-hello");
    }

    // ⑨ 关闭服务
    std::cout << "\n=== Step 9: Shutdown ===" << std::endl;
    {
        auto r = c.callSync("shutdown");
        TEST("shutdown call", !r.isError());
    }

    serverThread.join();
    std::cout << "[main] done." << std::endl;

    // ── 结果 ──
    std::cout << "\n========================================" << std::endl;
    std::cout << "  Results: " << gPassed << " passed, "
              << gFailed << " failed" << std::endl;
    std::cout << "========================================" << std::endl;

    return gFailed > 0 ? 1 : 0;
}
