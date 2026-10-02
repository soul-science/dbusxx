//! CLI output compile check: the generated headers are used as real code
//! (-fsyntax-only), instantiating every shape, property accessors included.
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

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

//! @readonly 'version' must expose no setter on either side: locks the rule
//! Codegen and Sema must agree on.
template <typename, typename = void>
struct HasSetVersion : std::false_type {};

template <typename T>
struct HasSetVersion<T, decltype(
    std::declval<T&>().setVersion(std::declval<const std::string&>()), void())>
    : std::true_type {};

static_assert(!HasSetVersion<CalculatorProxy>::value,
    "@readonly 'version' must not generate CalculatorProxy::setVersion");
static_assert(!HasSetVersion<CalculatorServer>::value,
    "@readonly 'version' must not generate CalculatorServer::setVersion");

int main() {
    CalculatorServer aCalc(std::make_unique<CalcImpl>());
    LoggerServer aLogger(std::make_unique<LoggerImpl>());
    CalculatorProxy aCalcProxy;
    LoggerProxy aLoggerProxy;

    (void)aCalc;
    (void)aLogger;

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

    //! Server side: every signal also gets an emit wrapper; a parameterless one
    //! has to stay callable with no arguments
    auto aNoArgSt = aCalc.emitNoArgEvent();
    (void)aNoArgSt;

    //! Proxy properties: get<Name>() -> Reply<T>, set<Name>() (writable only) -> Status
    (void)aCalcProxy.getVersion();
    (void)aCalcProxy.getCounter();
    (void)aCalcProxy.setCounter(1);
    (void)aCalcProxy.getMetadata();
    (void)aCalcProxy.setMetadata(std::map<std::string, std::string>{});
    (void)aCalcProxy.getUntouched();
    (void)aCalcProxy.setUntouched(0);
    (void)aCalcProxy.getTags();
    (void)aCalcProxy.setTags(std::map<std::string, std::string>{});
    (void)aCalcProxy.getSamples();
    (void)aCalcProxy.setSamples(std::vector<std::int32_t>{1, 2, 3});
    (void)aCalcProxy.getOrigin();
    (void)aCalcProxy.setOrigin(Point{1, 2});
    (void)aCalcProxy.getHistory();
    (void)aCalcProxy.setHistory(std::vector<Point>{ {1, 2}, {3, 4} });

    //! Skeleton properties: get<Name>() -> T, set<Name>() -> Status
    (void)aCalc.getVersion();
    (void)aCalc.getCounter();
    (void)aCalc.setCounter(1);
    (void)aCalc.getMetadata();
    (void)aCalc.setMetadata(std::map<std::string, std::string>{});
    (void)aCalc.getUntouched();
    (void)aCalc.setUntouched(0);
    (void)aCalc.getTags();
    (void)aCalc.setTags(std::map<std::string, std::string>{});
    (void)aCalc.getSamples();
    (void)aCalc.setSamples(std::vector<std::int32_t>{1, 2, 3});
    (void)aCalc.getOrigin();
    (void)aCalc.setOrigin(Point{1, 2});
    (void)aCalc.getHistory();
    (void)aCalc.setHistory(std::vector<Point>{ {1, 2}, {3, 4} });

    return 0;
}
