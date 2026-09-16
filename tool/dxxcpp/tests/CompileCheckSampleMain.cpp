//! CLI output compile check: headers generated from Sample.dxx used as real
//! code (syntax only). Covers Types.hpp, both Skeleton and both Proxy headers.
#define DBUSXX_SERVICE_NAME "com.example.app"
#include <cstdint>
#include <map>
#include <memory>
#include <string>

#include "CalculatorSkeleton.hpp"
#include "CalculatorProxy.hpp"
#include "LoggerSkeleton.hpp"
#include "LoggerProxy.hpp"


using namespace Com::Example::Calc;

struct CalcImpl : CalculatorInterface {
    std::int32_t add(std::int32_t a, std::int32_t b) override { return a + b; }
    std::int32_t multiply(std::int32_t a, std::int32_t b) override { return a * b; }
    Point translate(const Point& p, std::int32_t dx) override { return {}; }
    ConfigMap getConfig() override { return {}; }
    void notify(const std::string& msg) override {}
    void legacy(std::int32_t code) override {}
    std::int32_t syncOnly(std::int32_t val) override { return val; }
    bool asyncOnly(std::int32_t val) override { return val != 0; }
};

struct LoggerImpl : LoggerInterface {
    void log(const std::string& message) override {}
};

int main() {
    CalculatorServer aCalc(std::make_unique<CalcImpl>());
    LoggerServer aLogger(std::make_unique<LoggerImpl>());
    CalculatorProxy aCalcProxy;
    LoggerProxy aLoggerProxy;

    (void)aCalc;
    (void)aLogger;

    //! Properties and signals are registration-only (DBUSXX_* macros);
    //! method call sites must instantiate
    auto aReply = aCalcProxy.getConfig();
    auto aNotifySt = aCalcProxy.notify("hello");
    auto aSt = aLoggerProxy.log("hello");
    (void)aReply;
    (void)aNotifySt;
    (void)aSt;

    //! @sync -> only the synchronous shape, @async -> only the two async ones
    //! (the async pair shares the <name>Async name, the bare name is not generated)
    auto aSyncReply = aCalcProxy.syncOnly(1);
    auto aPending = aCalcProxy.asyncOnlyAsync(1);
    auto aAsyncStatus = aCalcProxy.asyncOnlyAsync([] (Dbusxx::Reply<bool>) {}, 1);
    (void)aSyncReply;
    (void)aPending;
    (void)aAsyncStatus;

    return 0;
}
