#include "grhsim_cpu_scalar.hpp"
#include "Vcpu_scalar.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <random>
#include <stdexcept>

int main()
{
    GrhSIM_cpu_scalar model; Vcpu_scalar reference; model.init();
    unsigned samples = 0;
    const auto step = [&](uint64_t a, uint64_t b, uint8_t small) {
        model.a = reference.a = a; model.b = reference.b = b;
        model.sa = reference.sa = a; model.sb = reference.sb = b;
        model.sm = reference.sm = small;
        model.eval(); reference.eval(); ++samples;
#define CHECK(name) if (uint64_t(model.out_##name) != uint64_t(reference.out_##name)) { \
    std::cerr << #name << " mismatch sample=" << samples << " a=" << a << " b=" << b \
              << " model=" << uint64_t(model.out_##name) << " ref=" << uint64_t(reference.out_##name) << '\n'; \
    throw std::runtime_error("scalar CPU/Verilator mismatch"); }
        CHECK(add) CHECK(sub) CHECK(mul) CHECK(div) CHECK(mod)
        CHECK(and) CHECK(or) CHECK(xor) CHECK(xnor) CHECK(shl) CHECK(lshr) CHECK(not) CHECK(assign) CHECK(mux)
        CHECK(eq) CHECK(ne) CHECK(caseEq) CHECK(caseNe)
        CHECK(lt) CHECK(le) CHECK(gt) CHECK(ge) CHECK(logicAnd) CHECK(logicOr)
        CHECK(sdiv) CHECK(smod) CHECK(sashr) CHECK(slt) CHECK(sle) CHECK(sgt) CHECK(sge)
        CHECK(reduceAnd) CHECK(reduceOr) CHECK(reduceXor) CHECK(reduceNand) CHECK(reduceNor) CHECK(reduceXnor) CHECK(logicNot)
        if ((model.out_static & 255u) != (reference.out_static & 255u) ||
            (model.out_dynamic & 255u) != (reference.out_dynamic & 255u) ||
            (model.out_array & 255u) != (reference.out_array & 255u))
            throw std::runtime_error("slice CPU/Verilator mismatch");
        CHECK(concat) CHECK(replicate)
#undef CHECK
    };
    const std::array<uint64_t, 10> corners{0, 1, 7, 31, 32, 63, 64, 65, UINT64_C(0x8000000000000000), UINT64_MAX};
    for (auto a : corners) for (auto b : corners) step(a, b, static_cast<uint8_t>(a));
    std::mt19937_64 generator(179);
    for (unsigned i = 0; i < 4096; ++i) step(generator(), i % 2 ? generator() : generator() % 128, generator());
    std::cout << "scalar emitted CPU / Verilator PASS samples=" << samples << '\n';
}
