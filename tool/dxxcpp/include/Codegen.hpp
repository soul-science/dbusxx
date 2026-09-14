#ifndef DXXCPP_CODEGEN_HPP
#define DXXCPP_CODEGEN_HPP

#include <string>

#include "Ir.hpp"


namespace Codegen {
//! Generate namespace by package
//! com.example.calc -> Com::Example::Calc
std::string cppNamespace(const std::string& aPackage);

//! Generate dbus path by package
//! com.example.calc -> /com/example/calc
std::string dbusPath(const std::string& aPackage);

//! Generate cpp type by ir
//! int32->std::int32_t, vector->std::vector<...>, ...
std::string cppType(const Ir::Root& aRoot, Ir::TypeId aId);

//! Types Generator: struct + using + static_assert
std::string genTypesHeader(const Ir::Root& aRoot);

//! Skeleton Generator: Abstract Interface & Server(CRTP + DBUSXX_*)
std::string genSkeletonHeader(const Ir::Root& aRoot, const Ir::Interface& aIface);

//! Proxy Generator
std::string genProxyHeader(const Ir::Root& aRoot, const Ir::Interface& aIface);
} // namespace Codegen
#endif
