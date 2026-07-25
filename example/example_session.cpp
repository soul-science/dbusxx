/****************************************************************************
 * example_session.cpp
 * 展示直接使用 Session 的完整流程:
 *   listenSignal → setInfo → registerBuilder → getProperty →
 *   onPropertyChanged → callSync → callAsync → emitSignal → Looper 事件循环
 *
 * 所有方法来自 main.cpp 的 Test class，覆盖 D-Bus 全部基础类型。
 ****************************************************************************/

#include "Session.hpp"
#include "Looper.hpp"

#include "Reply.hpp"
#include "PendingReply.hpp"

#include <array>
#include <csignal>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using namespace SSDbus;

// ── 业务类：暴露全部 D-Bus 基础类型方法 ──────────────────────────────────

class Test {
public:
    int8_t testInt8(int8_t i) {
        std::cout << "[server] testInt8, i=" << i << std::endl;
        return i;
    }

    uint8_t testUint8(uint8_t i) {
        std::cout << "[server] testUint8, i=" << i << std::endl;
        return i;
    }

    int16_t testInt16(int16_t i) {
        std::cout << "[server] testInt16, i=" << i << std::endl;
        return i;
    }

    uint16_t testUint16(uint16_t i) {
        std::cout << "[server] testUint16, i=" << i << std::endl;
        return i;
    }

    int32_t testInt32(int32_t i) {
        std::cout << "[server] testInt32, i=" << i << std::endl;
        return i;
    }

    uint32_t testUint32(uint32_t i) {
        std::cout << "[server] testUint32, i=" << i << std::endl;
        return i;
    }

    int64_t testInt64(int64_t i) {
        std::cout << "[server] testInt64, i=" << i << std::endl;
        return i;
    }

    uint64_t testUint64(uint64_t i) {
        std::cout << "[server] testUint64, i=" << i << std::endl;
        return i;
    }

    float testFloat(float i) {
        std::cout << "[server] testFloat, i=" << i << std::endl;
        return i;
    }

    double testDouble(double i) {
        std::cout << "[server] testDouble, i=" << i << std::endl;
        return i;
    }

    bool testBool(bool i) {
        std::cout << "[server] testBool, i=" << i << std::endl;
        return i;
    }

    const char* testConstChars(const char* i) {
        std::cout << "[server] testConstChars, i=" << i << std::endl;
        return i;
    }

    std::string testString(const std::string& i) {
        std::cout << "[server] testString, i=" << i << std::endl;
        return i;
    }

    std::string_view testStringView(std::string_view i) {
        std::cout << "[server] testStringView, i=" << i << std::endl;
        return i;
    }

    void testVoid() {
        std::cout << "[server] testVoid" << std::endl;
    }

    void testVector(std::vector<int> v) {
        std::cout << "[server] testVector: ";
        for (const auto& item : v) std::cout << item << " ";
        std::cout << std::endl;
    }

    void testVector2D(std::vector<std::vector<int>> vv) {
        std::cout << "[server] testVector2D: ";
        for (const auto& v : vv) {
            std::cout << "[";
            for (const auto& item : v) std::cout << item << " ";
            std::cout << "] ";
        }
        std::cout << std::endl;
    }

    void testArray(std::array<int, 3> a) {
        std::cout << "[server] testArray: ";
        for (const auto& item : a) std::cout << item << " ";
        std::cout << std::endl;
    }

    void testArray2D(std::array<std::array<int, 3>, 2> aa) {
        std::cout << "[server] testArray2D: ";
        for (const auto& a : aa) {
            std::cout << "[";
            for (const auto& item : a) std::cout << item << " ";
            std::cout << "] ";
        }
        std::cout << std::endl;
    }

    void testMultiArgs(int i, std::string s, std::vector<double> v) {
        std::cout << "[server] testMultiArgs: i=" << i << ", s=" << s << ", v=[";
        for (const auto& item : v) std::cout << item << " ";
        std::cout << "]" << std::endl;
    }

    // 监听信号回调
    void listenSignal(const std::string& aName) {
        std::cout << "[signal] NameAcquired: " << aName << std::endl;
    }

    int property1 {1};
    std::string property2 {"p2"};
};

// ── main ─────────────────────────────────────────────────────────────────

int main() {
    // ① 创建 Session（session bus）
    Session session(false);
    Test t;

    // ② 监听外部信号 — 成员函数指针
    Status st = session.listenSignal(
        "org.freedesktop.DBus", "/org/freedesktop/DBus",
        "org.freedesktop.DBus", "NameAcquired",
        &t, &Test::listenSignal);
    std::cout << "listenSignal (method ptr): " << st.message() << std::endl;

    // ② 监听外部信号 — lambda
    st = session.listenSignal(
        "org.freedesktop.DBus", "/org/freedesktop/DBus",
        "org.freedesktop.DBus", "NameAcquired",
        [](const std::string& name) {
            std::cout << "[signal lambda] NameAcquired: " << name << std::endl;
        });
    std::cout << "listenSignal (lambda): " << st.message() << std::endl;

    // ③ 设置服务身份 + 注册全部方法 / 信号 / 属性
    session.setInfo(
        {"com.example.test", "/com/example/test", "com.example.Test"});

    st = session.registerBuilder()
        .addMethod("testInt8",       &t, &Test::testInt8)
        .addMethod("testUint8",      &t, &Test::testUint8)
        .addMethod("testInt16",      &t, &Test::testInt16)
        .addMethod("testUint16",     &t, &Test::testUint16)
        .addMethod("testInt32",      &t, &Test::testInt32)
        .addMethod("testUint32",     &t, &Test::testUint32)
        .addMethod("testInt64",      &t, &Test::testInt64)
        .addMethod("testUint64",     &t, &Test::testUint64)
        .addMethod("testFloat",      &t, &Test::testFloat)
        .addMethod("testDouble",     &t, &Test::testDouble)
        .addMethod("testBool",       &t, &Test::testBool)
        .addMethod("testConstChars", &t, &Test::testConstChars)
        .addMethod("testString",     &t, &Test::testString)
        .addMethod("testStringView", &t, &Test::testStringView)
        .addMethod("testVoid",       &t, &Test::testVoid)
        .addMethod("testVector",     &t, &Test::testVector)
        .addMethod("testVector2D",   &t, &Test::testVector2D)
        .addMethod("testArray",      &t, &Test::testArray)
        .addMethod("testArray2D",    &t, &Test::testArray2D)
        .addMethod("testMultiArgs",  &t, &Test::testMultiArgs)
        .addSignal<int, int>("clear")
        .addProperty("property1",    t.property1)
        .addProperty("property2",    t.property2)
        .commit();
    std::cout << "registerBuilder: " << st.message() << std::endl;

    // ④ 读取属性
    int p1 = 0;
    st = session.getProperty("property1", p1);
    std::cout << "getProperty(property1) = " << p1
              << "  (" << st.message() << ")" << std::endl;

    std::string p2;
    st = session.getProperty("property2", p2);
    std::cout << "getProperty(property2) = " << p2
              << "  (" << st.message() << ")" << std::endl;

    // ⑤ 监听属性变化
    st = session.onPropertyChanged<int>("property1", [](int v) {
        std::cout << "[property] property1 -> " << v << std::endl;
    });
    std::cout << "onPropertyChanged(property1): " << st.message() << std::endl;

    // ⑥ 同步调用远程方法
    Reply<void> r1 = session.callSync(
        "com.sslog.service", "/com/sslog/service",
        "com.sslog.service.interfaces", "clearAll");
    std::cout << "callSync(void): isError=" << r1.isError() << std::endl;

    Reply<std::string> r2 = session.callSync<std::string>(
        "org.freedesktop.DBus", "/org/freedesktop/DBus",
        "org.freedesktop.DBus", "GetId");
    std::cout << "callSync(GetId): " << r2.value() << std::endl;

    // ⑦ 异步调用 — PendingReply 方式
    // 注意: PendingReply 必须保持存活直到回复到达，不能提前析构
    {
        PendingReply<std::string> tmp = session.callAsync<std::string>(
            "org.freedesktop.DBus",
            "/org/freedesktop/DBus",
            "org.freedesktop.DBus",
            "GetId"
        );

        tmp.setCallback(
            [] (Reply<std::string> aRep) -> void {
                std::cout << "[async PendingReply tmp] GetId = " << aRep.value() << std::endl;
            }
        );
    }

    PendingReply<std::string> reply;
    {
        PendingReply<std::string> tmp = session.callAsync<std::string>(
            "org.freedesktop.DBus",
            "/org/freedesktop/DBus",
            "org.freedesktop.DBus",
            "GetId"
        );

        tmp.setCallback(
            [] (Reply<std::string> aRep) -> void {
                std::cout << "[async PendingReply] GetId = " << aRep.value() << std::endl;
            }
        );
        reply = std::move(tmp);
    }

    // ⑧ 异步调用 — lambda 回调
    session.callAsync<std::string>(
        "org.freedesktop.DBus", "/org/freedesktop/DBus",
        "org.freedesktop.DBus", "GetId",
        [](Reply<std::string> rep) {
            std::cout << "[async callback] isError=" << rep.isError()
                      << ", value=" << rep.value() << std::endl;
        });

    // ⑨ 发射信号
    st = session.emitSignal("clear");
    std::cout << "emitSignal(clear): " << st.message() << std::endl;

    // ⑩ 启动事件循环
    Looper looper(session);
    std::thread t_loop(&Looper::run, &looper);

    // SIGINT 处理：优雅停止事件循环
    std::signal(SIGINT, [](int) {
        std::cout << "\n[signal] stopping..." << std::endl;
        _exit(0);
    });

    std::cout << "[main] event loop running, press Ctrl+C to stop..." << std::endl;
    sleep(60);

    looper.stop();
    t_loop.join();
    std::cout << "[main] done." << std::endl;

    return 0;
}
