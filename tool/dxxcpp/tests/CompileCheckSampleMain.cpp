//! CLI output compile check: the headers generated from Sample.dxx are used as
//! real code (-fsyntax-only), instantiating every shape a user can write --
//! the types header, both skeletons, all method call shapes and the signal listeners.
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
    void ping() override {}
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

    //! Properties are registration-only, so only the method call sites are
    //! instantiated here.
    auto aReply = aCalcProxy.getConfig();
    auto aNotifySt = aCalcProxy.notify("hello");
    auto aSt = aLoggerProxy.log("hello");
    (void)aReply;
    (void)aNotifySt;
    (void)aSt;

    //! @sync -> sync shape only; @async -> the two async shapes only, both named
    //! <name>Async (the bare name is never generated)
    auto aSyncReply = aCalcProxy.syncOnly(1);
    auto aPending = aCalcProxy.asyncOnlyAsync(1);
    auto aAsyncStatus = aCalcProxy.asyncOnlyAsync([] (Dbusxx::Reply<bool>) {}, 1);
    (void)aSyncReply;
    (void)aPending;
    (void)aAsyncStatus;

    //! void + @timeout: all three shapes carry <void, TimeoutUsec>
    auto aPingReply = aCalcProxy.ping();
    auto aPingPending = aCalcProxy.pingAsync();
    auto aPingStatus = aCalcProxy.pingAsync([] (Dbusxx::Reply<void>) {});
    (void)aPingReply;
    (void)aPingPending;
    (void)aPingStatus;

    //! Each signal gets an onXxx listener (Session::listenSignal + its isValidArgs
    //! guard). Listeners are permanent: capture only objects outliving the Proxy.
    auto aValueChangedSt = aCalcProxy.onValueChanged(
        [] (std::int32_t aOldVal, std::int32_t aNewVal) {
            (void)aOldVal;
            (void)aNewVal;
        });
    (void)aValueChangedSt;

    //! onLegacyEvent is @deprecated, so it is deliberately not called: this check
    //! compiles with -Werror=deprecated-declarations.
    auto aLogAddedSt = aLoggerProxy.onLogAdded(
        [] (const std::string& aMessage) { (void)aMessage; });
    (void)aLogAddedSt;

    return 0;
}
