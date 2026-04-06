#include "DbusSession.hpp"

#include <iostream>

class Test {

public:
    int test(int, double) {
        return 1;
    };

};

int main() {
    SSDbus::DbusSession session(true);
    session.setDbusInfo(
        {"com.example.test", "/com/example/test", "com.example.interface"}
    );

    Test t;
    auto ret = session.registerInterface("test", &t, &Test::test);
    std::cout << "register ret=" << static_cast<int>(ret.getStatus()) << std::endl; 

    return 0;
}