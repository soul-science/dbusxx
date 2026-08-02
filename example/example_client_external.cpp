/****************************************************************************
 * example_client_external.cpp
 * 展示 Client 外部 Looper 用法:
 *   用户自行管理 Session + Looper + 线程生命周期，
 *   Client 只作为 D-Bus 接口代理，通过外部 Looper 投递异步任务
 *
 * 与 example_client_internal 的区别:
 *   - example_client_internal: Client 内部创建/管理 Looper + thread（自管模式）
 *   - 本文件:        用户创建 Looper，传给 Client（外部管理模式）
 *
 * 优势: 多个 Client 可共享同一个 Looper，统一事件循环
 ****************************************************************************/

#include "Client.hpp"
#include "Server.hpp"

#include <chrono>
#include <condition_variable>
#include <csignal>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace SSDbus;

// ── 服务端：内嵌 Client，共用自身 Looper ──────────────────────────────────

class DemoServer : public Server<DemoServer> {
public:
    DemoServer()
        : Server(ServiceInfo{"com.example.ext", "/com/example/ext",
                             "com.example.External"}, false)
        , mSelfClient(new Client(looper(),
            "com.example.ext", "/com/example/ext", "com.example.External")) {}

    int32_t echoInt32(int32_t i) {
        std::cout << "[server] echoInt32: " << i << std::endl;
        return i;
    }
    SSDBUS_METHOD(echoInt32)

    std::string echoString(const std::string& i) {
        std::cout << "[server] echoString: " << i << std::endl;
        return i;
    }
    SSDBUS_METHOD(echoString)

    void triggerSignal(int a, int b) {
        std::cout << "[server] triggerSignal(" << a << ", " << b << ")" << std::endl;
        emit("mySignal", a, b);
    }
    SSDBUS_METHOD(triggerSignal)

    void shutdown() {
        std::cout << "[server] shutdown" << std::endl;
        stop();
    }
    SSDBUS_METHOD(shutdown)

    //! 通过内嵌 Client 监听自身信号（事件循环同线程，无竞争）
    void listenSelfSignal() {
        std::cout << "[server] listenSelfSignal via internal Client" << std::endl;
        mSelfClient->listenSignal("mySignal",
            [this](int a, int b) {
                std::cout << "  [server::selfClient] mySignal: a=" << a
                          << ", b=" << b << std::endl;
                mSelfSigA = a;
                mSelfSigB = b;
                mSelfSigReceived = true;
            });
    }

    //! 通过内嵌 Client 调用自身方法（loopback）
    void callSelf() {
        std::cout << "[server] callSelf via internal Client" << std::endl;
        mSelfClient->callAsync<std::string>(
            "echoString",
            [this](Reply<std::string> rep) {
                std::cout << "  [server::selfClient] echoString reply: "
                          << rep.value() << std::endl;
                mSelfCallResult = rep.value();
                mSelfCallDone = true;
            },
            std::string("loopback-test"));
    }

    SSDBUS_SIGNAL(mySignal, int, int)

    SSDBUS_PROPERTY_RO(name, std::string, std::string("external-demo"))
    SSDBUS_PROPERTY_RW(counter, int32_t, 0)

    // 内嵌 Client：共用 Server 自身 Looper，连接自身做 loopback
    std::unique_ptr<Client> mSelfClient;

    // 内嵌 Client 回调结果
    bool mSelfSigReceived { false };
    int mSelfSigA { 0 }, mSelfSigB { 0 };
    bool mSelfCallDone { false };
    std::string mSelfCallResult;
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

private:
    std::mutex m;
    std::condition_variable cv;
    bool flag { false };
};

static int gPassed = 0, gFailed = 0;

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

    // ② 用户手动创建 Looper + Session（外部管理模式）
    std::cout << "\n=== Step 2: Manual Looper + Session setup ===" << std::endl;
    Session asyncSession(false);
    Looper looper(asyncSession);
    std::thread looperThread([&looper] { looper.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::cout << "[main] external looper thread started" << std::endl;

    // ③ 构造 Client，传入外部 Looper 引用
    std::cout << "\n=== Step 3: Client with external Looper ===" << std::endl;
    Client c(looper, "com.example.ext", "/com/example/ext", "com.example.External");
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    std::cout << "[main] Client created (external looper mode)" << std::endl;

    // ④ 同步调用 — 走独立 thread_local Session，与外部 Looper 无关
    std::cout << "\n=== Step 4: Sync calls ===" << std::endl;
    {
        auto r = c.callSync<int32_t>("echoInt32", 42);
        TEST("callSync echoInt32", !r.isError() && r.value() == 42);
    }
    {
        auto r = c.callSync<std::string>("echoString", std::string("hello"));
        TEST("callSync echoString", !r.isError() && r.value() == "hello");
    }

    // ⑤ 属性读写 — getProperty / setProperty（走 syncSession）
    std::cout << "\n=== Step 5: Properties ===" << std::endl;
    {
        // RO
        auto r = c.getProperty<std::string>("name");
        TEST("getProperty RO name", !r.isError() && r.value() == "external-demo");
        // RO 写应失败
        auto st = c.setProperty<std::string>("name", std::string("hack"));
        TEST("setProperty RO fail", st.isError());
        // RW
        auto st2 = c.setProperty<int32_t>("counter", 10);
        TEST("setProperty RW counter", st2.isSuccess());
        auto r2 = c.getProperty<int32_t>("counter");
        TEST("getProperty RW counter", !r2.isError() && r2.value() == 10);
    }

    // ⑥ 信号监听 — 任务投递到外部 Looper
    std::cout << "\n=== Step 6: Signal listening (via external Looper) ===" << std::endl;
    SyncFlag sigFlag;
    int sigA = 0, sigB = 0;

    Status st = c.listenSignal("mySignal", [&](int a, int b) {
        std::cout << "  [client] mySignal: a=" << a << ", b=" << b << std::endl;
        sigA = a; sigB = b;
        sigFlag.set();
    });
    TEST("listenSignal via external looper", st.isSuccess());

    // ⑦ 触发信号
    std::cout << "\n=== Step 7: Trigger signal ===" << std::endl;
    {
        auto r = c.callSync("triggerSignal", 7, 8);
        TEST("triggerSignal call", !r.isError());
        sigFlag.wait();
        TEST("signal received", sigA == 7 && sigB == 8);
    }

    // ⑧ 异步调用 — 任务投递到外部 Looper
    std::cout << "\n=== Step 8: Async call (via external Looper) ===" << std::endl;
    {
        SyncFlag asyncDone;
        std::string asyncResult;

        Status st2 = c.callAsync<std::string>(
            "echoString",
            [&](Reply<std::string> rep) {
                asyncResult = rep.value();
                asyncDone.set();
            },
            std::string("async-world"));

        TEST("callAsync posted", st2.isSuccess());
        asyncDone.wait();
        TEST("callAsync result", asyncResult == "async-world");
    }

    // ⑨ 远程属性变更监听 — 任务投递到外部 Looper
    std::cout << "\n=== Step 9: Remote property changed ===" << std::endl;
    {
        SyncFlag propFlag;
        int32_t newVal = 0;
        Status st2 = c.onPropertyChanged("counter",
            [&](const int32_t& v) {
                std::cout << "  [client] counter changed: " << v << std::endl;
                newVal = v;
                propFlag.set();
            });
        TEST("onPropertyChanged register", st2.isSuccess());

        c.setProperty<int32_t>("counter", 99);
        propFlag.wait();
        TEST("onPropertyChanged fired", newVal == 99);
    }

    // ⑩ 内嵌 Client — 服务端内部用自身 Looper 监听自己信号
    std::cout << "\n=== Step 10: Internal Client listens self signal ===" << std::endl;
    server.listenSelfSignal();
    {
        // 触发信号 → 先被 main 线程的 c.listenSignal 收到，也被 server 的 mSelfClient 收到
        auto r = c.callSync("triggerSignal", 42, 43);
        TEST("triggerSignal call", !r.isError());
        // 等待 server 内部的 self-client 回调（在 server 事件循环线程）
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        TEST("selfClient signal received", server.mSelfSigReceived
             && server.mSelfSigA == 42 && server.mSelfSigB == 43);
    }

    // ⑪ 内嵌 Client — 服务端 loopback 调用自身方法
    std::cout << "\n=== Step 11: Internal Client calls self method ===" << std::endl;
    {
        server.callSelf();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        TEST("selfClient callAsync result",
            server.mSelfCallDone && server.mSelfCallResult == "loopback-test");
    }

    // ⑫ 关闭
    std::cout << "\n=== Step 12: Shutdown ===" << std::endl;
    {
        auto r = c.callSync("shutdown");
        TEST("shutdown call", !r.isError());
    }

    // ── 自定义清理顺序：先停 looper，再 join 线程 ──
    serverThread.join();
    std::cout << "[main] server thread joined" << std::endl;

    looper.stop();
    looperThread.join();
    std::cout << "[main] looper thread joined" << std::endl;

    // ── 结果 ──
    std::cout << "\n========================================" << std::endl;
    std::cout << "  Results: " << gPassed << " passed, "
              << gFailed << " failed" << std::endl;
    std::cout << "========================================" << std::endl;

    return gFailed > 0 ? 1 : 0;
}
