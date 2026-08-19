#ifndef DBUSXX_SIGNAL_HANDLER_HPP
#define DBUSXX_SIGNAL_HANDLER_HPP

#include <tuple>

#include "private/adaptor/RawCommon.hpp"
#include "private/method/FunctionTrait.hpp"
#include "private/message/MessagePrivate.hpp"

#include "Args.hpp"


namespace Dbusxx {
namespace Private {
template<typename Callback>
struct SignalHandler {
    Callback callback;

    using traits = Method::FuncTrait<std::decay_t<Callback>>;

    SignalHandler(Callback aCb) : callback(std::move(aCb)) {}

    static int onSignal(Adaptor::RawBusMessagePtr aMsg, void* aUsr, Adaptor::RawBusErrorPtr) {
        auto self = static_cast<SignalHandler*>(aUsr);
        Private::MessagePrivate message(Adaptor::RawMessageSharePtr(aMsg, false));

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