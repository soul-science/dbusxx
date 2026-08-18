#ifndef DBUSXX_VTABLE_REGISTRAR_HPP
#define DBUSXX_VTABLE_REGISTRAR_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "private/adaptor/RawCommon.hpp"
#include "private/adaptor/RawBusSharePtr.hpp"
#include "private/adaptor/RawSlotSharePtr.hpp"


namespace Dbusxx {
namespace Adaptor {
struct VTableContext {
    using VTablePtr = std::unique_ptr<Adaptor::RawBusVTable[]>;
    VTablePtr vtables;
    std::unique_ptr<void*[]> datas;
    std::vector<std::string> names;
    std::vector<std::string> inputs;
    std::vector<std::string> outputs;
    Adaptor::RawSlotSharePtr slot;
};

class VTableRegistrar {
    enum class Type : uint8_t {
        METHOD = 0,
        SIGNAL,
        PROPERTY_RO,
        PROPERTY_RW
    };

    struct VTableEntry {
        std::string name;
        std::string input;
        std::string output;
        Adaptor::RawBusMessageHandler callback;
        Adaptor::RawBusPropertyGetter getter;
        Adaptor::RawBusPropertySetter setter;
        void* data;
        Type type;
    };

public:
    VTableRegistrar(const Adaptor::RawBusSharePtr& aBus,
        std::string_view aPath, std::string_view aIface);

    VTableRegistrar& addMethod(std::string_view aFunc, std::string_view aInput, 
        std::string_view aOutput, Adaptor::RawBusMessageHandler aCallback, void* aData);

    VTableRegistrar& addSiganl(std::string_view aSignal, std::string_view aInput);

    VTableRegistrar& addProperty(std::string_view aProperty, std::string_view aInput,
        Adaptor::RawBusPropertyGetter aGetter, Adaptor::RawBusPropertySetter aSetter,
        void* aData, bool writable = true);

    Status commit(std::unique_ptr<VTableContext>& aCtx);

    inline std::string path() const {
        return mPath;
    }

    inline std::string interface() const {
        return mIface;
    }

private:
    Adaptor::RawBusSharePtr mBus;
    std::string mPath;
    std::string mIface;
    std::vector<VTableEntry> mEntries;
};

}

}
#endif