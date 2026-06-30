#ifndef SSDBUS_VTABLE_REGISTRAR_HPP
#define SSDBUS_VTABLE_REGISTRAR_HPP

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "adaptor/RawAdaptor.hpp"
#include "SessionPrivate.hpp"

#include "DbusSlot.hpp"

namespace SSDbus {
namespace Private {

struct VTableContext {
    using VTablePtr = std::unique_ptr<Adaptor::RawBusVTable[]>;
    VTablePtr vtable;
    Slot slot;
    std::string func;
    std::string input;
    std::string output;
};

class VTableRegistrar {

    enum class Type : uint8_t {
        METHOD = 0,
        SIGNAL
    };

    struct VTableEntry {
        std::string func;
        std::string input;
        std::string output;
        Adaptor::RawBusMessageHandler callback;
        void* data;
        Type type;
    };

public:
    VTableRegistrar(SessionPrivate* aSession, std::string_view aPath, std::string_view aIface)
        : mSession(aSession)
        , mPath(aPath)
        , mIface(aIface) {}

    VTableRegistrar& addMethod(std::string_view aFunc, std::string_view aInput, 
        std::string_view aOutput, Adaptor::RawBusMessageHandler aCallback, void* aData) {
        mEntries.push_back({
            std::string(aFunc),
            std::string(aInput),
            std::string(aOutput),
            aCallback, aData, Type::METHOD
        });

        return *this;
    }

    VTableRegistrar& addSiganl(std::string_view aSignal, std::string_view aInput) {
        mEntries.push_back({
            std::string(aSignal),
            std::string(aInput),
            "", nullptr, nullptr, Type::SIGNAL
        });

        return *this;
    }

    Status commit(std::vector<std::unique_ptr<VTableContext>>& aVTables) {
        for (auto& entry: mEntries) {
            auto ctx = std::make_unique<VTableContext>();
            ctx->func = std::move(entry.func);
            ctx->input = std::move(entry.input);
            ctx->output = std::move(entry.output);
            std::string regName = ctx->func + std::string("_") + ctx->input;
            if (mSession->methods().count(regName)) {
                return Status(StatusCode::METHOD_EXISTS);  // 已存在，不覆盖
            }

            Adaptor::RawBusVTable item;
            switch (entry.type) {
                case Type::METHOD:
                    item = SD_BUS_METHOD(ctx->func.c_str(), ctx->input.c_str(),
                        ctx->output.c_str(), entry.callback, 0);
                    break;
                case Type::SIGNAL:
                item = SD_BUS_SIGNAL(ctx->func.c_str(), ctx->input.c_str(), 0);
                    break;
                default:
                    break;
            }

            ctx->vtable.reset(new Adaptor::RawBusVTable[3] {
                SD_BUS_VTABLE_START(0), item, SD_BUS_VTABLE_END
            });

            Adaptor::RawBusSlotPtr rawSlot{nullptr};
            Status st = Adaptor::RawBus::addObjectToVTable(
               mSession->rawBus(), rawSlot, mPath.c_str(), mIface.c_str(),
               ctx->vtable.get(), entry.data
            );
            if (st.isError()) {
                return st;
            }

            ctx->slot = Slot(rawSlot);
            aVTables.push_back(std::move(ctx));
        }

        return Status(StatusCode::SUCCESS);
    }


private:
    SessionPrivate* mSession;
    std::string mPath;
    std::string mIface;
    std::vector<VTableEntry> mEntries;
};

}

}
#endif