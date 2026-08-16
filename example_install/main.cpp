/****************************************************************************
 * example_install.cpp
 * 安装验证测试：通过 find_package(ssdbus) 链接 /usr/local 已安装的
 * libssdbus.so + include/ssdbus 头文件，验证安装产物可被外部项目直接使用。
 *
 * 功能：
 *   1. 连接 session bus（注册唯一 name）
 *   2. 注册方法 double(int32) → int32（vtable 生成，验证注册 API）
 *   3. 远端调用 org.freedesktop.DBus.GetId（验证 string 返回解析）
 *   4. 远端调用 org.freedesktop.DBus.ListNames（验证 vector 容器反序列化）
 *
 * 注：不验证"单连接回环调用自己的 name"——sd-bus 单连接自调用不可靠，
 *     与安装产物无关。
 ****************************************************************************/

#include <ssdbus/Reply.hpp>
#include <ssdbus/Session.hpp>

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace SSDbus;

int main() {
    // ① 连接 session bus，注册唯一 name（避免与已存在的实例冲突）
    const std::string name =
        "com.example.installtest" + std::to_string(::getpid());
    Session session = Session::userSession(name);
    std::cout << "[install-test] session: " << name << std::endl;

    // ② 注册方法 double: int32 -> int32（验证 vtable 注册链可用）
    auto doubleFn = [](int32_t i) -> int32_t { return i * 2; };
    Status st = session
        .registerBuilder("/com/example/install", "com.example.Install")
        .addMethod("double", doubleFn)
        .commit();
    std::cout << "[install-test] registerBuilder: " << st.message() << std::endl;
    if (st.isError()) {
        return 1;
    }

    // ③ 远端调用 1：string 返回
    Reply<std::string> id = session.callSync<std::string>(
        "org.freedesktop.DBus", "/org/freedesktop/DBus",
        "org.freedesktop.DBus", "GetId");
    if (id.isError()) {
        std::cout << "[install-test] callSync(GetId) FAILED: "
                  << id.errorMessage() << std::endl;
        return 1;
    }
    std::cout << "[install-test] GetId = " << id.value() << std::endl;

    // ④ 远端调用 2：vector<string> 返回（验证容器反序列化）
    Reply<std::vector<std::string>> names = session.callSync<std::vector<std::string>>(
        "org.freedesktop.DBus", "/org/freedesktop/DBus",
        "org.freedesktop.DBus", "ListNames");
    if (names.isError()) {
        std::cout << "[install-test] callSync(ListNames) FAILED: "
                  << names.errorMessage() << std::endl;
        return 1;
    }
    std::cout << "[install-test] ListNames count = " << names.value().size()
              << std::endl;

    std::cout << "[install-test] ALL PASSED" << std::endl;
    return 0;
}
