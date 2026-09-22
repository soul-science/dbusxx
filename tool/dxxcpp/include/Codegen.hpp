#ifndef DXXCPP_CODEGEN_HPP
#define DXXCPP_CODEGEN_HPP

#include <string>

#include "Ir.hpp"


namespace Codegen {
//! Generate types header file name by package
//! com.example.calc -> ComExampleCalcTypes.hpp
std::string typesHeaderName(const Ir::Root& aRoot);

//! Types Generator: struct + using + static_assert
std::string genTypesHeader(const Ir::Root& aRoot);

//! Per-interface generated file names
//! <Interface>Skeleton.hpp / <Interface>Skeleton.cpp
//! <Interface>Proxy.hpp / <Interface>Proxy.cpp
std::string skeletonHeaderName(const Ir::Interface& aIface);
std::string skeletonSourceName(const Ir::Interface& aIface);
std::string proxyHeaderName(const Ir::Interface& aIface);
std::string proxySourceName(const Ir::Interface& aIface);

//! Skeleton Header Generator: declaration for Interface & Server(CRTP + DBUSXX_*)
std::string genSkeletonHeader(const Ir::Root& aRoot, const Ir::Interface& aIface);

//! Skeleton Source Generator: definition for Server
std::string genSkeletonSource(const Ir::Root& aRoot, const Ir::Interface& aIface);

//! Proxy Header Generator: declarations of client API
std::string genProxyHeader(const Ir::Root& aRoot, const Ir::Interface& aIface);

//! Proxy Source Generator: definition of client API
std::string genProxySource(const Ir::Root& aRoot, const Ir::Interface& aIface);
} // namespace Codegen
#endif
