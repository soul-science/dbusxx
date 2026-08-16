
#ifndef SSDBUS_DBUS_RETURN_STATUS_HPP
#define SSDBUS_DBUS_RETURN_STATUS_HPP

#include <string>


namespace SSDbus {
enum class StatusCode : uint8_t {
    SUCCESS = 0,

    // --- 调用方错误 ---
    INVALID_ARG,        // 参数无效
    NOT_FOUND,          // 服务/对象/接口不存在
    NO_SERVICE,         //! not found service
    NO_METHOD,          //! not found method/interface/path
    ACCESS_DENIED,      // 权限不足
    NAME_EXISTS,        // 总线名已被占用

    // --- 连接错误 ---
    NOT_CONNECTED,      // 未连接到总线
    CONN_RESET,         // 连接被重置
    BUSY,               // 资源忙

    // --- 传输错误 ---
    TIMEOUT,            // 调用超时
    NO_MEMORY,          // 内存不足
    NO_REPLY,           // 对方未回复
    IO_ERROR,           // I/O 错误
    MSG_TOO_LONG,       // 消息超长
    LIMIT_EXCEEDED,     // 超出限制

    // --- D-Bus 协议错误 ---
    PROTOCOL_ERROR,     // 协议错误
    TYPE_MISMATCH,
    DISCONNECTED,       // 对端断开

    // --- 未知 ---
    UNKNOWN_ERROR       // 未知错误（兜底）
};

inline constexpr const char* statusMessage(StatusCode aCode) {
    switch (aCode) {
        case StatusCode::SUCCESS:
            return "Success";
        case StatusCode::INVALID_ARG:
            return "Invalid argument";
        case StatusCode::NOT_FOUND:
            return "Service/object not found";
        case StatusCode::NO_SERVICE:
            return "Service not found";
        case StatusCode::NO_METHOD:
            return "Method not found, maybe path/interface/method error";
        case StatusCode::ACCESS_DENIED:
            return "Access denied";
        case StatusCode::NAME_EXISTS:
            return "name already taken";
        case StatusCode::NOT_CONNECTED:
            return "Not connected to bus";
        case StatusCode::CONN_RESET:
            return "Connection reset";
        case StatusCode::BUSY:
            return "Resource busy";
        case StatusCode::TIMEOUT:
            return "Operation timed out";
        case StatusCode::NO_MEMORY:
            return "Out of memory";
        case StatusCode::NO_REPLY:
            return "No reply received";
        case StatusCode::IO_ERROR:
            return "I/O error";
        case StatusCode::MSG_TOO_LONG:
            return "Message too long";
        case StatusCode::LIMIT_EXCEEDED:
            return "Limit exceeded";
        case StatusCode::PROTOCOL_ERROR:
            return "Protocol error";
        case StatusCode::DISCONNECTED:
            return "Peer disconnected";
        case StatusCode::TYPE_MISMATCH:
            return "Type mismatch";
        case StatusCode::UNKNOWN_ERROR:
            return "Unknown error";
    }

    return "Unknown";
}

class Status {
public:
    Status() = default;

    Status(StatusCode aCode)
        : mCode(aCode) {}

    inline StatusCode code() const {
        return mCode;
    }

    inline bool isSuccess() const {
        return mCode == StatusCode::SUCCESS;
    
    }

    inline bool isError() const {
        return mCode != StatusCode::SUCCESS;
    }

    inline std::string message() const {
        return statusMessage(mCode);
    }

private:
    StatusCode mCode { StatusCode::SUCCESS };
};

}

#endif