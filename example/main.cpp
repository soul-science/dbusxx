#include "Session.hpp"
#include "DbusEventLoop.hpp"

#include "Reply.hpp"
#include "PendingReply.hpp"

#include <array>
#include <iostream>
#include <string_view>
#include <string>
#include <vector>

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
        std::cout << "testUint32, i=" << i << std::endl;
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

    std::string testString(const std::string& i) {
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

    void testVector(std::vector<int> v) {
        std::cout << "testVector, v: ";
        for (const auto& item : v) {
            std::cout << item << " ";
        }

        std::cout << std::endl;
    }

    void testVector2D(std::vector<std::vector<int>> vv) {
        std::cout << "testVector2D, vv: ";
        for (const auto& v : vv) {
            std::cout << "\\ ";
            for (const auto& item : v) {
                std::cout << item << " ";
            }
            std::cout << "\\ ";
        }

        std::cout << std::endl;
    }

    void testArray(std::array<int, 3> a) {
        std::cout << "testArray, a: ";
        for (const auto& item : a) {
            std::cout << item << " ";
        }

        std::cout << std::endl;
    }

    void testArray2D(std::array<std::array<int, 3>, 2> aa) {
        std::cout << "testArray2D, aa: ";
        for (const auto& a : aa) {
            std::cout << "\\ ";
            for (const auto& item : a) {
                std::cout << item << " ";
            }
            std::cout << "\\ ";
        }

        std::cout << std::endl;
    }

    void testMultiArgs(int i, std::string s, std::vector<double> v) {
        std::cout << "testMultiArgs"
            << ", i = " << i
            << ", s = " << s << ", v = ( ";
        for (const auto& item : v) {
            std::cout << item << " ";
        }
        std::cout << ")" << std::endl;
    }

    void listenSignal(const std::string& aName) {
        std::cout << "listenSignal_cls_func -- Service acquired name: "
            << aName << std::endl;
    }

    int property1 {1};
    std::string property2 {"p2"};
};

void callback(Reply<int> aRep) {
    std::cout << "reply4 -- isError:" << aRep.isError() 
        << ", errorMessage:" << aRep.errorMessage()
        << ", value:" << aRep.value() << std::endl;
}

int main() {
    SSDbus::Session session(true);
    Test t;

    Status st = session.listenSignal(
        "org.freedesktop.DBus",
        "/org/freedesktop/DBus",
        "org.freedesktop.DBus",
        "NameAcquired",
        &t, &Test::listenSignal
    );

    std::cout << "listenSignal_cls_func -- code:" << static_cast<int>(st.code())
        << ", message:" << st.message() << std::endl;

    st = session.listenSignal(
        "org.freedesktop.DBus",
        "/org/freedesktop/DBus",
        "org.freedesktop.DBus",
        "NameAcquired",
        [] (const std::string& aName) {
            std::cout << "listenSignal_lambda_func -- Service acquired name: "
                << aName << std::endl;
        }
    );

    std::cout << "listenSignal_lambda_func -- code:" << static_cast<int>(st.code())
        << ", message:" << st.message() << std::endl;

    session.setInfo(
        {"com.example.test", "/com/example/test", "com.example.interface"}
    );

    st = session.registerBuilder()
        .addMethod("testInt8", &t, &Test::testInt8)
        .addMethod("testUint8", &t, &Test::testUint8)
        .addMethod("testInt16", &t, &Test::testInt16)
        .addMethod("testUint16", &t, &Test::testUint16)
        .addMethod("testInt32", &t, &Test::testInt32)
        .addMethod("testUint32", &t, &Test::testUint32)
        .addMethod("testInt64", &t, &Test::testInt64)
        .addMethod("testUint64", &t, &Test::testUint64)
        .addMethod("testFloat", &t, &Test::testFloat)
        .addMethod("testDouble", &t, &Test::testDouble)
        .addMethod("testBool", &t, &Test::testBool)
        .addMethod("testConstChars", &t, &Test::testConstChars)
        .addMethod("testString", &t, &Test::testString)
        .addMethod("testStringView", &t, &Test::testStringView)
        .addMethod("testVoid", &t, &Test::testVoid)
        .addMethod("testVector", &t, &Test::testVector)
        .addMethod("testVector2D", &t, &Test::testVector2D)
        .addMethod("testArray", &t, &Test::testArray)
        .addMethod("testArray2D", &t, &Test::testArray2D)
        .addMethod("testMultiArgs", &t, &Test::testMultiArgs)
        .addSignal<int, int>("clear")
        .addSignal<std::vector<int>>("onCount")
        .addProperty("property1", t.property1)
        .addProperty("property2", t.property2)
        .commit();

    std::cout << "register beginRegistration ret=" << st.message() << std::endl;

    int prop1 = 0;
    std::string prop2 = "";
    int prop3 = 0;    

    st = session.getProperty("property1", prop1);
    std::cout << "prop1: st.message=" << st.message() << ", value=" << prop1 << std::endl;

    st = session.getProperty("property2", prop2);
    std::cout << "prop2: st.message=" << st.message() << ", value=" << prop2 << std::endl;

    st = session.getProperty("property3", prop3);
    std::cout << "prop3: st.message=" << st.message() << ", value=" << prop3 << std::endl;

    st = session.onPropertyChanged<int>("property1", [](int newVal) {
        std::cout << "property1 → " << newVal << std::endl;
    });
    std::cout << "onPropertyChanged property1: st.message=" << st.message() << std::endl;

    st = session.onPropertyChanged<std::string>("property2", [](std::string newVal) {
        std::cout << "property2 → " << newVal << std::endl;
    });
    std::cout << "onPropertyChanged property2: st.message=" << st.message() << std::endl;

    st = session.onPropertyChanged<int>("property2", [](int newVal) {
        std::cout << "property2 → " << newVal << std::endl;
    });
    std::cout << "onPropertyChanged property2: st.message=" << st.message() << std::endl;

    // session.registerMethod("testInt8", &t, &Test::testInt8);
    // std::cout << "register testInt8 ret=" << ret.message() << std::endl;

    // ret = session.registerMethod("testUint8", &t, &Test::testUint8);
    // std::cout << "register testUint8 ret=" << ret.message() << std::endl;

    // ret = session.registerMethod("testInt16", &t, &Test::testInt16);
    // std::cout << "register testInt16 ret=" << ret.message() << std::endl;

    // ret = session.registerMethod("testUint16", &t, &Test::testUint16);
    // std::cout << "register testUint16 ret=" << ret.message() << std::endl;

    // ret = session.registerMethod("testInt32", &t, &Test::testInt32);
    // std::cout << "register testInt32 ret=" << ret.message() << std::endl;

    // ret = session.registerMethod("testUint32", &t, &Test::testUint32);
    // std::cout << "register testUint32 ret=" << ret.message() << std::endl;

    // ret = session.registerMethod("testInt64", &t, &Test::testInt64);
    // std::cout << "register testInt64 ret=" << ret.message() << std::endl;

    // ret = session.registerMethod("testUint64", &t, &Test::testUint64);
    // std::cout << "register testUint64 ret=" << ret.message() << std::endl;

    // ret = session.registerMethod("testFloat", &t, &Test::testFloat);
    // std::cout << "register testFloat ret=" << ret.message() << std::endl;

    // ret = session.registerMethod("testDouble", &t, &Test::testDouble);
    // std::cout << "register testDouble ret=" << ret.message() << std::endl;

    // ret = session.registerMethod("testBool", &t, &Test::testBool);
    // std::cout << "register testBool ret=" << ret.message() << std::endl;

    // ret = session.registerMethod("testConstChars", &t, &Test::testConstChars);
    // std::cout << "register testConstChars ret=" << ret.message() << std::endl;

    // ret = session.registerMethod("testString", &t, &Test::testString);
    // std::cout << "register testString ret=" << ret.message() << std::endl;

    // ret = session.registerMethod("testStringView", &t, &Test::testStringView);
    // std::cout << "register testStringView ret=" << ret.message() << std::endl;

    // ret = session.registerMethod("testVoid", &t, &Test::testVoid);
    // std::cout << "register testVoid ret=" << ret.message() << std::endl;

    Reply<void> reply1 = session.callSync(
        "com.sslog.service",
        "/com/sslog/service",
        "com.sslog.service.interfaces",
        "clearAll"
    );

    Reply<int> reply2 = session.callSync<int>(
        "org.freedesktop.DBus",
        "/org/freedesktop/DBus",
        "org.freedesktop.DBus",
        "GetId"                       // 返回类似 "a1b2c3..." 的总线 ID
    );

    std::cout << "reply2 -- isError:" << reply2.isError() 
        << ", value:" << reply2.value() << std::endl;


    PendingReply<std::string> reply3;
    {
    PendingReply<std::string> tmp = session.callAsync<std::string>(
        "org.freedesktop.DBus",
        "/org/freedesktop/DBus",
        "org.freedesktop.DBus",
        "GetId"
    );
    tmp.setCallback(
        [] (Reply<std::string> aRep) -> void {
            std::cout << "reply3 -- isError:" << aRep.isError() 
                << ", value:" << aRep.value() << std::endl;
        }
    );
    reply3 = std::move(tmp);
    }

    st = session.callAsync<int>(
        "org.freedesktop.DBus",
        "/org/freedesktop/DBus",
        "org.freedesktop.DBus",
        "GetId",
        &callback
    );

    st = session.callAsync<std::string>(
        "org.freedesktop.DBus",
        "/org/freedesktop/DBus",
        "org.freedesktop.DBus",
        "GetId",
        [] (Reply<std::string> aRep) -> void {
            std::cout << "reply5 -- isError:" << aRep.isError() 
                << ", value:" << aRep.value() << std::endl;
        }
    );

    // st = session.registerSignal<int, int>(
    //     "clear"
    // );

    // std::cout << "registerSignal -- code:" << static_cast<int>(st.code()) 
    //     << ", message:" << st.message() << std::endl;

    st = session.emitSignal(
        "clear"
    );

    std::cout << "emitSignal -- code:" << static_cast<int>(st.code()) 
        << ", message:" << st.message() << std::endl;

    DbusEventLoop loop(session);
    loop.run();

    return 0;
}