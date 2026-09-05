#ifndef WOLVRIX_GRHSIM_PASS_PASS_HPP
#define WOLVRIX_GRHSIM_PASS_PASS_HPP

#include "core/diagnostics.hpp"

#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace wolvrix::lib::grhsim
{
    class DialectRegistry;
    class GrhSimModel;

    enum class PassKind
    {
        Analysis,
        MetadataTransform,
        SemanticTransform,
        BackendMapping,
        Emit
    };

    struct PassResult
    {
        bool success = true;
        bool changed = false;
        std::vector<std::string> artifacts;
    };

    class Pass
    {
    public:
        Pass(std::string name, PassKind kind) : name_(std::move(name)), kind_(kind) {}
        virtual ~Pass() = default;

        const std::string &name() const noexcept { return name_; }
        PassKind kind() const noexcept { return kind_; }
        virtual PassResult run(GrhSimModel &model,
                               wolvrix::lib::diag::Diagnostics &diagnostics) = 0;

    private:
        std::string name_;
        PassKind kind_;
    };

    using PassFactory = std::function<std::unique_ptr<Pass>(
        std::span<const std::string_view> args, std::string &error)>;

    class PassRegistry
    {
    public:
        bool registerPass(std::string name, PassKind kind, PassFactory factory,
                          std::string &error);
        std::unique_ptr<Pass> create(std::string_view name,
                                     std::span<const std::string_view> args,
                                     std::string &error) const;
        std::vector<std::string> names() const;

    private:
        struct Entry
        {
            PassKind kind;
            PassFactory factory;
        };
        std::unordered_map<std::string, Entry> entries_;
    };

    struct PassManagerResult
    {
        bool success = true;
        bool changed = false;
        std::vector<std::string> artifacts;
    };

    class PassManager
    {
    public:
        explicit PassManager(const DialectRegistry &dialects) : dialects_(&dialects) {}
        void addPass(std::unique_ptr<Pass> pass);
        PassManagerResult run(GrhSimModel &model,
                              wolvrix::lib::diag::Diagnostics &diagnostics);

    private:
        const DialectRegistry *dialects_;
        std::vector<std::unique_ptr<Pass>> passes_;
    };

    PassRegistry makeDefaultPassRegistry();
    const PassRegistry &defaultPassRegistry();

} // namespace wolvrix::lib::grhsim

#endif // WOLVRIX_GRHSIM_PASS_PASS_HPP
