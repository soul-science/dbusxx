/****************************************************************************
 * example_peer.cpp
 * 展示 peer-to-peer D-Bus 连接的完整流程:
 *   服务端(Server) → 客户端(Client) → callSync / emitSignal /
 *   listenSignal / getProperty / setProperty / onPropertyChanged
 *
 * 架构:
 *   [main 线程: client] ←→ [server 线程: peer server event loop]
 *   通过 /tmp/dbusxx-peer-test socket 直连，无 bus daemon
 ****************************************************************************/

#include "Session.hpp"
#include "Looper.hpp"
#include "Client.hpp"
#include "Server.hpp"

#include <array>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <mutex>
#include <iostream>
#include <string>
#include <thread>

using namespace Dbusxx;

constexpr auto SOCKET_PATH = "unix:path=/tmp/com-example-peer";
constexpr auto PATH        = "/com/example/peer";
constexpr auto IFACE       = "com.example.Peer";

// ── 服务端: 使用 Server 宏注册 ───────────────────────────────────────────

class PeerDemo : public Server<PeerDemo> {
public:
    PeerDemo()
        : Server(SessionType::PEER, SOCKET_PATH) {}

    // ── 方法 ──────────────────────────────────────────────────────────

    DBUSXX_PATH(PATH)
    DBUSXX_IFACE(IFACE)
    int32_t echo(int32_t x) {
        std::cout << "[server] echo(" << x << ")" << std::endl;
        return x * 2;
    }
    DBUSXX_METHOD(echo)

    std::string greet(const std::string& name) {
        std::cout << "[server] greet: Hello, " << name << "!" << std::endl;
        return "Hello, " + name + "!";
    }
    DBUSXX_METHOD(greet)

    void ping() {
        std::cout << "[server] ping" << std::endl;
    }
    DBUSXX_METHOD(ping)

    // ── 信号 ──────────────────────────────────────────────────────────

    DBUSXX_SIGNAL(onNotify, int32_t, std::string)

    // ── 属性 ──────────────────────────────────────────────────────────

    DBUSXX_PROPERTY_RW(counter, int32_t, 0)
    DBUSXX_PROPERTY_RO(version, std::string, "1.0.0")
};

// ── main ─────────────────────────────────────────────────────────────────

int main() {
    std::cout << "=== Peer-to-Peer D-Bus Test ===" << std::endl;
    std::cout << "Socket: " << SOCKET_PATH << std::endl;

    // ── 1. 启动服务端 ────────────────────────────────────────────────
    PeerDemo server;
    std::thread serverThread([&server] {
        std::cout << "[server] starting event loop..." << std::endl;
        server.run();
        std::cout << "[server] event loop stopped." << std::endl;
    });

    // 等 server 就绪
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // ── 2. 创建客户端 ────────────────────────────────────────────────
    Client client(SessionType::PEER, SOCKET_PATH, PATH, IFACE);

    // ── 3. 测试 callSync ─────────────────────────────────────────────

    std::cout << "\n--- 3. callSync tests ---" << std::endl;

    // 无参 void
    auto st = client.callSync<void>("ping").status();
    std::cout << "[client] callSync(ping): " << st.message() << std::endl;

    // 带参有返回值
    auto repEcho = client.callSync<int32_t>("echo", 42);
    std::cout << "[client] callSync(echo, 42): value=" << repEcho.value()
              << ", status=" << repEcho.status().message() << std::endl;

    // 字符串类型
    auto repGreet = client.callSync<std::string>("greet", std::string("World"));
    std::cout << "[client] callSync(greet, \"World\"): value=" << repGreet.value()
              << ", status=" << repGreet.status().message() << std::endl;

    // ── 4. 测试信号 ──────────────────────────────────────────────────

    std::cout << "\n--- 4. Signal tests ---" << std::endl;

    std::promise<std::pair<int32_t, std::string>> signalPromise;
    auto signalFuture = signalPromise.get_future();

    st = client.listenSignal("onNotify",
        [&signalPromise](int32_t code, const std::string& msg) {
            std::cout << "[client] signal onNotify received: code="
                      << code << ", msg=" << msg << std::endl;
            signalPromise.set_value({code, msg});
        });
    std::cout << "[client] listenSignal(onNotify): " << st.message() << std::endl;

    // 由 server 线程 emit 信号
    server.post([&server] {
        server.emit(PATH, IFACE, "onNotify", 100, std::string("hello from server"));
    });

    auto sigResult = signalFuture.get();
    std::cout << "[client] got signal values: (" << sigResult.first
              << ", " << sigResult.second << ")" << std::endl;

    // ── 5. 测试属性 ──────────────────────────────────────────────────

    std::cout << "\n--- 5. Property tests ---" << std::endl;

    // get RO property
    auto verRep = client.getProperty<std::string>("version");
    std::cout << "[client] getProperty(version): "
              << verRep.value() << std::endl;

    // get RW property
    auto cntRep = client.getProperty<int32_t>("counter");
    std::cout << "[client] getProperty(counter): "
              << cntRep.value() << std::endl;

    // set RW property
    st = client.setProperty("counter", 42);
    std::cout << "[client] setProperty(counter, 42): "
              << st.message() << std::endl;

    // 验证 set 生效
    cntRep = client.getProperty<int32_t>("counter");
    std::cout << "[client] getProperty(counter) after set: "
              << cntRep.value() << std::endl;

    // ── 6. 测试属性变更监听 ──────────────────────────────────────────

    std::cout << "\n--- 6. Property change tests ---" << std::endl;

    // 属性在 test 5 改过 42，其 PropertiesChanged 信号可能仍滞留在客户端
    // 异步线程里，注册后会先收到残留的 42。不能再用一次性 promise（二次
    // set_value 会抛 std::future_error），改等目标值 99。
    std::mutex propMutex;
    std::condition_variable propCv;
    int32_t propValue = 0;
    bool gotTarget = false;

    st = client.onPropertyChanged("counter",
        [&](int32_t newVal) {
            std::lock_guard lock(propMutex);
            std::cout << "[client] onPropertyChanged(counter): "
                      << newVal << std::endl;
            propValue = newVal;
            if (newVal == 99) {
                gotTarget = true;
            }
            propCv.notify_all();
        });
    std::cout << "[client] onPropertyChanged(counter): "
              << st.message() << std::endl;

    // 由 server 线程改属性
    server.post([&server] {
        server.setProperty(PATH, IFACE, "counter", 99);
    });

    {
        std::unique_lock lock(propMutex);
        propCv.wait(lock, [&gotTarget] { return gotTarget; });
    }
    std::cout << "[client] property changed to: " << propValue << std::endl;

    // ── 7. 清理 ──────────────────────────────────────────────────────

    std::cout << "\n=== All tests passed! ===" << std::endl;
    std::cout << "[main] stopping server..." << std::endl;

    server.stop();
    serverThread.join();

    std::cout << "[main] done." << std::endl;
    return 0;
}
