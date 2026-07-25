#ifndef SSDBUS_SIGNAL_HANDLER_HPP
#define SSDBUS_SIGNAL_HANDLER_HPP

#include "adaptor/RawCommon.hpp"
#include "method/FunctionTrait.hpp"
#include "adaptor/DbusArgs.hpp"

namespace SSDbus {
namespace Private {

template<typename Callback>
struct SignalHandler {
    Callback callback;
    Adaptor::RawBusSlotPtr slot { nullptr };

    SignalHandler(Callback aCb) : callback(std::move(aCb)) {}

    static int onSignal(Adaptor::RawBusMessagePtr aMsg, void* aUsr, Adaptor::RawBusErrorPtr) {
        auto self = static_cast<SignalHandler*>(aUsr);
        Private::MessagePrivate message(Adaptor::RawMessageSharePtr(aMsg, false));

        using callbackType = std::decay_t<Callback>;
        using traits = Method::FuncTrait<callbackType>;

        auto impl = [&]<typename... Args>(std::tuple<Args...>*) {
            if constexpr (traits::argSize) {
                std::tuple<typename ArgTypeAdaptor<std::decay_t<Args>>::type...> tpl;
                message.read(tpl);
                std::apply(self->callback, tpl);
            } else {
                self->callback();
            }

            return 0;
        };


        return impl(static_cast<typename traits::ArgsTuple*>(nullptr));
    }

};

}
}


#endif