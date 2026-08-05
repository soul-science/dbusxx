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

struct VTableContext {
    using VTablePtr = std::unique_ptr<Adaptor::RawBusVTable[]>;
    VTablePtr vtable;
    Adaptor::RawSlotSharePtr slot;
    std::string name;
    std::string input;
    std::string output;
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
    VTableRegistrar(const Adaptor::RawBusSharePtr& aBus, std::string_view aPath, std::string_view aIface)
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

    Status commit(std::vector<std::unique_ptr<VTableContext>>& aVTables) {
        for (auto& entry: mEntries) {
            auto ctx = std::make_unique<VTableContext>();
            ctx->name = std::move(entry.name);
            ctx->input = std::move(entry.input);
            ctx->output = std::move(entry.output);

            Adaptor::RawBusVTable item;
            switch (entry.type) {
                case Type::METHOD: {
                    item = SD_BUS_METHOD(ctx->name.c_str(), ctx->input.c_str(),
                        ctx->output.c_str(), entry.callback, 0);
                    break;
                }
                case Type::SIGNAL: {
                    item = SD_BUS_SIGNAL(ctx->name.c_str(), ctx->input.c_str(), 0);
                    break;
                }
                case Type::PROPERTY_RO: {
                    item = SD_BUS_PROPERTY(
                        ctx->name.c_str(), ctx->input.c_str(),
                        entry.getter, 0,
                        SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE
                    );
                    break;
                }
                case Type::PROPERTY_RW: {
                    item = SD_BUS_WRITABLE_PROPERTY(
                        ctx->name.c_str(), ctx->input.c_str(),
                        entry.getter, entry.setter, 0,
                        SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE
                    );
                    break;
                }
                default:
                    break;
            }

            ctx->vtable.reset(new Adaptor::RawBusVTable[3] {
                SD_BUS_VTABLE_START(0), item, SD_BUS_VTABLE_END
            });

            Adaptor::RawBusSlotPtr rawSlot{nullptr};
            Status st = Adaptor::RawBus::addObjectToVTable(
               mBus.get(), rawSlot, mPath.c_str(), mIface.c_str(),
               ctx->vtable.get(), entry.data
            );
            if (st.isError()) {
                return st;
            }

            ctx->slot = Adaptor::RawSlotSharePtr(rawSlot);
            aVTables.push_back(std::move(ctx));
        }

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