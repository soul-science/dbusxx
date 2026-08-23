# MetaObject 反射元对象与宏

> 对应头文件：`library/include/MetaObject.hpp`，公开命名空间：`Dbusxx`

## 简介

`MetaObject<Derived>` 是 CRTP 基类，为派生类收集反射元数据。派生类用 `DBUSXX_*` 宏标注成员（方法/信号/属性），`Session::registerObject()` 会消费这些元数据，一步完成接口注册。

通常你不需要直接使用 `MetaObject` —— 继承 `Server<Derived>` 时会自动获得该能力；若使用 `Session::registerObject` 则需自行继承 `MetaObject<Derived>`。

## 模板类：`MetaObject<Derived>`

```cpp
template<typename Derived>
class MetaObject {
protected:
    using Self = Derived;
    // 内部维护 per-派生类的注册表 registry()
    static std::vector<MethodEntry>& registry();
};
```

`registry()` 是每个派生类一份的静态注册表，由宏在静态初始化阶段填充，`Session::registerObject()` 遍历它完成注册。

## 注解宏

### `DBUSXX_PATH(aPath)`

设置后续标注所属的对象路径。作用于当前派生类。

```cpp
#define DBUSXX_PATH(aPath)
```

### `DBUSXX_IFACE(aIface)`

设置后续标注所属的接口名。作用于当前派生类。

```cpp
#define DBUSXX_IFACE(aIface)
```

### `DBUSXX_METHOD(aMethod)`

把成员函数 `aMethod` 暴露为 D-Bus 方法（参数与返回类型由函数签名自动推导）。

```cpp
#define DBUSXX_METHOD(aMethod)
```

### `DBUSXX_SIGNAL(aSignal, ...)`

注册一个 D-Bus 信号，`...` 为信号参数类型列表（如 `int32_t, std::string`）。

```cpp
#define DBUSXX_SIGNAL(aSignal, ...)
```

### `DBUSXX_PROPERTY_RO(aName, aType, aInitValue)`

注册一个只读属性，包装器持有自己的值副本。

```cpp
#define DBUSXX_PROPERTY_RO(aName, aType, aInitValue)
```

### `DBUSXX_PROPERTY_RW(aName, aType, aInitValue)`

注册一个读写属性，包装器持有自己的值副本。

```cpp
#define DBUSXX_PROPERTY_RW(aName, aType, aInitValue)
```

## API 示例（逐项）

所有宏都必须在继承 `MetaObject<Derived>` 的类内部使用，下面用注释标注每个宏：

```cpp
#include <dbusxx/MetaObject.hpp>
#include <dbusxx/Session.hpp>

using namespace Dbusxx;

class Calc : public MetaObject<Calc> {
public:
    // (1) DBUSXX_PATH(...) —— 设置后续标注的对象路径
    DBUSXX_PATH("/com/example/calc")

    // (2) DBUSXX_IFACE(...) —— 设置后续标注的接口名
    DBUSXX_IFACE("com.example.Calc")

    // (3) DBUSXX_METHOD(...) —— 暴露成员函数为 D-Bus 方法
    int32_t add(int32_t a, int32_t b) { return a + b; }
    DBUSXX_METHOD(add)

    // (4) DBUSXX_SIGNAL(name, 类型列表...) —— 注册信号
    DBUSXX_SIGNAL(valueChanged, int32_t, int32_t)

    // (5) DBUSXX_PROPERTY_RO(name, 类型, 初值) —— 只读属性
    DBUSXX_PROPERTY_RO(version, std::string, std::string("1.0.0"))

    // (6) DBUSXX_PROPERTY_RW(name, 类型, 初值) —— 读写属性
    DBUSXX_PROPERTY_RW(counter, int32_t, 0)
};

int main() {
    Session sess = Session::userSession("com.example.Calc");
    Calc calc;
    // 一步注册全部标注成员
    auto st = sess.registerObject("/com/example/calc", "com.example.Calc", &calc);
    // ...
}
```

## 注意事项

- `DBUSXX_PATH` / `DBUSXX_IFACE` 作用于其后所有标注，直到被新的设置覆盖。
- 属性宏的包装器**拥有自己的值副本**，与类成员相互独立。
- 使用 `Server<Derived>` 时无需手动调用 `registerObject`，`run()` 会基于宏注册。
- 宏依赖 `Self`（即 `Derived`），因此只能用在继承 `MetaObject<Derived>` 的类内部。
