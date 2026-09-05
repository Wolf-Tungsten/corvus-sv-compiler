#include "grhsim/dialect/registry.hpp"

#include "grhsim/dialect/core.hpp"

#include <algorithm>
#include <utility>

namespace wolvrix::lib::grhsim
{

    bool DialectRegistry::registerDialect(DialectDefinition dialect, std::string &error)
    {
        if (dialect.name.empty() || dialect.version.empty())
        {
            error = "dialect name and version must be non-empty";
            return false;
        }
        auto [it, inserted] = dialects_.emplace(dialect.name, std::move(dialect));
        if (!inserted)
        {
            error = "dialect is already registered: " + it->first;
            return false;
        }
        return true;
    }

    bool DialectRegistry::registerReference(
        std::unordered_map<std::string, std::string> &table,
        std::string_view dialect, std::string_view qualifiedName,
        std::string_view kind, std::string &error)
    {
        if (dialects_.find(std::string(dialect)) == dialects_.end())
        {
            error = "cannot register " + std::string(kind) + " for unknown dialect: " +
                    std::string(dialect);
            return false;
        }
        const std::string prefix = std::string(dialect) + ".";
        if (qualifiedName.empty() || !qualifiedName.starts_with(prefix))
        {
            error = std::string(kind) + " must use the dialect-qualified prefix " + prefix;
            return false;
        }
        auto [it, inserted] = table.emplace(std::string(qualifiedName), std::string(dialect));
        if (!inserted)
        {
            error = std::string(kind) + " is already registered: " + it->first;
            return false;
        }
        return true;
    }

    bool DialectRegistry::registerType(std::string_view dialect,
                                       std::string_view qualifiedName,
                                       std::string &error)
    {
        return registerReference(types_, dialect, qualifiedName, "type", error);
    }

    bool DialectRegistry::registerOp(std::string_view dialect,
                                     std::string_view qualifiedName,
                                     std::string &error)
    {
        return registerReference(ops_, dialect, qualifiedName, "op", error);
    }

    bool DialectRegistry::registerFunctionDecl(std::string_view dialect,
                                               std::string_view qualifiedName,
                                               std::string &error)
    {
        return registerReference(functionDecls_, dialect, qualifiedName,
                                 "function declaration", error);
    }

    bool DialectRegistry::registerInitStep(std::string_view dialect,
                                           std::string_view qualifiedName,
                                           std::string &error)
    {
        return registerReference(initSteps_, dialect, qualifiedName, "init step", error);
    }

    const DialectDefinition *DialectRegistry::findDialect(std::string_view name) const noexcept
    {
        auto it = dialects_.find(std::string(name));
        return it == dialects_.end() ? nullptr : &it->second;
    }

    bool DialectRegistry::hasType(std::string_view qualifiedName) const noexcept
    {
        return types_.find(std::string(qualifiedName)) != types_.end();
    }

    bool DialectRegistry::hasOp(std::string_view qualifiedName) const noexcept
    {
        return ops_.find(std::string(qualifiedName)) != ops_.end();
    }

    bool DialectRegistry::hasFunctionDecl(std::string_view qualifiedName) const noexcept
    {
        return functionDecls_.find(std::string(qualifiedName)) != functionDecls_.end();
    }

    bool DialectRegistry::hasInitStep(std::string_view qualifiedName) const noexcept
    {
        return initSteps_.find(std::string(qualifiedName)) != initSteps_.end();
    }

    std::vector<std::string> DialectRegistry::dialectNames() const
    {
        std::vector<std::string> names;
        names.reserve(dialects_.size());
        for (const auto &[name, definition] : dialects_)
        {
            (void)definition;
            names.push_back(name);
        }
        std::sort(names.begin(), names.end());
        return names;
    }

    DialectRegistry makeDefaultDialectRegistry()
    {
        DialectRegistry registry;
        if (!registerCoreDialect(registry))
        {
            return {};
        }
        return registry;
    }

    const DialectRegistry &defaultDialectRegistry()
    {
        static const DialectRegistry registry = makeDefaultDialectRegistry();
        return registry;
    }

} // namespace wolvrix::lib::grhsim
