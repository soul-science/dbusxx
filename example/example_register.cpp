/****************************************************************************
 * example_register.cpp
 * 测试 registerMethod / registerSignal 可接受的 callable 类型:
 *  静态函数、成员函数、lambda、std::function
 ****************************************************************************/

#include "Session.hpp"
#include "Looper.hpp"

#include <functional>
#include <iostream>
#include <string>
#include <thread>

using namespace SSDbus;

// ── 1. 静态函数 ──────────────────────────────────────────────────────────

static int64_t staticAdd(int64_t a, int64_t b) {
    std::cout << "[static] staticAdd(" << a << ", " << b << ") = " << (a + b) << std::endl;
    return a + b;
}

static void staticVoid() {
    std::cout << "[static] staticVoid" << std::endl;
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
};

// ── 3. lambda / std::function — 方法在 main 里注册 ────────────────────

// ── main ─────────────────────────────────────────────────────────────────

int main() {
    auto session = Session::userSession(
        {"com.example.register", "/com/example/register", "com.example.Register"}
    );
    Calc calc;

    // ── 注册静态函数 ─────────────────────────────────────────────────
    Status st = session.registerMethod("staticAdd", staticAdd);
    std::cout << "register staticAdd: " << st.message() << std::endl;

    st = session.registerMethod("staticVoid", staticVoid);
    std::cout << "register staticVoid: " << st.message() << std::endl;

    // ── 注册成员函数 ─────────────────────────────────────────────────
    st = session.registerMethod("multiply", &calc, &Calc::multiply);
    std::cout << "register multiply(member ptr): " << st.message() << std::endl;

    st = session.registerMethod("greet", &calc, &Calc::greet);
    std::cout << "register greet(member ptr): " << st.message() << std::endl;

    // ── 注册 lambda ──────────────────────────────────────────────────
    auto lambdaDiv = [](double a, double b) -> double {
        std::cout << "[lambda] div(" << a << ", " << b << ") = " << (a / b) << std::endl;
        return a / b;
    };
    st = session.registerMethod("lambdaDiv", lambdaDiv);
    std::cout << "register lambdaDiv: " << st.message() << std::endl;

    st = session.registerMethod("lambdaGreet",
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
    st = session.registerMethod("stdFnUpper", fnUpper);
    std::cout << "register stdFnUpper: " << st.message() << std::endl;

    // ── 注册信号 ─────────────────────────────────────────────────────
    st = session.registerSignal<int64_t, std::string>("onResult");
    std::cout << "registerSignal(onResult): " << st.message() << std::endl;

    st = session.registerSignal<>("onTick");
    std::cout << "registerSignal(onTick): " << st.message() << std::endl;

    // ── 启动事件循环 ─────────────────────────────────────────────────
    Looper looper(session);
    std::thread t([&looper] { looper.run(); });

    std::cout << "[main] running, press Ctrl+C to stop..." << std::endl;
    sleep(30);

    looper.stop();
    t.join();
    return 0;
}