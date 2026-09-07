#include "grhsim_cpu_init.hpp"

#include <iostream>
#include <stdexcept>

namespace
{
    std::uint64_t next(std::uint64_t &state)
    {
        std::uint64_t value = (state += UINT64_C(0x9e3779b97f4a7c15));
        value = (value ^ (value >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
        value = (value ^ (value >> 27)) * UINT64_C(0x94d049bb133111eb);
        return value ^ (value >> 31);
    }

    int signed5(std::uint64_t value) { return int(value & 15) - int(value & 16); }

    void require(bool condition, const char *message)
    { if (!condition) throw std::runtime_error(message); }
}

int main()
{
    try
    {
        const std::array<unsigned, 8> filled{0xa5, 0xa5, 0x3c, 0x3c, 0x3c, 0xa5, 0xee, 0xee};
        const std::array<unsigned, 8> hex{0x55, 0x55, 0, 0xad, 0x23, 0x55, 0x55, 0x55};
        const std::array<unsigned, 8> bin{0, 5, 2, 255, 4, 4, 6, 2};
        std::array<unsigned, 8> randomByte{};
        std::array<int, 8> randomSigned{};
        std::array<std::array<std::uint64_t, 3>, 8> randomWide{};
        std::uint64_t rng = UINT64_C(0x6a09e667f3bcc909);
        for (unsigned i = 2; i < 6; ++i) randomByte[i] = next(rng) & 255;
        for (auto &value : randomSigned) value = signed5(next(rng));
        for (auto &value : randomWide) { for (auto &word : value) word = next(rng); value[2] &= 1; }
        std::array<std::uint64_t, 3> randomScalar{}, seeded{};
        for (auto &word : randomScalar) word = next(rng);
        randomScalar[2] &= 1;
        rng = std::uint64_t(-7);
        for (auto &word : seeded) word = next(rng);
        seeded[2] &= 1;
        rng = 0;
        const auto seededSigned = signed5(next(rng));
        for (unsigned instance = 0; instance < 3; ++instance)
        {
            GrhSIM_cpu_init model;
            for (unsigned reset = 0; reset < 4; ++reset)
            {
                model.init();
                for (unsigned row = 0; row < 8; ++row)
                {
                    model.address = row; model.eval(); model.eval();
                    require(model.filled == filled[row], "fill range/order mismatch");
                    require(model.hex == hex[row], "hex readmem sparse/order/truncation mismatch");
                    require(model.bin == bin[row], "binary readmem/const sequence mismatch");
                    const std::array<std::uint64_t, 3> wide = row == 2 || row == 3 ?
                        std::array<std::uint64_t, 3>{UINT64_C(0x123456789abcdef0), 0, 1} :
                        std::array<std::uint64_t, 3>{UINT64_MAX, UINT64_MAX, 1};
                    require(model.wide_filled == wide, "wide fill mismatch");
                    require(model.wide_const == (row == 7 ? std::array<std::uint64_t, 3>{0, 0, 1} :
                        std::array<std::uint64_t, 3>{row, 0, 0}), "wide constant table mismatch");
                    std::array<std::uint64_t, 3> readmem{};
                    if (row == 1) readmem = {1, 0, 1};
                    if (row == 2) readmem = {UINT64_MAX, UINT64_MAX, 1};
                    if (row == 7) readmem = {UINT64_C(0x123456789abcdef0), UINT64_C(0x123456789abcdef0), 0};
                    require(model.wide_readmem == readmem, "wide readmem high word/padding mismatch");
                    require(model.fragmented == (row < 4 ? 4 : 8), "fragmented coverage mismatch");
                    require(model.signed_fill == -3 && model.unknown == 0, "signed/unknown fill mismatch");
                    require(model.random_byte == randomByte[row] && model.random_signed == randomSigned[row] &&
                        model.random_wide == randomWide[row], "random fill sampling/order/reset mismatch");
                    require(model.seeded == seeded && model.seeded_signed == seededSigned && model.random_scalar == randomScalar,
                        "scalar seed/shared RNG mismatch");
                }
            }
        }
        std::cout << "Array init passed: 96 samples, 12 resets, range/order/const/readmem/random/seed, 8 MiB stack\n";
    }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
