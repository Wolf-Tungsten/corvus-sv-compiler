#include "grhsim_cpu_cdc.hpp"
#include "Vcpu_cdc.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <random>
#include <stdexcept>

int main()
{
    GrhSIM_cpu_cdc model; Vcpu_cdc reference; model.init();
    std::array<std::uint8_t, 6> expected{};
    bool oldA = false, oldB = false, oldReset = false;
    unsigned samples = 0, simultaneous = 0, asyncResets = 0;
    const auto step = [&](bool a, bool b, bool reset, bool incA, bool incB) {
        const auto previous = expected;
        const bool edgeA = !oldA && a, edgeB = oldB && !b, edgeReset = !oldReset && reset;
        simultaneous += edgeA && edgeB; asyncResets += edgeReset && !edgeA && !edgeB;
        if (edgeA || edgeReset) {
            expected[0] = reset ? 0 : std::uint8_t(previous[0] + incA);
            expected[2] = reset ? 0 : previous[1]; expected[3] = reset ? 0 : previous[2];
        }
        if (edgeB || edgeReset) {
            expected[1] = reset ? 0 : std::uint8_t(previous[1] + incB);
            expected[4] = reset ? 0 : previous[0]; expected[5] = reset ? 0 : previous[4];
        }
        model.clock_a = reference.clock_a = a; model.clock_b = reference.clock_b = b;
        model.reset = reference.reset = reset;
        model.inc_a = reference.inc_a = incA; model.inc_b = reference.inc_b = incB;
        model.eval(); reference.eval(); ++samples;
        const std::array<std::uint8_t, 6> actual{model.count_a, model.count_b, model.b_sync1, model.b_sync2, model.a_sync1, model.a_sync2};
        const std::array<std::uint8_t, 6> rtl{reference.count_a, reference.count_b, reference.b_sync1, reference.b_sync2, reference.a_sync1, reference.a_sync2};
        if (actual != expected || rtl != expected) {
            std::cerr << "CDC mismatch at sample " << samples << '\n';
            throw std::runtime_error("CDC CPU/Verilator/scoreboard mismatch");
        }
        oldA = a; oldB = b; oldReset = reset;
    };
    step(0, 0, 0, 0, 0); step(0, 1, 0, 1, 1); step(0, 1, 1, 1, 1); step(0, 1, 0, 1, 1);
    for (unsigned i = 0; i < 1024; ++i) { step(1, 0, 0, 1, 1); step(0, 1, 0, 1, 1); }
    std::mt19937 random(20260907);
    bool a = false, b = true, reset = false;
    for (unsigned i = 0; i < 8192; ++i) {
        if (i % 2 == 0) a = !a;
        if (i % 13 == 0) b = !b;
        if (i % 97 == 0) reset = !reset;
        const bool incA = random() & 1, incB = random() & 1;
        step(a, b, reset, incA, incB);
        if (i % 16 == 0) step(a, b, reset, incA, incB);
    }
    if (simultaneous < 1024 || !asyncResets) throw std::runtime_error("CDC edge coverage missing");
    reference.final();
    std::cout << "CDC CPU / Verilator / scoreboard PASS samples=" << samples
              << " simultaneous=" << simultaneous << " async_resets=" << asyncResets << '\n';
}
