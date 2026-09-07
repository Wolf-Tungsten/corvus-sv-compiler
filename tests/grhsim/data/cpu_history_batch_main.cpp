#include "grhsim_cpu_history_batch.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <random>
#include <stdexcept>

int main()
{
    GrhSIM_cpu_history_batch model;
    std::mt19937 random(20260907);
    unsigned samples = 0;
    for (unsigned reset = 0; reset < 4; ++reset) {
        model.init();
        std::array<std::uint8_t, 16> expected{};
        std::array<bool, 16> histories{};
        for (unsigned i = 0; i < histories.size(); ++i) histories[i] = i & 1;
        std::uint8_t sharedA = 0, sharedB = 0;
        bool sharedHistory = false;
        const auto step = [&](bool clock, bool clockB, std::uint8_t data) {
            for (unsigned i = 0; i < expected.size(); ++i)
                if (!histories[i == 10 ? 9 : i] && clock) expected[i] = data;
            histories.fill(clock);
            // Both guards read old H; A then B sample H, even when B has no edge.
            for (unsigned round = 0; ; ++round) {
                const auto nextA = !sharedHistory && clock ? data : sharedA;
                const auto nextB = !sharedHistory && clockB ? data : sharedB;
                if (nextA == sharedA && nextB == sharedB && sharedHistory == clockB) break;
                if (round == 8) throw std::runtime_error("shared history reference did not converge");
                sharedA = nextA; sharedB = nextB; sharedHistory = clockB;
            }
            model.clock = clock; model.clock_b = clockB; model.data = data; model.eval(); ++samples;
            const std::array<std::uint8_t, 16> actual{
                model.q0, model.q1, model.q2, model.q3, model.q4, model.q5, model.q6, model.q7,
                model.q8, model.q9, model.q10, model.q11, model.q12, model.q13, model.q14, model.q15};
            if (actual != expected || model.history_read != clock || model.shared_a != sharedA ||
                model.shared_b != sharedB || model.shared_history != sharedHistory) {
                std::cerr << "history batch mismatch at sample " << samples << '\n';
                throw std::runtime_error("private/shared history or initial edge mismatch");
            }
        };
        step(true, false, 42);
        if (model.q0 != 42 || model.q1 != 0 || model.q10 != 0)
            throw std::runtime_error("batch discarded distinct initial history values");
        step(true, false, 77); step(false, false, 19); step(true, false, 31);
        step(true, true, 53); step(false, true, 91); step(false, false, 12);
        bool clock = false, clockB = false;
        for (unsigned i = 0; i < 1024; ++i) {
            if (i % 3 == 0) clock = !clock;
            if (i % 5 == 0) clockB = !clockB;
            const auto data = std::uint8_t(random());
            step(clock, clockB, data);
            if (i % 8 == 0) step(clock, clockB, data);
        }
    }
    std::cout << "history batches PASS samples=" << samples << " resets=4\n";
}
