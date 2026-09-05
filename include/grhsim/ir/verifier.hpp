#ifndef WOLVRIX_GRHSIM_IR_VERIFIER_HPP
#define WOLVRIX_GRHSIM_IR_VERIFIER_HPP

#include "core/diagnostics.hpp"

namespace wolvrix::lib::grhsim
{
    class DialectRegistry;
    class GrhSimModel;

    bool verifyGrhSimModel(const GrhSimModel &model,
                           const DialectRegistry &registry,
                           wolvrix::lib::diag::Diagnostics &diagnostics);
}

#endif // WOLVRIX_GRHSIM_IR_VERIFIER_HPP
