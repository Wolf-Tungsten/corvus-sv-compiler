#ifndef WOLVRIX_GRHSIM_DIALECT_REGISTRY_HPP
#define WOLVRIX_GRHSIM_DIALECT_REGISTRY_HPP

#include "grhsim/ir/model.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace wolvrix::lib::grhsim
{

    struct DialectDefinition
    {
        std::string name;
        std::string version;
        std::string schemaFingerprint;
    };

    class DialectRegistry
    {
    public:
        bool registerDialect(DialectDefinition dialect, std::string &error);
        bool registerType(std::string_view dialect, std::string_view qualifiedName,
                          std::string &error);
        bool registerOp(std::string_view dialect, std::string_view qualifiedName,
                        std::string &error);
        bool registerFunctionDecl(std::string_view dialect, std::string_view qualifiedName,
                                  std::string &error);
        bool registerInitStep(std::string_view dialect, std::string_view qualifiedName,
                              std::string &error);

        const DialectDefinition *findDialect(std::string_view name) const noexcept;
        bool hasType(std::string_view qualifiedName) const noexcept;
        bool hasOp(std::string_view qualifiedName) const noexcept;
        bool hasFunctionDecl(std::string_view qualifiedName) const noexcept;
        bool hasInitStep(std::string_view qualifiedName) const noexcept;
        std::vector<std::string> dialectNames() const;

    private:
        bool registerReference(std::unordered_map<std::string, std::string> &table,
                               std::string_view dialect, std::string_view qualifiedName,
                               std::string_view kind, std::string &error);

        std::unordered_map<std::string, DialectDefinition> dialects_;
        std::unordered_map<std::string, std::string> types_;
        std::unordered_map<std::string, std::string> ops_;
        std::unordered_map<std::string, std::string> functionDecls_;
        std::unordered_map<std::string, std::string> initSteps_;
    };

    DialectRegistry makeDefaultDialectRegistry();
    const DialectRegistry &defaultDialectRegistry();

} // namespace wolvrix::lib::grhsim

#endif // WOLVRIX_GRHSIM_DIALECT_REGISTRY_HPP
