# MetaObject — Reflection Meta Object & Macros

> Header: `library/include/MetaObject.hpp` · public namespace: `Dbusxx`

## Overview

`MetaObject<Derived>` is a CRTP base class that collects reflection metadata for the derived class. The derived class annotates members (methods/signals/properties) with the `DBUSXX_*` macros, and `Session::registerObject()` consumes that metadata to register the interface in one step.

Usually you don't need to use `MetaObject` directly — deriving from `Server<Derived>` gives you this capability automatically; if you use `Session::registerObject`, you must derive from `MetaObject<Derived>` yourself.

## Template class: `MetaObject<Derived>`

```cpp
template<typename Derived>
class MetaObject {
protected:
    using Self = Derived;
    // Internal per-derived-class registry registry()
    static std::vector<MethodEntry>& registry();
};
```

`registry()` is one static registry per derived class, populated by the macros during static initialization; `Session::registerObject()` iterates it to perform the registration.

## Annotation macros

### `DBUSXX_PATH(aPath)`

Sets the object path for subsequent annotations in this class.

```cpp
#define DBUSXX_PATH(aPath)
```

### `DBUSXX_IFACE(aIface)`

Sets the interface name for subsequent annotations in this class.

```cpp
#define DBUSXX_IFACE(aIface)
```

### `DBUSXX_METHOD(aMethod)`

Exposes the member function `aMethod` as a D-Bus method (argument and return types are deduced from the function signature).

```cpp
#define DBUSXX_METHOD(aMethod)
```

### `DBUSXX_SIGNAL(aSignal, ...)`

Registers a D-Bus signal; `...` is the list of signal argument types (e.g. `int32_t, std::string`).

```cpp
#define DBUSXX_SIGNAL(aSignal, ...)
```

### `DBUSXX_PROPERTY_RO(aName, aType, aInitValue)`

Registers a read-only property; the wrapper owns its own copy of the value.

```cpp
#define DBUSXX_PROPERTY_RO(aName, aType, aInitValue)
```

### `DBUSXX_PROPERTY_RW(aName, aType, aInitValue)`

Registers a read-write property; the wrapper owns its own copy of the value.

```cpp
#define DBUSXX_PROPERTY_RW(aName, aType, aInitValue)
```

## Per-API Examples

All macros must be used inside a class deriving from `MetaObject<Derived>`; each macro is annotated with a comment below:

```cpp
#include <dbusxx/MetaObject.hpp>
#include <dbusxx/Session.hpp>

using namespace Dbusxx;

class Calc : public MetaObject<Calc> {
public:
    // (1) DBUSXX_PATH(...) — set the object path for subsequent annotations
    DBUSXX_PATH("/com/example/calc")

    // (2) DBUSXX_IFACE(...) — set the interface name for subsequent annotations
    DBUSXX_IFACE("com.example.Calc")

    // (3) DBUSXX_METHOD(...) — expose a member function as a D-Bus method
    int32_t add(int32_t a, int32_t b) { return a + b; }
    DBUSXX_METHOD(add)

    // (4) DBUSXX_SIGNAL(name, type-list...) — register a signal
    DBUSXX_SIGNAL(valueChanged, int32_t, int32_t)

    // (5) DBUSXX_PROPERTY_RO(name, type, init) — read-only property
    DBUSXX_PROPERTY_RO(version, std::string, std::string("1.0.0"))

    // (6) DBUSXX_PROPERTY_RW(name, type, init) — read-write property
    DBUSXX_PROPERTY_RW(counter, int32_t, 0)
};

int main() {
    Session sess = Session::userSession("com.example.Calc");
    Calc calc;
    // Register all annotated members in one step
    auto st = sess.registerObject("/com/example/calc", "com.example.Calc", &calc);
    // ...
}
```

## Notes

- `DBUSXX_PATH` / `DBUSXX_IFACE` apply to all subsequent annotations until overridden by a new setting.
- Property wrappers **own their own copy of the value**, independent of class members.
- When using `Server<Derived>`, you don't need to call `registerObject` manually — `run()` registers based on the macros.
- The macros depend on `Self` (i.e. `Derived`), so they can only be used inside a class deriving from `MetaObject<Derived>`.
