#ifndef WOLVRIX_EMIT_GRHSIM_RUNTIME_HPP
#define WOLVRIX_EMIT_GRHSIM_RUNTIME_HPP

#include <iosfwd>

namespace wolvrix::lib::emit
{
    struct GrhSimRuntimeOptions
    {
        bool waveform = false;
        bool systemTasks = false;
        bool oneBitBitwiseBytes = false;
    };

    void writeGrhSimRuntime(std::ostream &output, const GrhSimRuntimeOptions &options = {});
}

#endif
