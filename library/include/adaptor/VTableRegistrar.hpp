#ifndef SSDBUS_VTABLE_REGISTRAR_HPP
#define SSDBUS_VTABLE_REGISTRAR_HPP

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "adaptor/RawCommon.hpp"
#include "adaptor/RawBusSharePtr.hpp"
#include "adaptor/RawSlotSharePtr.hpp"

namespace SSDbus {
namespace Adaptor {

// struct VTableContext {
//     using VTablePtr = std::unique_ptr<Adaptor::RawBusVTable[]>;
//     VTablePtr vtable;
//     Adaptor::RawSlotSharePtr slot;
//     std::string name;
//     std::string input;
//     std::string output;
// };

struct VTableData {
    Adaptor::RawBusVTable vtable;
    void* userdata;
};

struct VTableContext {
    using VTablePtr = std::unique_ptr<Adaptor::VTableData[]>;
    VTablePtr vtable;
    Adaptor::RawSlotSharePtr slot;
    std::vector<std::string> names;
    std::vector<std::string> inputs;
    std::vector<std::string> outputs;
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
        std::string_view aPath, std::string_view aIface)
        : mBus(aBus)
        , mPath(aPath)
        , mIface(aIface) {}

    VTableRegistrar& addMethod(std::string_view aFunc, std::string_view aInput, 
        std::string_view aOutput, Adaptor::RawBusMessageHandler aCallback, void* aData) {
        mEntries.push_back({
            std::string(aFunc),
            std::string(aInput),
            std::string(aOutput),
            aCallback, nullptr, nullptr, aData , Type::METHOD
        });

        return *this;
    }

    VTableRegistrar& addSiganl(std::string_view aSignal, std::string_view aInput) {
        mEntries.push_back({
            std::string(aSignal),
            std::string(aInput),
            "", nullptr, nullptr, nullptr, nullptr, Type::SIGNAL
        });

        return *this;
    }

    VTableRegistrar& addProperty(std::string_view aProperty, std::string_view aInput,
        Adaptor::RawBusPropertyGetter aGetter, Adaptor::RawBusPropertySetter aSetter, void* aData, bool writable = true) {
        mEntries.push_back({
            std::string(aProperty),
            std::string(aInput),
            "", nullptr, aGetter, aSetter, aData,
            writable ? Type::PROPERTY_RW : Type::PROPERTY_RO
        });
        return *this;
    }

    Status commit(std::unique_ptr<VTableContext>& aCtx) {
        aCtx = std::make_unique<VTableContext>();
        auto vtables = std::make_unique<VTableData[]>(mEntries.size() + 2);

        vtables[0].vtable = {};
        vtables[0].vtable.type = _SD_BUS_VTABLE_START;
        vtables[0].vtable.flags = 0;
        vtables[0].vtable.x.start = {
            .element_size = sizeof(VTableData),
            .features = _SD_BUS_VTABLE_PARAM_NAMES,
            .vtable_format_reference = &sd_bus_object_vtable_format,
        };
        vtables[0].userdata = nullptr;

        size_t n = mEntries.size();
        for (size_t i = 0; i < n; ++i) {
            auto& entry = mEntries[i];
            aCtx->names.push_back(std::move(entry.name));
            aCtx->inputs.push_back(std::move(entry.input));
            aCtx->outputs.push_back(std::move(entry.output));

            size_t offset = offsetof(VTableData, userdata);
            switch (entry.type) {
                case Type::METHOD: {
                    vtables[i + 1].vtable = SD_BUS_METHOD_WITH_OFFSET(
                        aCtx->names.back().c_str(),
                        aCtx->inputs.back().c_str(),
                        aCtx->outputs.back().c_str(),
                        entry.callback,
                        offset,
                        SD_BUS_VTABLE_ABSOLUTE_OFFSET
                    );
                    break;
                }
                case Type::SIGNAL: {
                    vtables[i + 1].vtable = SD_BUS_SIGNAL(
                        aCtx->names.back().c_str(),
                        aCtx->inputs.back().c_str(), 0
                    );
                    break;
                }
                case Type::PROPERTY_RO: {
                    vtables[i + 1].vtable = SD_BUS_PROPERTY(
                        aCtx->names.back().c_str(),
                        aCtx->inputs.back().c_str(),
                        mEntries[i].getter,
                        offset,
                        SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE | SD_BUS_VTABLE_ABSOLUTE_OFFSET
                    );
                    break;
                }
                case Type::PROPERTY_RW: {
                    vtables[i + 1].vtable = SD_BUS_WRITABLE_PROPERTY(
                        aCtx->names.back().c_str(),
                        aCtx->inputs.back().c_str(),
                        mEntries[i].getter, mEntries[i].setter,
                        offset,
                        SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE | SD_BUS_VTABLE_ABSOLUTE_OFFSET
                    );
                    break;
                }
                default:
                    break;
            }

            vtables[i + 1].userdata = entry.data;
        }

        vtables[n + 1].vtable = SD_BUS_VTABLE_END;
        vtables[n + 1].userdata = nullptr;
        Adaptor::RawBusSlotPtr rawSlot{nullptr};
        Status st = Adaptor::RawBus::addObjectToVTable(
            mBus.get(), rawSlot, mPath.c_str(), mIface.c_str(),
            &vtables[0].vtable, nullptr
        );
        if (st.isError()) {
            return st;
        }

        aCtx->slot = Adaptor::RawSlotSharePtr(rawSlot);
        aCtx->vtable.reset(
            reinterpret_cast<VTableData*>(vtables.release()));

        return Status(StatusCode::SUCCESS);
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