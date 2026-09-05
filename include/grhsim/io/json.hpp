#ifndef WOLVRIX_GRHSIM_IO_JSON_HPP
#define WOLVRIX_GRHSIM_IO_JSON_HPP

#include "core/diagnostics.hpp"

#include <filesystem>
#include <iosfwd>
#include <memory>

namespace wolvrix::lib::grhsim
{
    class DialectRegistry;
    class GrhSimModel;

    inline constexpr const char *kGrhSimJsonFormat = "wolvrix.grhsim.v1";

    bool writeGrhSimJson(const GrhSimModel &model,
                         std::ostream &output,
                         const DialectRegistry &registry,
                         wolvrix::lib::diag::Diagnostics &diagnostics,
                         bool pretty = false);

    std::unique_ptr<GrhSimModel> readGrhSimJson(
        std::istream &input,
        const DialectRegistry &registry,
        wolvrix::lib::diag::Diagnostics &diagnostics);

    bool storeGrhSimModel(const GrhSimModel &model,
                          const std::filesystem::path &path,
                          const DialectRegistry &registry,
                          wolvrix::lib::diag::Diagnostics &diagnostics,
                          bool pretty = false);

    std::unique_ptr<GrhSimModel> loadGrhSimModel(
        const std::filesystem::path &path,
        const DialectRegistry &registry,
        wolvrix::lib::diag::Diagnostics &diagnostics);

} // namespace wolvrix::lib::grhsim

#endif // WOLVRIX_GRHSIM_IO_JSON_HPP
