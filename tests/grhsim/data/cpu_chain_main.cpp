#include "grhsim_cpu_chain.hpp"
#include "Vcpu_chain.h"

#include <cstdint>
#include <iostream>
#include <random>
#include <stdexcept>

int main()
{
    GrhSIM_cpu_chain model;
    Vcpu_chain reference;
    model.init();
    unsigned samples = 0;
    const auto step = [&](unsigned clock, unsigned clockB, unsigned reset, unsigned enable, unsigned data) {
        reference.clock = model.clock = clock;
        reference.clock_b = model.clock_b = clockB;
        reference.reset = model.reset = reset;
        reference.enable = model.enable = enable;
        reference.data = model.data = data;
        model.eval(); reference.eval(); ++samples;
        if (model.q1 != reference.q1 || model.q2 != reference.q2 || model.qg != reference.qg ||
            model.qn != reference.qn || model.latched != reference.latched || model.merged != reference.merged ||
            model.cancelled != reference.cancelled)
        {
            std::cerr << "mismatch at sample " << samples << " inputs=" << clock << ',' << clockB << ','
                      << reset << ',' << enable << ',' << data << " q=" << unsigned(model.q1) << ',' << unsigned(model.q2)
                      << ',' << unsigned(model.qg) << ',' << unsigned(model.qn) << ',' << unsigned(model.latched)
                      << ',' << unsigned(model.merged) << " ref=" << unsigned(reference.q1) << ',' << unsigned(reference.q2)
                      << ',' << unsigned(reference.qg) << ',' << unsigned(reference.qn) << ',' << unsigned(reference.latched)
                      << ',' << unsigned(reference.merged) << '\n';
            throw std::runtime_error("emitted CPU model differs from Verilator");
        }
    };
    step(0, 0, 0, 0, 0); step(0, 1, 1, 0, 0); step(0, 1, 0, 1, 17);
    step(1, 1, 0, 1, 17);
    if (model.q1 != 17 || model.q2 != 0) throw std::runtime_error("one edge advanced chain twice");
    if (model.cancelled != 0) throw std::runtime_error("later write failed to restore the visible value");
    for (unsigned i = 0; i < 32; ++i) step(1, 1, 0, 1, 17);
    step(0, 1, 0, 1, 34); step(1, 1, 0, 1, 34);
    step(1, 1, 0, 0, 34); step(1, 1, 0, 1, 34);
    if (model.qg != 17) throw std::runtime_error("derived clock edge was lost");
    std::mt19937 generator(1701);
    unsigned clock = 1, clockB = 1, reset = 0, enable = 1, data = 34;
    for (unsigned i = 0; i < 4096; ++i)
    {
        const auto value = generator();
        switch (i % 5)
        {
        case 0: clock ^= 1; break;
        case 1: clockB ^= 1; break;
        case 2: enable = value & 1; break;
        case 3: data = value & 255; break;
        default: reset = (value & 15) == 0; break;
        }
        step(clock, clockB, reset, enable, data);
    }
    reference.final();
    std::cout << "emitted CPU / Verilator PASS samples=" << samples << '\n';
}
