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
#include <map>
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

    // ─ Map 读写方法 ─

    // 读取 map: echo 回显 int→int map
    /*
        busctl call com.example.myservice \
        /com/example/myservice \
        com.example.MyService \
        testMapRead \
        "a{si}" \
        2 \
        "name" 100  \
        "age" 25 
    */
    std::map<std::string, int32_t> testMapRead(
        const std::map<std::string, int32_t>& m) {
        std::cout << "[MyService] testMapRead: { ";
        for (const auto& [k, v] : m)
            std::cout << k << ":" << v << " ";
        std::cout << "}" << std::endl;
        return m;
    }
    SSDBUS_METHOD(testMapRead)

    // 写入 map: 接收 string→string map
    void testMapWrite(const std::map<std::string, std::string>& m) {
        std::cout << "[MyService] testMapWrite: { ";
        for (const auto& [k, v] : m)
            std::cout << k << ":\"" << v << "\" ";
        std::cout << "}" << std::endl;
    }
    SSDBUS_METHOD(testMapWrite)

    //! 嵌套 map: map<string, vector<int>>
    /*
        busctl --user call com.example.myservice \
        /com/example/myservice \
        com.example.MyService \
        testMapNested \
        "a{sai}" \
        2 \
        "key1" 3 1 2 3 \
        "key2" 2 4 5

    //! 属性测试:
    //!   RO 属性 Get (status):
    //!     busctl --user get-property com.example.myservice \
    //!       /com/example/myservice com.example.MyService status
    //!   RO 属性 Set 应失败:
    //!     busctl --user set-property com.example.myservice \
    //!       /com/example/myservice com.example.MyService status s "stopped"
    //!   RW 属性 Get (version):
    //!     busctl --user get-property com.example.myservice \
    //!       /com/example/myservice com.example.MyService version
    //!   RW 属性 Set:
    //!     busctl --user set-property com.example.myservice \
    //!       /com/example/myservice com.example.MyService version i 99
    */
    void testMapNested(
        const std::map<std::string, std::vector<int>>& m) {
        std::cout << "[MyService] testMapNested: { ";
        for (const auto& [k, v] : m) {
            std::cout << k << ":[";
            for (auto x : v) std::cout << x << ",";
            std::cout << "] ";
        }
        std::cout << "}" << std::endl;
    }
    SSDBUS_METHOD(testMapNested)

    SSDBUS_SIGNAL(clear, int, int)

    // ─ 属性 ─
    // RO: 只读属性，外部只能 Get 不能 Set
    SSDBUS_PROPERTY_RO(status, std::string, std::string("running"))
    // RW: 读写属性，外部可以 Get 也可以 Set
    SSDBUS_PROPERTY_RW(version, int32_t, 1)
    SSDBUS_PROPERTY_RW(description, std::string, std::string("meta-service"))
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
