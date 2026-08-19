/****************************************************************************
 * example_register.cpp
 * 测试 registerMethod / registerSignal 可接受的 callable 类型:
 *  静态函数、成员函数、lambda、std::function
 ****************************************************************************/

#include "Session.hpp"
#include "Looper.hpp"

#include <cctype>
#include <cstdint>
#include <functional>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace Dbusxx;

// ── 1. 静态函数 ──────────────────────────────────────────────────────────

static int64_t staticAdd(int64_t a, int64_t b) {
    std::cout << "[static] staticAdd(" << a << ", " << b << ") = " << (a + b) << std::endl;
    return a + b;
}

static void staticVoid() {
    std::cout << "[static] staticVoid" << std::endl;
}

struct ExStruct {
    int a;
    std::string b;
};

struct Point {
    int32_t x;
    int32_t y;
};

static void staticStruct(ExStruct aStruct) {
    std::cout << "[static] staticStruct(" << aStruct.a << ", " << aStruct.b << ")" << std::endl;
}

//! struct 作为入参 + 返回值（回显）
static ExStruct echoStruct(const ExStruct& s) {
    std::cout << "[static] echoStruct(" << s.a << ", " << s.b << ")" << std::endl;
    return s;
}

//! vector<Struct> 作为入参 + 返回值
static std::vector<ExStruct> echoStructList(const std::vector<ExStruct>& v) {
    std::cout << "[static] echoStructList size=" << v.size() << std::endl;
    return v;
}

//! 多个 struct 参数，返回聚合结果
static Point addPoints(const Point& p, const Point& q) {
    std::cout << "[static] addPoints(" << p.x << "," << p.y << ") + ("
              << q.x << "," << q.y << ")" << std::endl;
    return Point { p.x + q.x, p.y + q.y };
}

// ── 2. 成员函数类（普通类，不继承 MetaObject）────────────────────────────

class Calc {
public:
    int32_t multiply(int32_t x, int32_t y) {
        std::cout << "[member] multiply(" << x << ", " << y << ") = " << (x * y) << std::endl;
        return x * y;
    }

    void greet(const std::string& name) {
        std::cout << "[member] greet: Hello, " << name << "!" << std::endl;
    }

    //! struct 入参 + 返回，内部改写字段
    ExStruct toUpperStruct(ExStruct s) {
        for (auto& c : s.b) {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
        std::cout << "[member] toUpperStruct -> " << s.a << ", " << s.b << std::endl;
        return s;
    }
};

// ── 3. lambda / std::function — 方法在 main 里注册 ────────────────────

// ── main ─────────────────────────────────────────────────────────────────

int main() {
    auto session = Session::userSession("com.example.register");
    Calc calc;

    // ── 注册静态函数 ─────────────────────────────────────────────────
    Status st = session.registerMethod(
        "/com/example/register", "com.example.Register",
        "staticAdd", staticAdd);
    std::cout << "register staticAdd: " << st.message() << std::endl;

    st = session.registerMethod(
        "/com/example/register", "com.example.Register",
        "staticStruct", staticStruct);
    std::cout << "register ExStruct: " << st.message() << std::endl;

    st = session.registerMethod(
        "/com/example/register", "com.example.Register",
        "staticVoid", staticVoid);
    std::cout << "register staticVoid: " << st.message() << std::endl;

    // ── 注册成员函数 ─────────────────────────────────────────────────
    st = session.registerMethod(
        "/com/example/register", "com.example.Register",
        "multiply", &calc, &Calc::multiply);
    std::cout << "register multiply(member ptr): " << st.message() << std::endl;

    st = session.registerMethod(
        "/com/example/register", "com.example.Register",
        "greet", &calc, &Calc::greet);
    std::cout << "register greet(member ptr): " << st.message() << std::endl;

    // ── 注册 lambda ──────────────────────────────────────────────────
    auto lambdaDiv = [](double a, double b) -> double {
        std::cout << "[lambda] div(" << a << ", " << b << ") = " << (a / b) << std::endl;
        return a / b;
    };
    st = session.registerMethod(
        "/com/example/register", "com.example.Register",
        "lambdaDiv", lambdaDiv);
    std::cout << "register lambdaDiv: " << st.message() << std::endl;

    st = session.registerMethod(
        "/com/example/register", "com.example.Register", "lambdaGreet",
        [](const std::string& name) {
            std::cout << "[lambda] Hello, " << name << "!" << std::endl;
        });
    std::cout << "register lambdaGreet: " << st.message() << std::endl;

    // ── 注册 std::function ───────────────────────────────────────────
    std::function<std::string(const std::string&)> fnUpper =
        [](const std::string& s) -> std::string {
            std::string r = s;
            for (auto& c : r) c = static_cast<char>(std::toupper(c));
            std::cout << "[std::function] upper: " << r << std::endl;
            return r;
        };
    st = session.registerMethod(
        "/com/example/register", "com.example.Register",
        "stdFnUpper", fnUpper);
    std::cout << "register stdFnUpper: " << st.message() << std::endl;

    // ── 注册自定义结构体方法（验证 registerMethod 接受 struct 类型）──
    st = session.registerMethod(
        "/com/example/register", "com.example.Register",
        "echoStruct", echoStruct);
    std::cout << "register echoStruct: " << st.message() << std::endl;

    st = session.registerMethod(
        "/com/example/register", "com.example.Register",
        "echoStructList", echoStructList);
    std::cout << "register echoStructList: " << st.message() << std::endl;

    st = session.registerMethod(
        "/com/example/register", "com.example.Register",
        "addPoints", addPoints);
    std::cout << "register addPoints: " << st.message() << std::endl;

    st = session.registerMethod(
        "/com/example/register", "com.example.Register",
        "toUpperStruct", &calc, &Calc::toUpperStruct);
    std::cout << "register toUpperStruct(member ptr): " << st.message() << std::endl;

    // ── 注册信号 ─────────────────────────────────────────────────────
    st = session.registerSignal<int64_t, std::string>(
        "/com/example/register", "com.example.Register", "onResult");
    std::cout << "registerSignal(onResult): " << st.message() << std::endl;

    st = session.registerSignal<>(
        "/com/example/register", "com.example.Register", "onTick");
    std::cout << "registerSignal(onTick): " << st.message() << std::endl;

    st = session.registerSignal<ExStruct>(
        "/com/example/register", "com.example.Register", "onStruct");
    std::cout << "registerSignal(onStruct): " << st.message() << std::endl;

    // ── 签名验证（注册的副产品：struct 萃取是否正确）────────────────
    std::cout << "signature ExStruct      = " << getSignature<ExStruct>() << std::endl;
    std::cout << "signature Point         = " << getSignature<Point>() << std::endl;
    std::cout << "signature Vec<ExStruct> = "
              << getSignature<std::vector<ExStruct>>() << std::endl;

    // ── 启动事件循环 ─────────────────────────────────────────────────
    Looper looper(session);
    std::thread t([&looper] { looper.run(); });

    std::cout << "[main] running, press Ctrl+C to stop..." << std::endl;
    sleep(30);

    looper.stop();
    t.join();
    return 0;
}