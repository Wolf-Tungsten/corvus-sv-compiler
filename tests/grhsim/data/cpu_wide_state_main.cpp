#include "grhsim_cpu_wide_state.hpp"
#include "Vcpu_wide_state.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <random>
#include <stdexcept>

template<class Reference>
void set(Reference &target, const std::array<std::uint64_t, 3> &source)
{
    for (unsigned i = 0; i < 5; ++i) target[i] = source[i / 2] >> (32 * (i % 2));
}

template<class Reference>
void check(const char *name, const std::array<std::uint64_t, 3> &actual, const Reference &expected)
{
    for (unsigned i = 0; i < 5; ++i)
        if (static_cast<std::uint32_t>(actual[i / 2] >> (32 * (i % 2))) != expected[i])
            throw std::runtime_error(std::string(name) + " wide state mismatch");
    if (actual[2] > 1) throw std::runtime_error("nonzero state padding");
}

int main()
{
    GrhSIM_cpu_wide_state model; Vcpu_wide_state reference; model.init();
    std::mt19937_64 random(20260906);
    unsigned samples = 0;
    const auto step = [&] {
        model.eval(); reference.eval(); ++samples;
        check("q", model.q, reference.q); check("latched", model.latched, reference.latched);
        check("memory", model.memory_out, reference.memory_out);
        check("sequence", model.sequence_out, reference.sequence_out); check("fill", model.fill_out, reference.fill_out);
    };
    for (unsigned cycle = 0; cycle < 256; ++cycle)
    {
        model.clock = reference.clock = 0; step();
        model.en_a = reference.en_a = cycle & 1;
        model.en_b = reference.en_b = (cycle >> 1) & 1;
        model.fill = reference.fill = cycle % 7 == 0;
        model.address = reference.address = random() & 3;
        model.address_b = reference.address_b = cycle % 2 ? model.address : (random() & 3);
        model.data_a = {random(), random(), random() & 1};
        model.data_b = {random(), random(), random() & 1};
        model.mask = cycle % 3 == 0 ? std::array<std::uint64_t, 3>{0, 0, 0} :
            cycle % 3 == 1 ? std::array<std::uint64_t, 3>{UINT64_MAX, UINT64_MAX, 1} :
                            std::array<std::uint64_t, 3>{random(), random(), random() & 1};
        set(reference.data_a, model.data_a); set(reference.data_b, model.data_b); set(reference.mask, model.mask);
        step();
        model.clock = reference.clock = 1; step();
        model.data_a = {random(), random(), random() & 1};
        model.data_b = {random(), random(), random() & 1};
        set(reference.data_a, model.data_a); set(reference.data_b, model.data_b);
        step();
        for (unsigned address = 0; address < 4; ++address)
        {
            model.address = reference.address = address; step();
        }
    }
    std::cout << "wide state emitted CPU / Verilator PASS samples=" << samples << '\n';
}
