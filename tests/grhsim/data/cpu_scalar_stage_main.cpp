#include "grhsim_cpu_scalar_stage.hpp"
#include "scalar_stage_slots.hpp"

#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>

void require(bool condition, const char *message)
{
    if (!condition) throw std::runtime_error(message);
}

template<class T>
void testLane(GrhSIM_cpu_scalar_stage &model, unsigned lane, unsigned width,
              T &data0, T &data1, T &mask0, T &mask1, const T &output, const bool &history)
{
    model.init();
    const auto [state, offset] = scalar_stage_slots[lane];
    const auto write = [&](T value) { model.cpu_write_scalar<T>(state, offset, 0, 0, true, value); };
    const auto visible = [&] { return cpu_at<T>(model.cpu_objects.get(), offset); };
    write(T{0});
    require(model.cpu_pending.empty() && !model.cpu_dirty[state], "clean no-op created a pending write");
    write(T{1}); write(T{1});
    require(model.cpu_pending.size() == 1 && model.cpu_dirty[state] && visible() == T{0},
            "dirty no-op duplicated a record or published early");
    write(T{0});
    require(model.cpu_pending.size() == 1 && !model.cpu_publish() && visible() == T{0},
            "write back to visible value lost cancellation semantics");
    require(model.cpu_pending.empty() && !model.cpu_dirty[state], "publication left dirty state");
    // Interoperate with a previously staged value, as required for shared shadow storage.
    model.cpu_stage<T>(state, offset, 0, 0, true) = T{1}; write(T{0});
    require(!model.cpu_publish() && visible() == T{0}, "full write ignored an existing shadow");
    write(T{1});
    require(model.cpu_publish() && visible() == T{1}, "changed scalar failed to publish projection");
    write(T{1});
    require(model.cpu_pending.empty(), "unchanged nonzero visible value created pending work");

    model.init(); model.clock = false; model.enable = true;
    model.eval();
    const std::uint64_t bits = width == 64 ? UINT64_MAX : (UINT64_C(1) << width) - 1;
    std::mt19937_64 random(90173 + lane);
    std::uint64_t expected = 0;
    for (unsigned sample = 0; sample < 1024; ++sample) {
        model.clock = false; model.eval();
        require(!history, "falling edge failed to sample observed history");
        data0 = static_cast<T>(random()); data1 = static_cast<T>(random());
        mask0 = static_cast<T>(random()); mask1 = static_cast<T>(random());
        switch (sample % 8) {
        case 0: mask0 = T{0}; mask1 = T{0}; break;
        case 1: data0 = static_cast<T>(expected); data1 = data0; break;
        case 2: // Full first write followed by restoration of the pre-edge value.
            mask0 = static_cast<T>(bits); mask1 = mask0; data1 = static_cast<T>(expected); break;
        case 3: // Disjoint masks must merge using the first writer's shadow.
            mask0 = static_cast<T>(bits & UINT64_C(0x5555555555555555));
            mask1 = static_cast<T>(bits & UINT64_C(0xaaaaaaaaaaaaaaaa)); break;
        default: break;
        }
        model.enable = sample % 11 != 0;
        if (model.enable) {
            for (auto pair : {std::pair{data0, mask0}, std::pair{data1, mask1}}) {
                const auto mask = static_cast<std::uint64_t>(pair.second) & bits;
                expected = ((expected & ~mask) | (static_cast<std::uint64_t>(pair.first) & mask)) & bits;
            }
        }
        model.clock = true; model.eval();
        require((static_cast<std::uint64_t>(output) & bits) == expected && history,
                "ordered scalar masked write or edge sampling mismatch");
        // Changing data at a stable high clock must not create a second edge.
        data0 = static_cast<T>(random()); data1 = static_cast<T>(random()); model.eval();
        require((static_cast<std::uint64_t>(output) & bits) == expected && history,
                "stable clock repeated a write");
    }
}

int main()
{
    GrhSIM_cpu_scalar_stage model;
#define CHECK_LANE(N, W) testLane(model, N, W, model.data##N##_0, model.data##N##_1, \
                                model.mask##N##_0, model.mask##N##_1, model.q##N, model.history##N)
    CHECK_LANE(0, 1); CHECK_LANE(1, 5); CHECK_LANE(2, 5); CHECK_LANE(3, 8);
    CHECK_LANE(4, 13); CHECK_LANE(5, 13); CHECK_LANE(6, 32); CHECK_LANE(7, 32);
    CHECK_LANE(8, 64); CHECK_LANE(9, 64);
#undef CHECK_LANE
    std::cout << "scalar staging PASS lanes=10 edges=10240\n";
}
