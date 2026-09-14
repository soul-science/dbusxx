#include "Codegen.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>

#include "Cpp.hpp"


namespace Codegen {
namespace {
constexpr std::size_t INDENT_SIZE { 4 };

std::string upper(std::string aStr) {
    std::transform(aStr.begin(), aStr.end(), aStr.begin(),
        [] (unsigned char aCh) -> char {
            return std::toupper(aCh);
        });
    return aStr;
}

std::string capital(std::string aStr) {
    if (!aStr.empty() && std::isalpha(aStr[0])) {
        aStr[0] = std::toupper(aStr[0]);
    }

    return aStr;
}

// com.example.calc -> COM_EXAMPLE_CALC
std::string guardPrefix(const std::string& aPackage) {
    std::string g;
    for (char c : aPackage) {
        g += (c == '.') ? '_' : c;
    }

    return upper(g);
}

//! Judge if the type is scalar
bool isScalarBase(const Ir::Root& aIr, Ir::TypeId aId) {
    const Ir::TypeNode& node = aIr.type(aId);
    const auto* base = std::get_if<Ir::TypeBase>(&node.kind);
    if (!base) {
        return false;
    }

    //! string/bytes -> const&
    static const std::unordered_set<std::string> SCALAR = {
        "int8", "int16", "int32", "int64",
        "uint8", "uint16", "uint32", "uint64",
        "float", "double", "bool"
    };

    return SCALAR.count(aIr.nameOf(base->name)) > 0;
}

//! Generate parameter
std::string paramDecl(const Ir::Root& aIr, const Ir::Field& aField) {
    const bool byValue = isScalarBase(aIr, aField.type);
    return (byValue ? cppType(aIr, aField.type) :
        "const " + cppType(aIr, aField.type) + "&") + " " + Cpp::ident(aField.name);
}

std::string callArgs(const std::vector<Ir::Parameter>& aParams) {
    std::string s;
    for (size_t i = 0; i < aParams.size(); ++i) {
        if (i) {
            s += ", ";
        }

        s += Cpp::ident(aParams[i].name);
    }

    return s;
}

//! Parameter wrapper: Convert any actual parameter into a string
//! Therefore, all placeholders for call points should use %s
class Arg {
public:
    Arg(const std::string& aValue)
        : mValue(aValue) {}

    Arg(std::string_view aValue)
    : mValue(aValue) {}

    Arg(const char* aValue)
        : mValue(aValue ? aValue : "") {}
    Arg(bool aValue)
        : mValue(aValue ? "true" : "false") {}

    template<typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    Arg(T aValue)
        : mValue(std::to_string(aValue)) {}

    const char* c_str() const {
        return mValue.c_str();
    }

private:
    std::string mValue;
};

//! Call snprintf with Arg array (all placeholders are processed as %s)
template <std::size_t... I>
int snprintfArgs(char* aBuf, std::size_t aSize,
  const std::string& aFormat, const Arg* aArgs, std::index_sequence<I...>) {
    return std::snprintf(aBuf, aSize, aFormat.c_str(), aArgs[I].c_str()...);
}

//! Format one line with indentation; returns "indent + text + \n"
template<typename ...Args>
std::string line(std::size_t aIndent,
  std::string_view aFormat, const Args&... aArgs) {
    const std::string fmt(aFormat);
    std::string body;
    if constexpr (sizeof...(aArgs) == 0) {
        body = fmt;
    } else {
        const Arg args[] = { Arg(aArgs)... };
        const auto seq = std::make_index_sequence<sizeof...(aArgs)>{};
        const int len = snprintfArgs(nullptr, 0, fmt, args, seq);
        if (len < 0) {
            return std::string(aIndent, ' ') + "\n";
        }

        std::string buf(static_cast<std::size_t>(len) + 1, '\0');
        snprintfArgs(buf.data(), buf.size(), fmt, args, seq);
        buf.pop_back();
        body = std::move(buf);
    }

    return std::string(aIndent, ' ') + body + "\n";
}
} // namespace

//! Generate namespace by package
std::string cppNamespace(const std::string& aPackage) {
    std::string space;
    size_t start = 0;
    while (start <= aPackage.size()) {
        size_t end = aPackage.find('.', start);
        std::string seg = aPackage.substr(start,
            end == std::string::npos ? std::string::npos : end - start);
        if (!space.empty()) {
            space += "::";
        }

        space += capital(seg);
        if (end == std::string::npos) {
            break;
        }

        start = end + 1;
    }
    return space;
}

//! Generate dbus path by package
std::string dbusPath(const std::string& aPackage) {
    std::string p = "/";
    for (char c : aPackage) {
        p += (c == '.') ? '/' : c;
    }
        
    return p;
}

//! Generate cpp type by ir
std::string cppType(const Ir::Root& aIr, Ir::TypeId aId) {
    static const std::unordered_map<std::string, std::string> TYPE_MAP = {
        {"int8", "std::int8_t"},
        {"int16", "std::int16_t"},
        {"int32", "std::int32_t"},
        {"int64", "std::int64_t"},
        {"uint8", "std::uint8_t"},
        {"uint16", "std::uint16_t"},
        {"uint32", "std::uint32_t"},
        {"uint64", "std::uint64_t"},
        {"float", "float"},
        {"double", "double"},
        {"bool", "bool"},
        {"string", "std::string"},
        {"bytes", "std::vector<std::uint8_t>"}
    };

    const Ir::TypeNode& node = aIr.type(aId);
    if (const auto* b = std::get_if<Ir::TypeBase>(&node.kind)) {
        auto it = TYPE_MAP.find(aIr.nameOf(b->name));
        return it != TYPE_MAP.end() ? it->second : aIr.nameOf(b->name);
    }

    if (const auto* v = std::get_if<Ir::TypeVector>(&node.kind)) {
        return "std::vector<" + cppType(aIr, v->element) + ">";
    }

    if (const auto* a = std::get_if<Ir::TypeArray>(&node.kind)) {
        return "std::array<" + cppType(aIr, a->element) + ", " + std::to_string(a->n) + ">";
    }

    if (const auto* m = std::get_if<Ir::TypeMap>(&node.kind)) {
        return "std::map<" + cppType(aIr, m->key) + ", " + cppType(aIr, m->value) + ">";
    }

    if (const auto* s = std::get_if<Ir::TypeStruct>(&node.kind)) {
        return aIr.structBy(s->def).name;
    }

    return "?";
}

//! Types Generator: struct + using + static_assert
std::string genTypesHeader(const Ir::Root& aIr) {
    std::string out;
    const std::string hppGuardPrefix = guardPrefix(aIr.package);
    const std::string space = cppNamespace(aIr.package);

    //! Definition of .hpp guard
    out += line(0, "#ifndef %s_TYPES_HPP", hppGuardPrefix);
    out += line(0, "#define %s_TYPES_HPP", hppGuardPrefix);
    out += '\n';

    //! Included header files
    out += line(0, "#include <array>");
    out += line(0, "#include <cstdint>");
    out += line(0, "#include <map>");
    out += line(0, "#include <string>");
    out += line(0, "#include <type_traits>");
    out += line(0, "#include <vector>");
    out += '\n';

    //! Definition of namespace
    out += line(0, "namespace %s {", space);

    //! Definition of structs
    for (const auto& st : aIr.structs) {
        out += line(0, "struct %s {", st.name);
        for (const auto& field : st.fields) {
            out += line(INDENT_SIZE, "%s %s;", cppType(aIr, field.type), Cpp::ident(field.name));
        }

        //! Generate operator==
        //! Use == to determine whether the value has changed
        //!   in the library's PropertyWrapper::set().
        //! However, C++17 does not automatically generate == for aggregates,
        //!   so it generates one by field (member functions do not break aggregates)
        //! Fields are guaranteed to be non-empty by Sema
        out += '\n';
        const std::size_t last = st.fields.size() - 1;
        out += line(INDENT_SIZE, "bool operator==(const %s& aOther) const {", st.name);
        for (std::size_t i = 0; i < st.fields.size(); ++i) {
            const std::string name = Cpp::ident(st.fields[i].name);
            out += line(i == 0 ? INDENT_SIZE * 2 : INDENT_SIZE * 3,
                "%s%s == aOther.%s%s",
                (i == 0 ? "return " : "&& "), name, name,
                (i == last ? ";" : ""));
        }

        out += line(INDENT_SIZE, "}");
        out += line(0, "};");
        out += '\n';
    }

    //! Definition of aliases
    for (const auto& alias : aIr.aliases) {
        out += line(0, "using %s = %s;", alias.name, cppType(aIr, alias.target));
    }
    if (!aIr.aliases.empty()) {
        out += '\n';
    }

    for (const auto& st : aIr.structs) {
        out += line(0,
            "static_assert(std::is_aggregate_v<%s>, \"%s must be an aggregate\");",
            st.name, st.name
        );
    }

    out += line(0, "} // namespace %s", space);
    out += line(0, "#endif");
    return out;
}

//! Skeleton Generator: Abstract Interface & Server(CRTP + DBUSXX_*)
std::string genSkeletonHeader(const Ir::Root& aIr, const Ir::Interface& aIfce) {
    std::string output;
    const std::string hppGuardPrefix = guardPrefix(aIr.package);
    const std::string hppGuardName = upper(aIfce.name);
    const std::string space = cppNamespace(aIr.package);
    const std::string ifceName = aIr.package + "." + aIfce.name;
    const std::string pathName = dbusPath(aIr.package);

    //! Definition of .hpp guard
    output += line(0, "#ifndef %s_%s_SKELETON_HPP", hppGuardPrefix, hppGuardName);
    output += line(0, "#define %s_%s_SKELETON_HPP", hppGuardPrefix, hppGuardName);
    output += '\n';

    //! Included header files
    output += line(0, "#include <dbusxx/Server.hpp>");
    output += line(0, "#include <dbusxx/Session.hpp>");
    output += line(0, "#include <memory>");
    output += '\n';

    //! 
    output += line(0, "#include \"Types.hpp\"");
    output += '\n';

    //! 
    output += line(0, "#ifndef DBUSXX_SERVICE_NAME");
    output += line(0, "#error \"DBUSXX_SERVICE_NAME must be defined (e.g. -DDBUSXX_SERVICE_NAME=\\\"com.example.app\\\")\"");
    output += line(0, "#endif");
    output += '\n';

    //! Definition of namespace
    output += line(0, "namespace %s {", space);
    output += '\n';

    //! Definition of Interface Class
    output += line(0, "class %sInterface {", aIfce.name);
    output += line(0, "public:");
    output += line(INDENT_SIZE, "virtual ~%sInterface() = default;", aIfce.name);
    for (const auto& m : aIfce.methods) {
        std::string ret = m.ret ? cppType(aIr, *m.ret) : "void";
        std::string decl;
        for (size_t i = 0; i < m.params.size(); ++i) {
            if (i) {
                decl += ", ";
            }

            decl += paramDecl(aIr, m.params[i]);
        }

        output += line(INDENT_SIZE, "virtual %s %s(%s) = 0;", ret, m.name, decl);
    }

    output += line(0, "};");
    output += '\n';

    //! Definition of Skeleton Class
    output += line(0, "class %sServer final : public Dbusxx::Server<%sServer> {",
              aIfce.name, aIfce.name);
    output += line(0, "public:");
    output += line(INDENT_SIZE, "explicit %sServer(std::unique_ptr<%sInterface> aIface)",
              aIfce.name, aIfce.name);
    output += line(INDENT_SIZE * 2, ": Dbusxx::Server<%sServer>(DBUSXX_SERVICE_NAME)", aIfce.name);
    output += line(INDENT_SIZE * 2, ", mIface(std::move(aIface)) {}");
    output += '\n';
    output += line(INDENT_SIZE, "DBUSXX_PATH(\"%s\")", pathName);
    output += line(INDENT_SIZE, "DBUSXX_IFACE(\"%s\")", ifceName);

    //! Definition of methods
    for (const auto& method : aIfce.methods) {
        std::string ret = method.ret ? cppType(aIr, *method.ret) : "void";
        std::string decl;
        for (size_t i = 0; i < method.params.size(); ++i) {
            if (i) {
                decl += ", ";
            }

            decl += paramDecl(aIr, method.params[i]);
        }

        output += '\n';
        if (method.deprecated) {
            //! Using comments instead of [[deprecated]]:
            //! DBUSXX_METHOD will refer &Self::<this function>
            output += line(INDENT_SIZE, "// @deprecated");
        }

        output += line(INDENT_SIZE, "%s %s(%s) {", ret, method.name, decl);
        std::string call = "mIface->" + method.name +
            "(" + callArgs(method.params) + ")";
        output += line(INDENT_SIZE * 2, "%s;", (method.ret ? "return " + call : call));
        output += line(INDENT_SIZE, "}");
        output += line(INDENT_SIZE, "DBUSXX_METHOD(%s)", method.name);
    }

    //! Definition of signals
    for (const auto& signal : aIfce.signals) {
        std::string types;
        for (size_t i = 0; i < signal.params.size(); ++i) {
            if (i) { types += ", "; }
            types += cppType(aIr, signal.params[i].type);
        }
        output += '\n';
        output += line(INDENT_SIZE, "DBUSXX_SIGNAL(%s%s)", signal.name,
                  (types.empty() ? "" : ", " + types));
    }

    //! Definition of properties
    for (const auto& prop : aIfce.properties) {
        const std::string ty = cppType(aIr, prop.type);
        //! DBUSXX_PROPERTY_*, if type contains ",", using decltype(...)
        const bool needDecltype = ty.find(',') != std::string::npos;
        const std::string tyUse = needDecltype ?
            ("decltype(" + ty + "{})") : ty;
        const std::string init = prop.defaultValue.empty() ?
            (needDecltype ? "{}" : (ty + "{}")) : prop.defaultValue;
        output += '\n';
        output += line(INDENT_SIZE, "DBUSXX_PROPERTY_%s(%s, %s, %s)",
            (prop.readonly ? "RO" : "RW"), prop.name, tyUse, init);
    }

    output += '\n';
    output += line(0, "private:");
    output += line(INDENT_SIZE, "std::unique_ptr<%sInterface> mIface;", aIfce.name);
    output += line(0, "};");
    output += '\n';

    output += line(0, "} // namespace %s", space);
    output += line(0, "#endif");
    return output;
}

//! Proxy Generator
std::string genProxyHeader(const Ir::Root& aIr, const Ir::Interface& aIfce) {
    std::string output;
    const std::string prefix = guardPrefix(aIr.package);
    const std::string ifaceU = upper(aIfce.name);
    const std::string ns = cppNamespace(aIr.package);
    const std::string fullIface = aIr.package + "." + aIfce.name;
    const std::string path = dbusPath(aIr.package);

    //! Definition of .hpp guard
    output += line(0, "#ifndef %s_%s_PROXY_HPP", prefix, ifaceU);
    output += line(0, "#define %s_%s_PROXY_HPP", prefix, ifaceU);
    output += '\n';

    //! Included header files
    output += line(0, "#include <dbusxx/Client.hpp>");
    output += line(0, "#include <dbusxx/Reply.hpp>");
    output += '\n';

    output += line(0, "#include \"Types.hpp\"");
    output += '\n';

    output += line(0, "#ifndef DBUSXX_SERVICE_NAME");
    output += line(0, "#error \"DBUSXX_SERVICE_NAME must be defined (e.g. -DDBUSXX_SERVICE_NAME=\\\"com.example.app\\\")\"");
    output += line(0, "#endif");
    output += '\n';

    //! Definition of namespace
    output += line(0, "namespace %s {", ns);
    output += '\n';

    //! Definition of class
    output += line(0, "class %sProxy {", aIfce.name);
    output += line(0, "public:");

    //! Definition of constructor
    output += line(INDENT_SIZE, "explicit %sProxy()", aIfce.name);
    output += line(INDENT_SIZE * 2, ": mClient(Dbusxx::SessionType::USER, DBUSXX_SERVICE_NAME,");
    output += line(INDENT_SIZE * 2, "\"%s\", \"%s\") {}", path, fullIface);
    output += '\n';
    output += line(INDENT_SIZE, "%sProxy(const %sProxy&) = delete;", aIfce.name, aIfce.name);
    output += line(INDENT_SIZE, "%sProxy& operator=(const %sProxy&) = delete;", aIfce.name, aIfce.name);
    output += line(INDENT_SIZE, "%sProxy(%sProxy&&) = default;", aIfce.name, aIfce.name);
    output += line(INDENT_SIZE, "%sProxy& operator=(%sProxy&&) = default;", aIfce.name, aIfce.name);
    output += '\n';

    //! Definition of methods
    for (const auto& method : aIfce.methods) {
        std::string decl;
        for (size_t i = 0; i < method.params.size(); ++i) {
            if (i) {
                decl += ", ";
            }

            decl += paramDecl(aIr, method.params[i]);
        }

        const std::string args = callArgs(method.params);
        const std::string replyType = method.ret ?
            ("Dbusxx::Reply<" + cppType(aIr, *method.ret) + ">")
            : "Dbusxx::Reply<void>";

        std::string note = "// " + std::string(method.ret ? "sync" : "oneway");
        if (method.timeoutUsec) {
            note += " [timeout=" + std::to_string(*method.timeoutUsec) + "us]";
        }

        output += line(INDENT_SIZE, "%s", note);

        //! Only use "deprecated" in Proxy to expose to user
        if (method.deprecated) {
            output += line(INDENT_SIZE, "[[deprecated]]");
        }

        output += line(INDENT_SIZE, "[[nodiscard]] %s %s(%s) {",
            replyType, method.name, decl);

        std::string call;
        if (method.ret && method.timeoutUsec) {
            call = "mClient.callSync<" + cppType(aIr, *method.ret) + ", " +
                std::to_string(*method.timeoutUsec) + ">(\"" + method.name + "\"";
        }
        else if (method.ret) {
            call = "mClient.callSync<" +
                cppType(aIr, *method.ret) + ">(\"" + method.name + "\"";
        }
        else if (method.timeoutUsec) {
            call = "mClient.callSync<void, " +
                std::to_string(*method.timeoutUsec) + ">(\"" + method.name + "\"";
        }
        else {
            call = "mClient.callSync(\"" + method.name + "\"";
        }

        if (!args.empty()) {
            call += ", " + args;
        }

        call += ")";

        output += line(INDENT_SIZE * 2, "return %s;", call);
        output += line(INDENT_SIZE, "}");
        output += '\n';
    }

    //! Definition of private variable
    output += line(0, "private:");
    output += line(INDENT_SIZE, "Dbusxx::Client mClient;");
    output += line(0, "};");
    output += '\n';

    output += line(0, "} // namespace %s", ns);
    output += line(0, "#endif");
    return output;
}

} // namespace Codegen
