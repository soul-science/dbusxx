# Verify the dxxcpp CLI output (run after the dxxcpp_cli test generated it)
# usage: cmake -DOUT_DIR=<dir> -P CheckCliOutputs.cmake

if(NOT DEFINED OUT_DIR)
    message(FATAL_ERROR "OUT_DIR is required")
endif()

set(EXPECTED
    Types.hpp
    CalculatorSkeleton.hpp
    CalculatorProxy.hpp
    LoggerSkeleton.hpp
    LoggerProxy.hpp
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

file(READ "${OUT_DIR}/Types.hpp" TYPES_HPP)
foreach(NEEDLE
    "struct Point {"
    "bool operator==(const Point& aOther) const {"
    "static_assert(std::is_aggregate_v<Point>")
    string(FIND "${TYPES_HPP}" "${NEEDLE}" FOUND_AT)
    if(FOUND_AT EQUAL -1)
        message(FATAL_ERROR "Types.hpp misses '${NEEDLE}'")
    endif()
endforeach()

# Generated code must use the installed library layout; the service name is
# injected by the consumer (DBUSXX_SERVICE_NAME)
file(READ "${OUT_DIR}/CalculatorSkeleton.hpp" SKELETON)
foreach(NEEDLE
    "#include <dbusxx/Server.hpp>"
    "#include \"Types.hpp\""
    "DBUSXX_SERVICE_NAME"
    "DBUSXX_METHOD(notify)"
    "DBUSXX_PROPERTY_RW(samples, std::vector<std::int32_t>, {1, 2, 3})"
    "DBUSXX_PROPERTY_RW(history, std::vector<Point>, {{1, 2}, {3, 4}})"
    "DBUSXX_PROPERTY_RW(untouched, std::int32_t, std::int32_t{})"
    "DBUSXX_PROPERTY_RW(tags, decltype(std::map<std::string, std::string>{}), {})"
    "class CalculatorServer final : public Dbusxx::Server<CalculatorServer>")
    string(FIND "${SKELETON}" "${NEEDLE}" FOUND_AT)
    if(FOUND_AT EQUAL -1)
        message(FATAL_ERROR "CalculatorSkeleton.hpp misses '${NEEDLE}'")
    endif()
endforeach()

file(READ "${OUT_DIR}/CalculatorProxy.hpp" PROXY)
foreach(NEEDLE
    "#include <dbusxx/Client.hpp>"
    "#include <functional>"
    "class CalculatorProxy"
    "callSync"
    "[[deprecated]]"
    "Dbusxx::Reply<std::int32_t> syncOnly(std::int32_t val)"
    "Dbusxx::PendingReply<std::int32_t> addAsync"
    "Dbusxx::PendingReply<bool> asyncOnlyAsync"
    "Dbusxx::Status addAsync(std::function<void(Dbusxx::Reply<std::int32_t>)> aCallback"
    "Dbusxx::Status asyncOnlyAsync(std::function<void(Dbusxx::Reply<bool>)> aCallback"
    "Dbusxx::Status onValueChanged(std::function<void("
    "std::int32_t newVal)> aCallback)"
    "mClient.listenSignal(\"valueChanged\", std::move(aCallback))"
    "mClient.callSync<void, 500000>(\"ping\")"
    "mClient.callAsync<bool>(\"asyncOnly\", std::move(aCallback), val)")
    string(FIND "${PROXY}" "${NEEDLE}" FOUND_AT)
    if(FOUND_AT EQUAL -1)
        message(FATAL_ERROR "CalculatorProxy.hpp misses '${NEEDLE}'")
    endif()
endforeach()

message(STATUS "cli outputs ok")
