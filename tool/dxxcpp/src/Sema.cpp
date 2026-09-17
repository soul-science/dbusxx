#include "Sema.hpp"

#include <cctype>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "Cpp.hpp"


namespace Sema {
namespace {
constexpr std::string_view TYEP_VECTOR_STR { "vector" };
constexpr std::string_view TYPE_ARRAY_STR { "array" };
constexpr std::string_view TYPE_MAP_STR { "map" };

constexpr std::string_view ANNS_TIMEOUT_STR { "timeout" };
constexpr std::string_view ANNS_READONLY_STR { "readonly" };
constexpr std::string_view ANNS_SYNC_STR { "sync" };
constexpr std::string_view ANNS_ASYNC_STR { "async" };
constexpr std::string_view ANNS_DEPRECATED_STR { "deprecated" };

enum class AnnTarget {
    METHOD,
    PROPERTY,
    SIGNAL
};

bool isBasicName(const std::string& aName) {
    static const std::unordered_set<std::string> basic = {
        "int8", "int16", "int32", "int64",
        "uint8", "uint16", "uint32", "uint64",
        "float", "double", "bool", "string", "bytes"};
    return basic.count(aName) > 0;
}

bool resolveKeyBasic(const Ast::Root& aAstRoot, const Ast::Type& aAstType,
                     std::unordered_set<std::string>& aSeen) {
    if (isBasicName(aAstType.name)) {
        return true;
    }

    if (!aAstType.templateType.empty()) {
        return false;
    }

    if (aAstRoot.findStruct(aAstType.name)) {
        return false;
    }

    const Ast::AliasType* al = aAstRoot.findAlias(aAstType.name);
    if (al) {
        if (!aSeen.insert(aAstType.name).second) {
            return false;
        }

        bool ok = resolveKeyBasic(aAstRoot, al->targetType, aSeen);
        aSeen.erase(aAstType.name);
        return ok;
    }

    return false;
}

bool isExistedName(const std::string& aName) {
    return isBasicName(aName)
        || aName == TYEP_VECTOR_STR
        || aName == TYPE_ARRAY_STR
        || aName == TYPE_MAP_STR;
}

bool isPositiveInt(const std::string& aStr) {
    if (aStr.empty()) {
        return false;
    }

    bool noneZero = false;
    for (char c : aStr) {
        if (c < '0' || c > '9') {
            return false;
        }

        if (c != '0') {
            noneZero = true;
        }
    }

    return noneZero;
}

bool isIdentifier(const std::string& aStr) {
    if (aStr.empty()) {
        return false;
    }

    if (!std::isalpha(aStr[0])) {
        return false;
    }

    for (char c : aStr) {
        if (!(std::isalnum(c) || c == '_')) {
            return false;
        }
    }

    return true;
}

void report(std::vector<Error>& errs,
  const std::string& aMsg, const Ast::Loc& aLoc) {
    errs.push_back(Error{ aMsg, aLoc.line, aLoc.col });
}

void appendErrors(std::vector<Error>& aTo, const std::vector<Error>& aFrom) {
    aTo.insert(aTo.end(), aFrom.begin(), aFrom.end());
}

std::vector<Error> checkType(const Ast::Root& aAstRoot, const Ast::Type& aAstType) {
    std::vector<Error> errs;

    if (isBasicName(aAstType.name)) {
        if (!aAstType.templateType.empty()) {
            report(
                errs,
                "The basic type '" + aAstType.name + "' cannot have template arg",
                aAstType.loc
            );
        }

        return errs;
    }

    if (aAstType.name == TYEP_VECTOR_STR) {
        if (aAstType.templateType.size() != 1) {
            report(
                errs,
                "vector<T> only need 1 template arg, but "
                    + std::to_string(aAstType.templateType.size()),
                aAstType.loc
            );

            for (const auto& t : aAstType.templateType) {
                appendErrors(errs, checkType(aAstRoot, t));
            }
        }

        return errs;
    }

    if (aAstType.name == TYPE_ARRAY_STR) {
        if (aAstType.templateType.size() != 2) {
            report(
                errs,
                "array<T, N>: only need 2 template args, but "
                    + std::to_string(aAstType.templateType.size()),
                aAstType.loc
            );
        } else {
            appendErrors(errs, checkType(aAstRoot, aAstType.templateType[0]));

            const Ast::Type& n = aAstType.templateType[1];
            if (!n.templateType.empty() || !isPositiveInt(n.name)) {
                report(
                    errs,
                    "array<T, N>: n must be a positive integer, but " + n.name,
                    n.loc
                );
            }
        }

        return errs;
    }

    if (aAstType.name == TYPE_MAP_STR) {
        if (aAstType.templateType.size() != 2) {
            report(
                errs,
                "map<K, V>: only need 2 template args, but "
                    + std::to_string(aAstType.templateType.size()),
                aAstType.loc
            );
        } else {
            const Ast::Type& key = aAstType.templateType[0];
            const Ast::Type& value = aAstType.templateType[1];
            appendErrors(errs, checkType(aAstRoot, key));
            appendErrors(errs, checkType(aAstRoot, value));

            std::unordered_set<std::string> seen;
            if (!resolveKeyBasic(aAstRoot, key, seen)) {
                report(errs,
                    "map key must resolve to a basic type: '" + key.name + "'",
                    key.loc
                );
            }
        }

        return errs;
    }

    const bool isStruct = aAstRoot.findStruct(aAstType.name) != nullptr;
    const bool isAlias  = aAstRoot.findAlias(aAstType.name) != nullptr;
    if (!isStruct && !isAlias) {
        report(
            errs,
            "unknown type: '" + aAstType.name + "'",
            aAstType.loc
        );
    } else if (!aAstType.templateType.empty()) {
        report(
            errs,
            "Type '" + aAstType.name + "' does not accept template parameters",
            aAstType.loc
        );
    }

    return errs;
}

std::vector<Error> checkAliasCycles(const Ast::Root& aAstRoot) {
    enum State {
        UNSEEN = 0,
        VISITING = 1,
        DONE = 2
    };

    std::vector<Error> errs;
    std::unordered_map<const Ast::AliasType*, int> state;
    for (const auto& a : aAstRoot.alias) {
        state[&a] = UNSEEN;
    }

    std::function<void(const Ast::AliasType*)> dfs =
        [&](const Ast::AliasType* a) -> void {
            int& st = state[a];
            if (st == DONE) {
                //! Done
                return;
            }

            if (st == VISITING) {
                report(errs, "alias cycle at '" + a->name + "'", a->loc);
                //! Has cycle
                return;
            }

            st = VISITING;
            const Ast::Type& tt = a->targetType;
            if (tt.templateType.empty()) {
                const Ast::AliasType* next = aAstRoot.findAlias(tt.name);
                if (next) {
                    dfs(next);
                }
            }

            st = DONE;
            //! Done
        };

    for (const auto& a : aAstRoot.alias) {
        dfs(&a);
    }

    return errs;
}

void directStructRefs(const Ast::Root& aAstRoot, const Ast::Type& aAstType,
  std::unordered_set<std::string>& aOut, std::unordered_set<std::string>& aSeen) {
    if (aAstType.name == TYPE_MAP_STR) {
        if (aAstType.templateType.size() >= 2) {
            directStructRefs(aAstRoot, aAstType.templateType[1], aOut, aSeen);
        }

        return;
    }

    if (aAstType.name == TYPE_ARRAY_STR || aAstType.name == TYEP_VECTOR_STR) {
        if (!aAstType.templateType.empty()) {
            directStructRefs(aAstRoot, aAstType.templateType[0], aOut, aSeen);
        }

        return;
    }

    if (isBasicName(aAstType.name)) {
        return;
    }

    if (!aAstType.templateType.empty()) {
        return;
    }

    if (aAstRoot.findStruct(aAstType.name)) {
        aOut.insert(aAstType.name);
        return;
    }

    const Ast::AliasType* al = aAstRoot.findAlias(aAstType.name);
    if (al) {
        if (aSeen.insert(aAstType.name).second) {
            directStructRefs(aAstRoot, al->targetType, aOut, aSeen);
            aSeen.erase(aAstType.name);
        }
    }
}

std::vector<Error> checkStructSelfRef(const Ast::Root& aAstRoot) {
    //! Adjacency List: Recursive inclusion of struct edges
    std::unordered_map<std::string, std::unordered_set<std::string>> adj;
    for (const auto& s : aAstRoot.structs) {
        std::unordered_set<std::string> deps;
        for (const auto& f : s.fields) {
            std::unordered_set<std::string> seen;
            directStructRefs(aAstRoot, f.type, deps, seen);
        }

        adj[s.name] = std::move(deps);
    }

    enum State {
        UNSEEN = 0,
        VISITING = 1,
        DONE = 2
    };

    std::unordered_map<std::string, int> st;
    for (const auto& [name, _] : adj) {
        st[name] = UNSEEN;
    }

    std::vector<Error> errs;
    std::function<bool(const std::string&)> dfs = [&](const std::string& n) -> bool {
        int& s = st[n];
        if (s == DONE) {
            return false;
        }

        if (s == VISITING) {
            return true;
        }

        s = VISITING;
        for (const auto& next : adj[n]) {
            if (dfs(next)) {
                return true;
            }
        }

        s = DONE;
        return false;
    };

    for (const auto& [name, _] : adj) {
        if (!dfs(name)) {
            continue;
        }

        for (const auto& s : aAstRoot.structs) {
            if (s.name == name) {
                report(
                    errs,
                    "struct '" + name + "' is recursive and cannot be represented ",
                    s.loc
                );
            }
        }

        break;
    }

    return errs;
}

//! A field only refer to a struct that is declared earlier
//! (mutual inclusion was already rejected by checkStructSelfRef)
std::vector<Error> checkStructOrder(const Ast::Root& aAstRoot) {
    std::unordered_map<std::string, std::size_t> order;
    for (std::size_t i = 0; i < aAstRoot.structs.size(); ++i) {
        order[aAstRoot.structs[i].name] = i;
    }

    std::vector<Error> errs;
    for (std::size_t i = 0; i < aAstRoot.structs.size(); ++i) {
        const Ast::StructType& st = aAstRoot.structs[i];
        std::unordered_set<std::string> reported;
        for (const auto& field : st.fields) {
            std::unordered_set<std::string> deps;
            std::unordered_set<std::string> seen;
            directStructRefs(aAstRoot, field.type, deps, seen);
            for (const auto& dep : deps) {
                if (order[dep] <= i || !reported.insert(dep).second) {
                    continue;
                }

                report(
                    errs,
                    "struct '" + dep + "' must be declared before struct '" +
                        st.name + "'",
                    field.loc
                );
            }
        }
    }

    return errs;
}

std::vector<Error> checkAnnotations(AnnTarget aT, const std::vector<Ast::Annotation>& aAnns) {
    std::vector<Error> errs;
    std::unordered_set<std::string> seen;
    for (const auto& a : aAnns) {
        if (!seen.emplace(a.name).second) {
            report(errs, "duplicate annotation @" + a.name, a.loc);
        }

        if (a.name == ANNS_TIMEOUT_STR) {
            if (aT != AnnTarget::METHOD) {
                report(errs, "@timeout is only for method", a.loc);
            }
            else if (!a.value || !isPositiveInt(*a.value)) {
                report(errs, "@timeout needs a positive integer (ms)", a.loc);
            }
        }
        else if (a.name == ANNS_READONLY_STR) {
            if (aT != AnnTarget::PROPERTY) {
                report(errs, "@readonly is only for property", a.loc);
            }
            else if (a.value) {
                report(errs, "@readonly doesn't require a value", a.loc);
            }
        }
        else if (a.name == ANNS_SYNC_STR || a.name == ANNS_ASYNC_STR) {
            if (aT != AnnTarget::METHOD) {
                report(errs, "@" + a.name + " is only for method", a.loc);
            }

            if ((a.name == ANNS_SYNC_STR && seen.count(ANNS_ASYNC_STR.data()) > 0)
                || (a.name == ANNS_ASYNC_STR && seen.count(ANNS_SYNC_STR.data()) > 0)) {
                report(errs, "@sync and @async are mutually exclusive", a.loc);
            }

            if (a.value) {
                report(errs, "@" + a.name + " doesn't require a value", a.loc);
            }
        }
        else if (a.name == ANNS_DEPRECATED_STR) {
            if (a.value) {
                report(errs, "@deprecated doesn't require a value", a.loc);
            }
        }
        else {
            report(errs, "unknown annotation @" + a.name, a.loc);
        }
    }

    return errs;
}

bool hasAnnotation(const std::vector<Ast::Annotation>& aAnnos, const std::string& aName) {
    return std::any_of(aAnnos.begin(), aAnnos.end(),
        [&](const auto& it) {
            return it.name == aName;
        }
    );
}

std::vector<Ast::Annotation>::const_iterator findAnnotation(
  const std::vector<Ast::Annotation>& aAnns, const std::string& aName) {
    for (auto iter = aAnns.begin(); iter != aAnns.end(); ++iter) {
        if (iter->name == aName) {
            return iter;
        }
    }

    return aAnns.end();
}
}

std::vector<Error> validateAst(const Ast::Root& aAstRoot) {
    std::vector<Error> errs;

    //! Check package
    {
        std::size_t segment = 0;
        bool bad = false;
        std::size_t pos = 0;
        const Ast::Package& package = aAstRoot.package;
        while (pos <= package.name.size()) {
            std::size_t end = package.name.find('.', pos);
            std::string part = package.name.substr(
                pos, end == std::string::npos ? std::string::npos : end - pos);
            ++segment;
            if (!isIdentifier(part)) {
                report(
                    errs,
                    "package segment '" + part + "' is not a valid identifier",
                    package.loc
                );
                bad = true;
                break;
            }

            if (Cpp::isKeyword(part)) {
                report(
                    errs,
                    "package segment '" + part + "' is a C++ keyword",
                    package.loc
                );
                bad = true;
                break;
            }

            if (end == std::string::npos) {
                break;
            }

            pos = end + 1;
        }

        if (!bad && segment < 2) {
            report(
                errs,
                "package should look like 'a.b.c'",
                package.loc
            );
        }
    }

    std::unordered_set<std::string> space;
    {
        auto checkDuplication = [&](const std::string& aName, const Ast::Loc& aLoc) {
            if (isExistedName(aName)) {
                report(
                    errs,
                    "type name '" + aName + "' is reserved",
                    aLoc
                );
                return;
            }

            if (!isIdentifier(aName)) {
                report(
                    errs,
                    "type name '" + aName + "' is not a valid identifier",
                    aLoc
                );
                return;
            }

            if (Cpp::isKeyword(aName)) {
                report(
                    errs,
                    "type name '" + aName + "' is a C++ keyword",
                    aLoc
                );
                return;
            }

            if (!space.emplace(aName).second) {
                report(
                    errs,
                    "duplicate type name '" + aName + "'",
                    aLoc
                );
            }
        };

        for (const auto& s : aAstRoot.structs) {
            checkDuplication(s.name, s.loc);
        }

        for (const auto& al : aAstRoot.alias) {
            checkDuplication(al.name, al.loc);
        }
    }

    //! Check cycle
    appendErrors(errs, checkAliasCycles(aAstRoot));
    appendErrors(errs, checkStructSelfRef(aAstRoot));

    //! Check struct declaration order (fields are contained by value)
    appendErrors(errs, checkStructOrder(aAstRoot));

    //! Check struct
    for (const auto& s : aAstRoot.structs) {
        //! The empty struct is invalid
        if (s.fields.empty()) {
            report(
                errs,
                "struct '" + s.name + "' must have at least one field",
                s.loc
            );
        }

        std::unordered_set<std::string> fnames;
        for (const auto& f : s.fields) {
            if (!isIdentifier(f.name)) {
                report(
                    errs,
                    "field '" + f.name +
                        "' is not a valid identifier in struct '" + s.name + "'",
                    f.loc
                );
            }

            if (!fnames.insert(f.name).second) {
                report(
                    errs,
                    "duplicate field '" + f.name + "' in struct " + s.name,
                    f.loc
                );
            }

            appendErrors(errs, checkType(aAstRoot, f.type));
        }
    }

    //! Check alias
    for (const auto& al : aAstRoot.alias) {
        appendErrors(errs, checkType(aAstRoot, al.targetType));
    }

    //! Check interface
    std::unordered_set<std::string> ifceNames;
    for (const auto& ifce : aAstRoot.interfaces) {
        if (!isIdentifier(ifce.name)) {
            report(
                errs,
                "interface '" + ifce.name + "' is not a valid identifier",
                ifce.loc
            );
        } else if (Cpp::isKeyword(ifce.name)) {
            report(
                errs,
                "interface '" + ifce.name + "' is a C++ keyword",
                ifce.loc
            );
        }

        //! Check if interface is duplicated
        if (!ifceNames.emplace(ifce.name).second) {
            report(
                errs,
                "duplicate interface '" + ifce.name + "'",
                ifce.loc
            );
        }

        std::unordered_set<std::string> memberNames;
        auto addMember = [&](const std::string& aName, const Ast::Loc& aLoc) {
            if (!isIdentifier(aName)) {
                report(
                    errs,
                    "member '" + aName +
                        "' is not a valid identifier in " + ifce.name,
                    aLoc
                );
            } else if (Cpp::isKeyword(aName)) {
                report(
                    errs,
                    "member '" + aName +
                        "' is a C++ keyword in " + ifce.name,
                    aLoc
                );
            }

            if (!memberNames.emplace(aName).second) {
                report(
                    errs,
                    "duplicate member '" + aName + "' in " + ifce.name,
                    aLoc
                );
            }
        };

        //! Check method
        for (const auto& m : ifce.methods) {
            //! Check if method is duplicated
            addMember(m.name, m.loc);

            //! Check annotation
            appendErrors(errs, checkAnnotations(AnnTarget::METHOD, m.annotations));

            //! Check param
            for (const auto& p : m.params) {
                //! The generated async shape declares its own completion callback,
                //! so a parameter with that name would be declared twice
                if (!hasAnnotation(m.annotations, ANNS_SYNC_STR.data())
                  && p.name == Ir::CALLBACK_PARAM) {
                    report(
                        errs,
                        "parameter '" + p.name + "' of method '" + m.name +
                            "' collides with the generated async callback",
                        p.loc
                    );
                }

                appendErrors(errs, checkType(aAstRoot, p.type));
            }

            //! Check return type
            if (m.retType) {
                appendErrors(errs, checkType(aAstRoot, *m.retType));
            }
        }

        //! Check signal
        for (const auto& s : ifce.signals) {
            //! Check if signal is duplicated
            addMember(s.name, s.loc);

            //! Check annotation
            appendErrors(errs, checkAnnotations(AnnTarget::SIGNAL, s.annotations));

            for (const auto& p : s.params) {
                appendErrors(errs, checkType(aAstRoot, p.type));
            }
        }

        //! Check property
        for (const auto& pr : ifce.properties) {
            //! Check if signal is duplicated
            addMember(pr.name, pr.loc);

            //! Check annotation
            appendErrors(errs, checkAnnotations(AnnTarget::PROPERTY, pr.annotations));

            appendErrors(errs, checkType(aAstRoot, pr.type));
        }

        //! Every name the Proxy declares must be unique: a method becomes
        //! <name> (unless @async) plus <name>Async (unless @sync), a signal
        //! becomes its listener name (properties are not part of the Proxy)
        std::unordered_map<std::string, std::string> generated;
        auto addGenerated = [&](const std::string& aName, const std::string& aOwner,
          const Ast::Loc& aLoc) {
            const auto [aIt, aInserted] = generated.emplace(aName, aOwner);
            if (!aInserted) {
                report(
                    errs,
                    "'" + aName + "' is generated twice in " + ifce.name +
                        ": by " + aIt->second + " and " + aOwner,
                    aLoc
                );
            }
        };

        for (const auto& m : ifce.methods) {
            const std::string owner = "method '" + m.name + "'";
            if (!hasAnnotation(m.annotations, ANNS_ASYNC_STR.data())) {
                addGenerated(m.name, owner, m.loc);
            }

            if (!hasAnnotation(m.annotations, ANNS_SYNC_STR.data())) {
                addGenerated(m.name + std::string(Ir::ASYNC_SUFFIX), owner, m.loc);
            }
        }

        for (const auto& s : ifce.signals) {
            addGenerated(Ir::signalListenerName(s.name), "signal '" + s.name + "'", s.loc);
        }
    }

    return errs;
}

Result generateIr(const Ast::Root& aAstRoot) {
    Ir::Root ir;
    std::vector<Error> errs;

    //! package
    ir.package = aAstRoot.package.name;

    ir.structs.resize(aAstRoot.structs.size());
    for (std::size_t i = 0; i< aAstRoot.structs.size(); ++i) {
        ir.structs[i].name = aAstRoot.structs[i].name;
    }

    ir.aliases.resize(aAstRoot.alias.size());
    for (std::size_t i = 0; i< aAstRoot.alias.size(); ++i) {
        ir.aliases[i].name = aAstRoot.alias[i].name;
    }

    std::function<Ir::TypeId(const Ast::Type&)> lowerType =
        [&](const Ast::Type& aAstType) -> Ir::TypeId {
            const std::string& name = aAstType.name;
            if (isBasicName(name)) {
                return ir.addTypeNode(
                    Ir::TypeNode {
                        Ir::TypeBase{
                            ir.addBaseTypeName(name)
                        }
                    }
                );
            }

            if (name == TYEP_VECTOR_STR) {
                if (aAstType.templateType.size() != 1) {
                    report(errs, "vector<T> need 1 arg", aAstType.loc);
                    return 0;
                }

                Ir::TypeId element = lowerType(aAstType.templateType[0]);
                return ir.addTypeNode(
                    Ir::TypeNode {
                        Ir::TypeVector { element }
                    }
                );
            }

            if (name == TYPE_ARRAY_STR) {
                if (aAstType.templateType.size() != 2) {
                    report(errs, "array<T,N> need 2 args", aAstType.loc);
                    return 0;
                }

                Ir::TypeId element = lowerType(aAstType.templateType[0]);
                std::uint32_t n = std::stoul(aAstType.templateType[1].name);
                return ir.addTypeNode(
                    Ir::TypeNode {
                        Ir::TypeArray { element, n }
                    }
                );
            }

            if (name == TYPE_MAP_STR) {
                if (aAstType.templateType.size() != 2) {
                    report(errs, "map<K,V> need 2 args", aAstType.loc);
                    return 0;
                }

                Ir::TypeId key = lowerType(aAstType.templateType[0]);
                Ir::TypeId val = lowerType(aAstType.templateType[1]);
                return ir.addTypeNode(
                    Ir::TypeNode {
                        Ir::TypeMap { key, val }
                    }
                );
            }

            for (std::size_t i = 0; i < aAstRoot.structs.size(); ++i) {
                if (aAstRoot.structs[i].name == name) {
                    return ir.addTypeNode(
                        Ir::TypeNode {
                            Ir::TypeStruct { static_cast<std::uint32_t>(i) }
                        }
                    );
                }
            }

            for (std::size_t i = 0; i < aAstRoot.alias.size(); ++i) {
                if (aAstRoot.alias[i].name == name) {
                    return lowerType(aAstRoot.alias[i].targetType);
                }
            }

            report(errs, "unknown type: '" + name + "'", aAstType.loc);
            return 0;
        };

    //! Generate struct
    for (std::size_t i = 0; i < aAstRoot.structs.size(); ++i) {
        for (const auto& f : aAstRoot.structs[i].fields) {
            ir.structs[i].fields.push_back(
                Ir::Field{ f.name, lowerType(f.type) }
            );
        }
    }

    //! Generate alias
    for (std::size_t i = 0; i < aAstRoot.alias.size(); ++i) {
        ir.aliases[i].target = lowerType(aAstRoot.alias[i].targetType);
    }

    //! Generate interface
    for (const auto& ai : aAstRoot.interfaces) {
        Ir::Interface ii;
        ii.name = ai.name;

        //! Generate method
        for (const auto& am : ai.methods) {
            Ir::Method im;
            im.name = am.name;
            im.deprecated = hasAnnotation(am.annotations, ANNS_DEPRECATED_STR.data());
            auto timeoutIt = findAnnotation(am.annotations, ANNS_TIMEOUT_STR.data());
            if (timeoutIt != am.annotations.end() && timeoutIt->value) {
                im.timeoutUsec = std::stoull(*timeoutIt->value) * 1000;
            }

            auto isSync = findAnnotation(am.annotations, ANNS_SYNC_STR.data());
            auto isAsync = findAnnotation(am.annotations, ANNS_ASYNC_STR.data());
            if (isSync != am.annotations.end()) {
                im.callMode = Ir::Method::CallMode::Sync;
            }
            else if (isAsync != am.annotations.end()) {
                im.callMode = Ir::Method::CallMode::Async;
            }

            for (const auto& ap : am.params) {
                im.params.push_back(
                    Ir::Parameter { ap.name, lowerType(ap.type) }
                );
            }

            if (am.retType) {
                im.ret = lowerType(*am.retType);
            }

            ii.methods.push_back(std::move(im));
        }

        //! Generate signal
        for (const auto& as : ai.signals) {
            Ir::Signal is;
            is.name = as.name;
            is.deprecated = hasAnnotation(as.annotations, ANNS_DEPRECATED_STR.data());
            for (const auto& ap : as.params) {
                is.params.push_back(
                    Ir::Parameter{ ap.name, lowerType(ap.type) }
                );
            }

            ii.signals.push_back(std::move(is));
        }

        //! Generate property
        for (const auto& ap : ai.properties) {
            Ir::Property ip;
            ip.name = ap.name;
            ip.readonly = hasAnnotation(ap.annotations, "readonly");
            ip.type = lowerType(ap.type);
            ip.defaultValue = ap.defaultValue;
            ii.properties.push_back(std::move(ip));
        }

        ir.interfaces.push_back(std::move(ii));
    }

    if (!errs.empty()) {
        return Result {
            std::nullopt, std::move(errs)
        };
    }

    return Result { std::move(ir), {} };
}

Result analyze(const Ast::Root& aAstRoot) {
    std::vector<Error> errors = validateAst(aAstRoot);
    if (!errors.empty()) {
        return Result {
            .ir = std::nullopt,
            .errors = std::move(errors)
        };
    }

    return generateIr(aAstRoot);
}

}