#include "DbusSession.hpp"
#include "DbusEventLoop.hpp"

#include <iostream>

using namespace SSDbus;

class Test {

public:

    int8_t testInt8(int8_t i) {
        std::cout << "testInt8, i=" << i << std::endl;
        return i;
    };

    uint8_t testUint8(uint8_t i) {
        std::cout << "testUint8, i=" << i << std::endl;
        return i;
    };

    int16_t testInt16(int16_t i) {
        std::cout << "testInt16, i=" << i << std::endl;
        return i;
    };

    uint16_t testUint16(uint16_t i) {
        std::cout << "testUint16, i=" << i << std::endl;
        return i;
    };

    int32_t testInt32(int32_t i) {
        std::cout << "testInt32, i=" << i << std::endl;
        return i;
    };

    uint32_t testUint32(uint32_t i) {
        std::cout << "testInt32, i=" << i << std::endl;
        return i;
    };

    int64_t testInt64(int64_t i) {
        std::cout << "testInt64, i=" << i << std::endl;
        return i;
    };

    uint64_t testUint64(uint64_t i) {
        std::cout << "testUint64, i=" << i << std::endl;
        return i;
    };

    float testFloat(float i) {
        std::cout << "testFloat, i=" << i << std::endl;
        return i;
    };

    double testDouble(double i) {
        std::cout << "testDouble, i=" << i << std::endl;
        return i;
    };

    bool testBool(bool i) {
        std::cout << "testBool, i=" << i << std::endl;
        return i;
    };

    const char* testConstChars(const char* i) {
        std::cout << "testConstChars, i=" << i << std::endl;
        return i;
    };

    std::string testString(std::string i) {
        std::cout << "testString, i=" << i << std::endl;
        return i;
    };

    std::string_view testStringView(std::string_view i) {
        std::cout << "testStringView, i=" << i << std::endl;
        return i;
    };

    void testVoid() {
        std::cout << "testVoid" << std::endl;
    }

};

int main() {
    SSDbus::DbusSession session(true);
    session.setDbusInfo(
        {"com.example.test", "/com/example/test", "com.example.interface"}
    );

    Test t;
    auto ret = session.registerInterface("testInt8", &t, &Test::testInt8);
    std::cout << "register testInt8 ret=" << static_cast<int>(ret.getStatus()) << std::endl;

    ret = session.registerInterface("testUint8", &t, &Test::testUint8);
    std::cout << "register testUint8 ret=" << static_cast<int>(ret.getStatus()) << std::endl;

    ret = session.registerInterface("testInt16", &t, &Test::testInt16);
    std::cout << "register testInt16 ret=" << static_cast<int>(ret.getStatus()) << std::endl;

    ret = session.registerInterface("testUint16", &t, &Test::testUint16);
    std::cout << "register testUint16 ret=" << static_cast<int>(ret.getStatus()) << std::endl;

    ret = session.registerInterface("testInt32", &t, &Test::testInt32);
    std::cout << "register testInt32 ret=" << static_cast<int>(ret.getStatus()) << std::endl;

    ret = session.registerInterface("testUint32", &t, &Test::testUint32);
    std::cout << "register testUint32 ret=" << static_cast<int>(ret.getStatus()) << std::endl;

    ret = session.registerInterface("testInt64", &t, &Test::testInt64);
    std::cout << "register testInt64 ret=" << static_cast<int>(ret.getStatus()) << std::endl;

    ret = session.registerInterface("testUint64", &t, &Test::testUint64);
    std::cout << "register testUint64 ret=" << static_cast<int>(ret.getStatus()) << std::endl;

    ret = session.registerInterface("testFloat", &t, &Test::testFloat);
    std::cout << "register testFloat ret=" << static_cast<int>(ret.getStatus()) << std::endl;

    ret = session.registerInterface("testDouble", &t, &Test::testDouble);
    std::cout << "register testDouble ret=" << static_cast<int>(ret.getStatus()) << std::endl;

    ret = session.registerInterface("testBool", &t, &Test::testBool);
    std::cout << "register testBool ret=" << static_cast<int>(ret.getStatus()) << std::endl;

    ret = session.registerInterface("testConstChars", &t, &Test::testConstChars);
    std::cout << "register testConstChars ret=" << static_cast<int>(ret.getStatus()) << std::endl;

    ret = session.registerInterface("testString", &t, &Test::testString);
    std::cout << "register testString ret=" << static_cast<int>(ret.getStatus()) << std::endl;

    ret = session.registerInterface("testStringView", &t, &Test::testStringView);
    std::cout << "register testStringView ret=" << static_cast<int>(ret.getStatus()) << std::endl;

    ret = session.registerInterface("testVoid", &t, &Test::testVoid);
    std::cout << "register testVoid ret=" << static_cast<int>(ret.getStatus()) << std::endl;

    DbusEventLoop loop(session);
    loop.run();

    return 0;
}