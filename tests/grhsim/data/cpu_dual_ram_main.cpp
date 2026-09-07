#include "grhsim_cpu_dual_ram.hpp"
#include "Vcpu_dual_ram.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <random>
#include <stdexcept>

int main()
{
    GrhSIM_cpu_dual_ram model; Vcpu_dual_ram reference; model.init();
    std::array<std::uint16_t, 8> memory{};
    std::uint16_t expectedA = 0, expectedB = 0;
    bool oldA = false, oldB = false, oldReset = false;
    unsigned samples = 0, simultaneous = 0, readOld = 0, asyncResets = 0;
    const auto step = [&] {
        const bool edgeA = !oldA && model.clock_a, edgeB = !oldB && model.clock_b;
        const bool edgeReset = !oldReset && model.reset;
        if (edgeA && edgeB && model.en_a && model.en_b && model.addr_a == model.addr_b)
            throw std::runtime_error("testbench introduced an undefined dual-write collision");
        simultaneous += edgeA && edgeB; asyncResets += edgeReset && !edgeA && !edgeB;
        if (edgeA || edgeReset) expectedA = model.reset ? 0 : memory[model.addr_a];
        if (edgeB || edgeReset) expectedB = model.reset ? 0 : memory[model.addr_b];
        const auto write = [&](bool edge, bool enabled, unsigned address, std::uint16_t data, std::uint16_t mask) {
            if (edge && enabled && !model.reset) {
                const auto next = std::uint16_t((memory[address] & ~mask) | (data & mask));
                readOld += next != memory[address]; memory[address] = next;
            }
        };
        write(edgeA, model.en_a, model.addr_a, model.data_a, model.mask_a);
        write(edgeB, model.en_b, model.addr_b, model.data_b, model.mask_b);
        reference.clock_a = model.clock_a; reference.clock_b = model.clock_b; reference.reset = model.reset;
        reference.en_a = model.en_a; reference.en_b = model.en_b;
        reference.addr_a = model.addr_a; reference.addr_b = model.addr_b; reference.probe = model.probe;
        reference.data_a = model.data_a; reference.data_b = model.data_b;
        reference.mask_a = model.mask_a; reference.mask_b = model.mask_b;
        model.eval(); reference.eval(); ++samples;
        if (model.q_a != expectedA || model.q_b != expectedB || model.observed != memory[model.probe] ||
            reference.q_a != expectedA || reference.q_b != expectedB || reference.observed != memory[model.probe]) {
            std::cerr << "dual RAM mismatch at sample " << samples << '\n';
            throw std::runtime_error("dual RAM CPU/Verilator/scoreboard mismatch");
        }
        oldA = model.clock_a; oldB = model.clock_b; oldReset = model.reset;
    };
    step(); model.reset = true; step(); model.reset = false; step();
    std::mt19937 random(20260908);
    for (unsigned i = 0; i < 8192; ++i) {
        if (i % 2 == 0) model.clock_a = !model.clock_a;
        if (i % 7 == 0) model.clock_b = !model.clock_b;
        if (i % 131 == 0) model.reset = !model.reset;
        model.en_a = random() & 1; model.en_b = random() & 1;
        model.addr_a = random() & 7; model.addr_b = random() & 7;
        if (!oldA && model.clock_a && !oldB && model.clock_b && model.addr_a == model.addr_b)
            model.addr_b = (model.addr_a + 1) & 7;
        model.data_a = random(); model.data_b = random();
        model.mask_a = i % 3 == 0 ? 0 : i % 3 == 1 ? 65535 : random();
        model.mask_b = i % 3 == 1 ? 0 : i % 3 == 2 ? 65535 : random();
        model.probe = random() & 7;
        step();
        if (i % 16 == 0) step();
        if (i % 64 == 0) for (unsigned address = 0; address < 8; ++address) { model.probe = address; step(); }
    }
    if (!simultaneous || readOld < 100 || !asyncResets) throw std::runtime_error("dual RAM edge coverage missing");
    reference.final();
    std::cout << "dual RAM CPU / Verilator / scoreboard PASS samples=" << samples
              << " simultaneous=" << simultaneous << " changed_writes=" << readOld << " async_resets=" << asyncResets << '\n';
}
