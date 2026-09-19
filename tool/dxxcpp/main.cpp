
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "Codegen.hpp"
#include "Parser.hpp"
#include "Sema.hpp"

namespace {
//! Process exit code
enum ExitCode : int {
    EXIT_OK = 0,        //! Success
    EXIT_ERROR = 1,     //! .dxx error (lexical/grammar/semantics)
    EXIT_USAGE = 2      //! Command usage error
};

struct Options {
    std::string input;
    std::string outputDir { "." };
    bool showHelp { false };
};

bool setOutputFolder(const std::string& aValue, Options* aOut) {
    if (aValue.empty()) {
        std::cerr << "error: output directory must not be empty\n";
        return false;
    }

    aOut->outputDir = aValue;
    return true;
}

void printUsage(std::ostream& aOut) {
    aOut << "dxxcpp - .dxx -> dbusxx C++ headers\n"
        << "\n"
        << "usage:\n"
        << "  dxxcpp [--dbus] <input.dxx> [-o <output-dir>]\n"
        << "\n"
        << "options:\n"
        << "  --dbus              backend: D-Bus (dbusxx); the only backend for now (default)\n"
        << "  -o, --output-dir    output directory for generated headers (default: current dir)\n"
        << "                      also accepts --output-dir=<dir> and -o<dir>\n"
        << "  -h, --help          show this help\n"
        << "\n"
        << "outputs (one per interface, the types header is shared per package):\n"
        << "  <Package>Types.hpp         e.g. ComExampleCalcTypes.hpp\n"
        << "  <Interface>Skeleton.hpp\n"
        << "  <Interface>Proxy.hpp\n"
        << "\n"
        << "Generated headers always include the library as <dbusxx/...> (installed layout);\n"
        << "the service well-known name is injected by the consumer via DBUSXX_SERVICE_NAME.\n";
}

bool parseArguments(int aArgc, char const* aArgv[], Options* aOut) {
    for (int i = 1; i < aArgc; ++i) {
        std::string arg = aArgv[i];
        std::string inlineValue;
        bool hasInlineValue = false;

        //! "--option=value" -> option, value
        const std::size_t equal = arg.find('=');
        if (arg.rfind("--", 0) == 0 && equal != std::string::npos) {
            inlineValue = arg.substr(equal + 1);
            arg = arg.substr(0, equal);
            hasInlineValue = true;
        }

        if (arg == "-h" || arg == "--help") {
            if (hasInlineValue) {
                std::cerr << "error: '" << arg << "' doesn't take a value\n";
                return false;
            }

            aOut->showHelp = true;
            continue;
        }

        if (arg == "--dbus") {
            if (hasInlineValue) {
                std::cerr << "error: '" << arg << "' doesn't take a value\n";
                return false;
            }

            //! The only backend currently available, kept for
            //! future additional backends
            continue;
        }

        //! -o <dir> / -o<dir> / --output-dir <dir> / --output-dir=<dir>
        if (arg == "-o" || arg == "--output-dir") {
            //! --output-dir=<dir>
            if (hasInlineValue) {
                if (!setOutputFolder(inlineValue, aOut)) {
                    return false;
                }

                continue;
            }

            if (i + 1 >= aArgc) {
                std::cerr << "error: '" << arg << "' needs a directory argument\n";
                return false;
            }

            //! -o <dir> / --output-dir <dir>
            if (!setOutputFolder(aArgv[++i], aOut)) {
                return false;
            }

            continue;
        }

        //! -o<dir>
        if (arg.rfind("-o", 0) == 0 && arg.size() > 2) {
            if (!setOutputFolder(arg.substr(2), aOut)) {
                return false;
            }

            continue;
        }

        if (!arg.empty() && arg[0] == '-') {
            std::cerr << "error: unknown option '" << arg << "'\n";
            return false;
        }

        if (!aOut->input.empty()) {
            std::cerr << "error: multiple input files ('" << aOut->input
                      << "' and '" << arg << "')\n";
            return false;
        }

        aOut->input = arg;
    }

    if (!aOut->showHelp && aOut->input.empty()) {
        std::cerr << "error: no input .dxx file\n";
        return false;
    }

    return true;
}

std::string errorPrefix(const std::string& aFile, std::size_t aLine, std::size_t aCol) {
    std::ostringstream oss;
    oss << aFile;
    if (aLine != 0) {
        oss << ":" << aLine << ":" << aCol;
    }

    return oss.str();
}

template<typename TError>
void printErrors(const std::string& aFile, const std::vector<TError>& aErrors) {
    for (const auto& e : aErrors) {
        std::cerr << errorPrefix(aFile, e.line, e.col) << ": error: " << e.msg << "\n";
    }
}

bool readSource(const std::string& aPath, std::string* aOut) {
    std::ifstream ifs(aPath);
    if (!ifs) {
        std::cerr << "error: cannot open '" << aPath << "'\n";
        return false;
    }

    std::ostringstream oss;
    oss << ifs.rdbuf();
    *aOut = oss.str();
    return true;
}

bool writeFile(const std::filesystem::path& aPath, const std::string& aText) {
    std::ofstream ofs(aPath, std::ios::binary);
    if (!ofs) {
        std::cerr << "error: cannot write '" << aPath.string() << "'\n";
        return false;
    }

    ofs << aText;
    if (!ofs) {
        std::cerr << "error: failed while writing '" << aPath.string() << "'\n";
        return false;
    }

    std::cout << "wrote " << aPath.string() << " (" << aText.size() << " bytes)\n";
    return true;
}

bool generate(const Ir::Root& aIr, const std::filesystem::path& aDir) {
    //! Generate <Package>Types.hpp
    if (!writeFile(aDir / Codegen::typesHeaderName(aIr),
            Codegen::genTypesHeader(aIr))) {
        return false;
    }

    for (const auto& ifce : aIr.interfaces) {
        //! Generate <Interface>Skeleton.hpp
        if (!writeFile(aDir / (ifce.name + "Skeleton.hpp"),
                Codegen::genSkeletonHeader(aIr, ifce))) {
            return false;
        }

        //! Generate <Interface>Proxy.hpp
        if (!writeFile(aDir / (ifce.name + "Proxy.hpp"),
                Codegen::genProxyHeader(aIr, ifce))) {
            return false;
        }
    }

    return true;
}
} // namespace

int main(int aArgc, char const* aArgv[]) {
    Options opt;
    //! Parse arguments
    if (!parseArguments(aArgc, aArgv, &opt)) {
        printUsage(std::cerr);
        return EXIT_USAGE;
    }

    //! Show usage help
    if (opt.showHelp) {
        printUsage(std::cout);
        return EXIT_OK;
    }

    std::string source;
    //! Read .dxx resource
    if (!readSource(opt.input, &source)) {
        return EXIT_ERROR;
    }

    //! Lexical analysis + Syntax analysis
    const Parser::Result parsed = Parser::parse(source);
    if (!parsed.errors.empty() || !parsed.root) {
        printErrors(opt.input, parsed.errors);
        std::cerr << "error: '" << opt.input << "' has syntax error(s), codegen aborted\n";
        return EXIT_ERROR;
    }

    //! Semantic analysis
    const Sema::Result sema = Sema::analyze(*parsed.root);
    if (!sema.errors.empty() || !sema.ir) {
        printErrors(opt.input, sema.errors);
        std::cerr << "error: '" << opt.input << "' has semantic error(s), codegen aborted\n";
        return EXIT_ERROR;
    }

    //! Create output folder
    std::error_code ec;
    const std::filesystem::path dir(opt.outputDir);
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        std::cerr << "error: cannot create directory '" << dir.string()
                  << "': " << ec.message() << "\n";
        return EXIT_ERROR;
    }

    //! Generate output files
    if (!generate(*sema.ir, dir)) {
        return EXIT_ERROR;
    }

    return EXIT_OK;
}

