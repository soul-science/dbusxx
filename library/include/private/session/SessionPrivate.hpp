#ifndef DBUSXX_SESSION_PRIVATE_HPP
#define DBUSXX_SESSION_PRIVATE_HPP

#include <cstdint>
#include <string>
#include <string_view>
#include <functional>
#include <memory>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <unordered_map>
#include <utility>
#include <vector>

#include "private/adaptor/RawCommon.hpp"
#include "private/adaptor/RawSlotSharePtr.hpp"
#include "private/adaptor/RawBusSharePtr.hpp"
#include "private/message/MessagePrivate.hpp" 
#include "private/adaptor/VTableRegistrar.hpp"
#include "Status.hpp"
#include "Utils.hpp"


namespace Dbusxx {
namespace Private {
class SessionPrivate {
public:
    struct MethodEntry {
        std::shared_ptr<void> data;
        std::string input;
        std::string output;
        Adaptor::RawBusMessageHandler callback;
    };

    struct SignalEntry {
        std::string input;
    };

    struct PropertyEntry {
        std::shared_ptr<void> data;
        std::string signature;
        bool writable;
        Adaptor::RawBusPropertyGetter getter;
        Adaptor::RawBusPropertySetter setter;
    };

    //! (path, interface) : ObjectInfo
    struct ObjectInfo {
        std::unique_ptr<Adaptor::VTableContext> context;
        std::unique_ptr<Adaptor::VTableRegistrar> reg;
        std::unordered_map<std::string, MethodEntry> methods;
        std::unordered_map<std::string, SignalEntry> signals;
        std::unordered_map<std::string, PropertyEntry> properties;

        static std::string makeKey(std::string_view path, std::string_view iface);

        static std::pair<std::string_view, std::string_view> parseKey(std::string_view key);
    };

    struct SignalHandlerInfo {
        std::string sender;
        std::string path;
        std::string iface;
        std::string signal;
        Adaptor::RawBusMessageHandler callback;
        std::shared_ptr<void> data;
        Adaptor::RawSlotSharePtr slot;
    };

    struct PropertyHandlerInfo {
        Adaptor::RawSlotSharePtr slot;
        std::shared_ptr<void> handler; 
    };

    using ObjectMap = std::unordered_map<std::string, ObjectInfo>;
    using PropertyHandlerMap = std::unordered_map<std::string, PropertyHandlerInfo>;
    using SignalHandlerVector = std::vector<SignalHandlerInfo>;

    SessionPrivate() = default;

    explicit SessionPrivate(SessionType aType, std::string_view aServiceName, bool aIsServer = false);

    ~SessionPrivate();

    inline Adaptor::RawBusSharePtr rawBus() const {
        return mRawBus;
    }

    inline std::string serviceName() const {
        return mServiceName;
    }

    inline SessionType type() const {
        return mType;
    }

    inline ObjectMap& objects() {
        return mRegisteredObjects;
    }

    inline SignalHandlerVector& signalHandlers() {
        return mRegisteredSigHandlers;
    }

    inline PropertyHandlerMap& propertyHandlers() {
        return mRegisteredPropHandlers;
    }

    inline void addSignalHandlerInfo(SignalHandlerInfo aInf) {
        mRegisteredSigHandlers.push_back(std::move(aInf));
    }

    inline void setPropertyHandlerInfo(std::string_view aKey, PropertyHandlerInfo aInf) {
        mRegisteredPropHandlers[std::string(aKey)] = std::move(aInf);
    }

    inline int listenFd() const {
        return mListenFd;
    }

    inline void setPeerAcceptedCallback(std::function<Status()> aCb) {
        mPeerAcceptedCb = std::move(aCb);
    }

    //! Dispatcher that runs a task on the thread driving this session (the
    //! Looper thread; registered by LooperPrivate::run). Async-reply handlers
    //! use it so their destruction — sd-bus slot/message unref, which touches
    //! non-atomic sd-bus refcounts — always happens on the sd-bus processing
    //! thread, never on whichever thread happens to release the last
    //! PendingReply. Empty when the session is driven directly (no Looper);
    //! callers must then release pending handles on the bus-processing thread.
    using OwnerPoster = std::function<void(std::function<void()>)>;

    void setOwnerPoster(OwnerPoster aPoster) {
        mOwnerPoster = std::move(aPoster);
    }

    inline const OwnerPoster& ownerPoster() const {
        return mOwnerPoster;
    }

    void addMethodEntry(const std::string& aKey, std::string_view aName,
        std::string aInput, std::string aOutput,
        Adaptor::RawBusMessageHandler aCallback, void* aData,
        std::shared_ptr<void> aDataHolder);

    void addSignalEntry(const std::string& aKey, std::string_view aName, std::string aInput);

    void addPropertyEntry(const std::string& aKey,
        std::string_view aName, std::string aSignature, bool aWritable,
        Adaptor::RawBusPropertyGetter, Adaptor::RawBusPropertySetter,
        void* aData, std::shared_ptr<void> aDataHolder);

    Status commitBuilder(const std::string& aKey);

    Status reconnect();

    Status requestNameToDaemon();

    Status acceptPeerConnection(Adaptor::RawBusEventPtr aEvent);

    MessagePrivate createReply(MessagePrivate& aCallMsg);

    MessagePrivate createMethodCall(std::string_view aService, std::string_view aPath,
        std::string_view aIface, std::string_view aMethod);

    Status sendMessage(MessagePrivate& aMsg);

    Status sendMessage(MessagePrivate& aMsg, std::string_view aDestination);

    int process();

    int wait(uint64_t aTimeoutUsec = UINT64_MAX);

    int getFd() const;

    void flush();

    std::shared_ptr<void> getPropertyHandler(std::string_view aKey) const {
        auto it = mRegisteredPropHandlers.find(std::string(aKey));
        if (it == mRegisteredPropHandlers.end()) { 
            return nullptr;
        }

        return it->second.handler;
    }

private:
    static Adaptor::RawBusSharePtr makePrivate(SessionType aType,
        std::string_view aServiceName = "", bool aIsServer = false, int* aListenFd = nullptr);

    void setDaemonDeathWatcher();

    int mListenFd{-1};
    Adaptor::RawBusSharePtr mRawBus { nullptr };
    SessionType mType { false };
    std::string mServiceName;
    bool mIsServer{false};
    std::function<Status()> mPeerAcceptedCb;
    OwnerPoster mOwnerPoster;

    ObjectMap mRegisteredObjects;
    SignalHandlerVector mRegisteredSigHandlers;
    PropertyHandlerMap mRegisteredPropHandlers;
};

}
}


#endif