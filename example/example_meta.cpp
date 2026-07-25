 /****************************************************************************
 * example_meta.cpp
 * 展示 MetaObject + 宏声明式注册:
 *   SSDBUS_METHOD / SSDBUS_SIGNAL / SSDBUS_LISTEN → registerObject 一键注册
 *
 * 所有方法来自 main.cpp 的 Test class，覆盖 D-Bus 全部基础类型。
 ****************************************************************************/

#include "Session.hpp"
#include "Looper.hpp"
#include "MetaObject.hpp"

#include <array>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using namespace SSDbus;

// ── 服务类：CRTP + 宏声明全部 D-Bus 接口 ─────────────────────────────────

class MyService : public MetaObject<MyService> {
public:
    int8_t testInt8(int8_t i) {
        std::cout << "[MyService] testInt8, i=" << i << std::endl;
        return i;
    }
    SSDBUS_METHOD(testInt8)

    uint8_t testUint8(uint8_t i) {
        std::cout << "[MyService] testUint8, i=" << i << std::endl;
        return i;
    }
    SSDBUS_METHOD(testUint8)

    int16_t testInt16(int16_t i) {
        std::cout << "[MyService] testInt16, i=" << i << std::endl;
        return i;
    }
    SSDBUS_METHOD(testInt16)

    uint16_t testUint16(uint16_t i) {
        std::cout << "[MyService] testUint16, i=" << i << std::endl;
        return i;
    }
    SSDBUS_METHOD(testUint16)

    int32_t testInt32(int32_t i) {
        std::cout << "[MyService] testInt32, i=" << i << std::endl;
        return i;
    }
    SSDBUS_METHOD(testInt32)

    uint32_t testUint32(uint32_t i) {
        std::cout << "[MyService] testUint32, i=" << i << std::endl;
        return i;
    }
    SSDBUS_METHOD(testUint32)

    int64_t testInt64(int64_t i) {
        std::cout << "[MyService] testInt64, i=" << i << std::endl;
        return i;
    }
    SSDBUS_METHOD(testInt64)

    uint64_t testUint64(uint64_t i) {
        std::cout << "[MyService] testUint64, i=" << i << std::endl;
        return i;
    }
    SSDBUS_METHOD(testUint64)

    float testFloat(float i) {
        std::cout << "[MyService] testFloat, i=" << i << std::endl;
        return i;
    }
    SSDBUS_METHOD(testFloat)

    double testDouble(double i) {
        std::cout << "[MyService] testDouble, i=" << i << std::endl;
        return i;
    }
    SSDBUS_METHOD(testDouble)

    bool testBool(bool i) {
        std::cout << "[MyService] testBool, i=" << i << std::endl;
        return i;
    }
    SSDBUS_METHOD(testBool)

    const char* testConstChars(const char* i) {
        std::cout << "[MyService] testConstChars, i=" << i << std::endl;
        return i;
    }
    SSDBUS_METHOD(testConstChars)

    std::string testString(const std::string& i) {
        std::cout << "[MyService] testString, i=" << i << std::endl;
        return i;
    }
    SSDBUS_METHOD(testString)

    std::string_view testStringView(std::string_view i) {
        std::cout << "[MyService] testStringView, i=" << i << std::endl;
        return i;
    }
    SSDBUS_METHOD(testStringView)

    void testVoid() {
        std::cout << "[MyService] testVoid" << std::endl;
    }
    SSDBUS_METHOD(testVoid)

    void testVector(std::vector<int> v) {
        std::cout << "[MyService] testVector: ";
        for (const auto& item : v) std::cout << item << " ";
        std::cout << std::endl;
    }
    SSDBUS_METHOD(testVector)

    void testVector2D(std::vector<std::vector<int>> vv) {
        std::cout << "[MyService] testVector2D: ";
        for (const auto& v : vv) {
            std::cout << "[";
            for (const auto& item : v) std::cout << item << " ";
            std::cout << "] ";
        }
        std::cout << std::endl;
    }
    SSDBUS_METHOD(testVector2D)

    void testArray(std::array<int, 3> a) {
        std::cout << "[MyService] testArray: ";
        for (const auto& item : a) std::cout << item << " ";
        std::cout << std::endl;
    }
    SSDBUS_METHOD(testArray)

    void testArray2D(std::array<std::array<int, 3>, 2> aa) {
        std::cout << "[MyService] testArray2D: ";
        for (const auto& a : aa) {
            std::cout << "[";
            for (const auto& item : a) std::cout << item << " ";
            std::cout << "] ";
        }
        std::cout << std::endl;
    }
    SSDBUS_METHOD(testArray2D)

    void testMultiArgs(int i, std::string s, std::vector<double> v) {
        std::cout << "[MyService] testMultiArgs: i=" << i << ", s=" << s << ", v=[";
        for (const auto& item : v) std::cout << item << " ";
        std::cout << "]" << std::endl;
    }
    SSDBUS_METHOD(testMultiArgs)

    SSDBUS_SIGNAL(clear, int, int)
};

// ── 监听类：用宏声明信号监听 ─────────────────────────────────────────────

class NameWatcher : public MetaObject<NameWatcher> {
public:
    void onNameAcquired(const std::string& name) {
        std::cout << "[NameWatcher] bus name acquired: " << name << std::endl;
    }
    SSDBUS_LISTEN(onNameAcquired,
        "org.freedesktop.DBus", "/org/freedesktop/DBus",
        "org.freedesktop.DBus", "NameAcquired")
};

// ── main ─────────────────────────────────────────────────────────────────

int main() {
    Session session(false);

    MyService svc;
    NameWatcher watcher;

    session.setInfo({"com.example.myservice", "/com/example/myservice",
                     "com.example.MyService"});

    Status st = session.registerObject(&svc);
    std::cout << "registerObject(MyService): " << st.message() << std::endl;

    st = session.registerObject(&watcher);
    std::cout << "registerObject(NameWatcher): " << st.message() << std::endl;

    Looper looper(session);
    std::thread t(&Looper::run, &looper);

    std::cout << "[main] event loop running, press Ctrl+C to stop..." << std::endl;
    sleep(30);

    looper.stop();
    t.join();
    std::cout << "[main] done." << std::endl;

    return 0;
}
