# Verify the dxxcpp CLI output (run after the dxxcpp_cli test generated it)
# usage: cmake -DOUT_DIR=<dir> -P CheckCliOutputs.cmake

if(NOT DEFINED OUT_DIR)
    message(FATAL_ERROR "OUT_DIR is required")
endif()

#! The types header name is derived from the package (com.example.calc)
set(TYPES_HEADER ComExampleCalcTypes.hpp)

set(EXPECTED
    ${TYPES_HEADER}
    CalculatorSkeleton.hpp
    CalculatorSkeleton.cpp
    CalculatorProxy.hpp
    CalculatorProxy.cpp
    LoggerSkeleton.hpp
    LoggerSkeleton.cpp
    LoggerProxy.hpp
    LoggerProxy.cpp
)

foreach(FILE_NAME IN LISTS EXPECTED)
    set(FILE_PATH "${OUT_DIR}/${FILE_NAME}")
    if(NOT EXISTS "${FILE_PATH}")
        message(FATAL_ERROR "missing generated file: ${FILE_PATH}")
    endif()

    file(SIZE "${FILE_PATH}" FILE_SIZE)
    if(FILE_SIZE EQUAL 0)
        message(FATAL_ERROR "generated file is empty: ${FILE_PATH}")
    endif()

    message(STATUS "found ${FILE_NAME} (${FILE_SIZE} bytes)")
endforeach()

file(READ "${OUT_DIR}/${TYPES_HEADER}" TYPES_HPP)
foreach(NEEDLE
    "struct Point {\n    std::int32_t x;"
    "bool operator==(const Point& aOther) const {\n        return x == aOther.x"
    "static_assert(std::is_aggregate_v<Point>"
)
    string(FIND "${TYPES_HPP}" "${NEEDLE}" FOUND_AT)
    if(FOUND_AT EQUAL -1)
        message(FATAL_ERROR "${TYPES_HEADER} misses '${NEEDLE}'")
    endif()
endforeach()

# Generated code must use the installed library layout; the service name is
# injected by the consumer (DBUSXX_SERVICE_NAME)
file(READ "${OUT_DIR}/CalculatorSkeleton.hpp" SKELETON)
foreach(NEEDLE
    "#include <dbusxx/Server.hpp>"
    "#include \"${TYPES_HEADER}\""
    "explicit CalculatorServer(std::unique_ptr<CalculatorInterface> aIface);"
    "DBUSXX_METHOD(notify)"
    "DBUSXX_PROPERTY_RW(samples, std::vector<std::int32_t>, {1, 2, 3})"
    "DBUSXX_PROPERTY_RW(history, std::vector<Point>, {{1, 2}, {3, 4}})"
    "DBUSXX_PROPERTY_RW(untouched, std::int32_t, std::int32_t{})"
    "DBUSXX_PROPERTY_RW(tags, decltype(std::map<std::string, std::string>{}), {})"
    "DBUSXX_SIGNAL(valueChanged, std::int32_t, std::int32_t)"
    "//! @deprecated\n    DBUSXX_SIGNAL(legacyEvent, std::int32_t)"
    "class CalculatorServer final : public Dbusxx::Server<CalculatorServer>"
)
    string(FIND "${SKELETON}" "${NEEDLE}" FOUND_AT)
    if(FOUND_AT EQUAL -1)
        message(FATAL_ERROR "CalculatorSkeleton.hpp misses '${NEEDLE}'")
    endif()
endforeach()

#! The call bodies live in <Interface>Proxy.cpp, so only declarations are here
#! The rendered parameter list is long, so it is assembled from two parts
string(CONCAT LISTENER_VALUE_CHANGED
    "Dbusxx::Status onValueChanged("
    "std::function<void(std::int32_t oldVal, std::int32_t newVal)> aCallback")

file(READ "${OUT_DIR}/CalculatorProxy.hpp" PROXY)
foreach(NEEDLE
    "#include <dbusxx/Client.hpp>"
    "#include <functional>"
    "#ifndef DBUSXX_SERVICE_NAME\n#error"
    "class CalculatorProxy {"
    "explicit CalculatorProxy();"
    "[[deprecated]]\n    [[nodiscard]] Dbusxx::Reply<void> legacy(std::int32_t code);"
    "[[deprecated]]\n    [[nodiscard]] Dbusxx::Status onLegacyEvent("
    "Dbusxx::Reply<std::int32_t> syncOnly(std::int32_t val)"
    "Dbusxx::PendingReply<std::int32_t> addAsync"
    "Dbusxx::PendingReply<bool> asyncOnlyAsync"
    "Dbusxx::Status addAsync(std::function<void(Dbusxx::Reply<std::int32_t>)> aCallback"
    "Dbusxx::Status asyncOnlyAsync(std::function<void(Dbusxx::Reply<bool>)> aCallback"
    "${LISTENER_VALUE_CHANGED}"
)
    string(FIND "${PROXY}" "${NEEDLE}" FOUND_AT)
    if(FOUND_AT EQUAL -1)
        message(FATAL_ERROR "CalculatorProxy.hpp misses '${NEEDLE}'")
    endif()
endforeach()

#! The ctor is the only place the macro is consumed: bind both halves
string(CONCAT PROXY_CTOR
    "CalculatorProxy::CalculatorProxy()\n"
    "    : mClient(Dbusxx::SessionType::USER, DBUSXX_SERVICE_NAME,")

#! The call bodies moved into the generated sources: the CLI has to write them
file(READ "${OUT_DIR}/CalculatorProxy.cpp" PROXY_SRC)
foreach(NEEDLE
    "#include \"CalculatorProxy.hpp\""
    "${PROXY_CTOR}"
    "mClient.callSync<void, 500000>(\"ping\")"
    "mClient.listenSignal(\"valueChanged\", std::move(aCallback))"
    "mClient.listenSignal(\"legacyEvent\", std::move(aCallback))"
    "mClient.callAsync<bool>(\"asyncOnly\", std::move(aCallback), val)"
)
    string(FIND "${PROXY_SRC}" "${NEEDLE}" FOUND_AT)
    if(FOUND_AT EQUAL -1)
        message(FATAL_ERROR "CalculatorProxy.cpp misses '${NEEDLE}'")
    endif()
endforeach()

#! The bodies are one-liners forwarding to the interface. Bind every needle to
#! the signature it belongs to (a bare "return mIface->add(a, b);" would also
#! pass if the body drifted onto another method) and cover both shapes
string(CONCAT BODY_ADD
    "std::int32_t CalculatorServer::add(std::int32_t a, std::int32_t b) {\n"
    "    return mIface->add(a, b);\n}")
string(CONCAT BODY_NOTIFY
    "void CalculatorServer::notify(const std::string& msg) {\n"
    "    mIface->notify(msg);\n}")

file(READ "${OUT_DIR}/CalculatorSkeleton.cpp" SKELETON_SRC)
foreach(NEEDLE
    "#include \"CalculatorSkeleton.hpp\""
    "#ifndef DBUSXX_SERVICE_NAME\n#error"
    "CalculatorServer::CalculatorServer(std::unique_ptr<CalculatorInterface> aIface)"
    ": Dbusxx::Server<CalculatorServer>(DBUSXX_SERVICE_NAME)"
    "${BODY_ADD}"
    "${BODY_NOTIFY}"
)
    string(FIND "${SKELETON_SRC}" "${NEEDLE}" FOUND_AT)
    if(FOUND_AT EQUAL -1)
        message(FATAL_ERROR "CalculatorSkeleton.cpp misses '${NEEDLE}'")
    endif()
endforeach()

message(STATUS "cli outputs ok")
