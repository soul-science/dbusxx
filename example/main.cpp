#include "DbusSession.hpp"

#include <iostream>

class Test {

public:
    void test() {
        std::cout << "hello world\n";
    };

};

int main() {
    SSDbus::DbusSession session(true);
    Test t;
    session.registerInterface("test", &t, &Test::test);
    return 0;
}