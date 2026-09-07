#include "grhsim_cpu_emit_shape.hpp"

#include <array>
#include <iostream>
#include <random>
#include <stdexcept>

int main()
{
    GrhSIM_cpu_emit_shape model;
    std::mt19937 random(670069);
    unsigned samples = 0;
    for (unsigned reset = 0; reset < 4; ++reset) {
        model.init();
        std::array<std::uint8_t, 4> memory{};
        std::uint8_t q0 = 0, q1 = 0;
        bool previousClock = false;
        model.clock = false;
        for (unsigned i = 0; i < 4096; ++i) {
            model.clock = (random() & 1) != 0;
            model.enable = (random() & 3) != 0;
            model.fill = (random() & 15) == 0;
            model.address_a = random() & 7;
            model.address_b = i % 2 ? model.address_a : random() & 7;
            model.read_address = random() & 3;
            model.data = random();
            model.mask = i % 3 == 0 ? 0 : i % 3 == 1 ? 255 : random();
            if (model.clock && !previousClock) {
                q1 = q0;
                q0 = model.data;
                if (model.fill) memory.fill(model.data);
                if (model.enable && model.address_a < memory.size())
                    memory[model.address_a] = (memory[model.address_a] & ~model.mask) | (model.data & model.mask);
                if (model.enable && model.address_b < memory.size()) memory[model.address_b] = 0;
            }
            previousClock = model.clock;
            for (unsigned repeat = 0; repeat < 2; ++repeat) {
                model.eval();
                ++samples;
                if (model.q0 != q0 || model.q1 != q1 || model.memory_out != memory[model.read_address])
                    throw std::runtime_error("state snapshot or mixed memory write publication mismatch");
            }
        }
    }
    std::cout << "emit shape PASS samples=" << samples << " resets=4\n";
}
