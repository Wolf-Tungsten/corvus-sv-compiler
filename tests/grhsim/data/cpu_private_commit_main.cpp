#include "grhsim_cpu_private_commit.hpp"

#include <cstdint>
#include <iostream>
#include <random>
#include <stdexcept>

namespace {
    struct Snapshot {
        bool bit = false;
        std::int8_t narrow = 0;
        std::uint64_t word = 0;
        bool operator==(const Snapshot &) const = default;
    } observed;
    unsigned calls = 0;
}

extern "C" void cpu_test_private(bool bit, std::int8_t narrow, std::uint64_t word)
{
    observed = {bit, narrow, word};
    ++calls;
}

int main()
{
    GrhSIM_cpu_private_commit model;
    std::mt19937 random(20260907);
    unsigned samples = 0;
    for (unsigned reset = 0; reset < 4; ++reset) {
        model.init();
        Snapshot expected;
        const auto step = [&] {
            const auto before = calls;
            model.eval();
            ++samples;
            if (calls != before + 1 || !(observed == expected))
                throw std::runtime_error("nonprojected commit changed round count or compute old-state observation");
            if (model.enable) {
                expected.bit = model.bit_mask ? model.bit_data : expected.bit;
                const auto bits = ((std::uint8_t(expected.narrow) & ~std::uint8_t(model.narrow_mask)) |
                                   (std::uint8_t(model.narrow_data) & std::uint8_t(model.narrow_mask))) & 31;
                expected.narrow = static_cast<std::int8_t>(bits < 16 ? bits : bits - 32);
                expected.word = (expected.word & ~model.word_mask) | (model.word_data & model.word_mask);
            }
        };
        model.enable = false;
        step();
        for (unsigned i = 0; i < 1024; ++i) {
            model.enable = (random() & 3) != 0;
            model.bit_data = (random() & 1) != 0;
            model.bit_mask = (random() & 1) != 0;
            model.narrow_data = static_cast<std::int8_t>(random());
            model.narrow_mask = i % 3 == 0 ? 0 : i % 3 == 1 ? -1 : static_cast<std::int8_t>(random());
            model.word_data = (std::uint64_t(random()) << 32) | random();
            model.word_mask = i % 3 == 0 ? 0 : i % 3 == 1 ? UINT64_MAX : (std::uint64_t(random()) << 32) | random();
            step();
            if (i % 8 == 0) step();
        }
    }
    std::cout << "private nonprojected commits PASS samples=" << samples << " resets=4 calls=" << calls << '\n';
}
