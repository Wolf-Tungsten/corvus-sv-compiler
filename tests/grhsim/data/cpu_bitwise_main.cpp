#include "grhsim_cpu_bitwise.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>

template<char Operation>
void legacy(const std::uint64_t *lhs, std::size_t lhsWords, const std::uint64_t *rhs,
            std::size_t rhsWords, std::size_t width, std::uint64_t *out, std::size_t outWords)
{
    if constexpr (Operation == '&') grhsim_and_words(lhs, lhsWords, rhs, rhsWords, width, out, outWords);
    if constexpr (Operation == '|') grhsim_or_words(lhs, lhsWords, rhs, rhsWords, width, out, outWords);
    if constexpr (Operation == '^') grhsim_xor_words(lhs, lhsWords, rhs, rhsWords, width, out, outWords);
    if constexpr (Operation == '~') grhsim_not_words(lhs, lhsWords, width, out, outWords);
}

template<char Operation>
void helperTests(std::mt19937_64 &random)
{
    for (std::size_t width : {65, 127, 128, 129, 192, 1024, 4097})
    {
        const auto words = (width + 63) / 64;
        for (unsigned sample = 0; sample < 64; ++sample)
        {
            std::vector<std::uint64_t> lhs(words), rhs(words), before(words), expected(words);
            for (std::size_t i = 0; i < words; ++i) { lhs[i] = random(); rhs[i] = random(); before[i] = random(); }
            const auto lhsWords = sample % 3 == 0 ? 1u : words;
            const auto rhsWords = sample % 3 == 1 ? 1u : words;
            legacy<Operation>(lhs.data(), lhsWords, rhs.data(), rhsWords, width, expected.data(), words);
            if (sample % 4 == 0) before = expected;
            if (sample % 4 == 1) { before = expected; before.front() ^= 1; }
            if (sample % 4 == 2) { before = expected; before.back() ^= UINT64_C(1) << 63; }
            auto actual = before;
            const bool changed = cpu_bitwise_words_changed<Operation>(lhs.data(), lhsWords, rhs.data(), rhsWords, width, actual.data(), words);
            if (actual != expected || changed != (before != expected)) throw std::runtime_error("bitwise changed/output mismatch");
            if (cpu_bitwise_words_changed<Operation>(lhs.data(), lhsWords, rhs.data(), rhsWords, width, actual.data(), words))
                throw std::runtime_error("unchanged bitwise output reported changed");
            auto aliasLhs = lhs;
            const bool lhsChanged = cpu_bitwise_words_changed<Operation>(aliasLhs.data(), lhsWords, rhs.data(), rhsWords, width, aliasLhs.data(), words);
            if (aliasLhs != expected || lhsChanged != (lhs != expected)) throw std::runtime_error("lhs alias mismatch");
            auto aliasRhs = rhs;
            const bool rhsChanged = cpu_bitwise_words_changed<Operation>(lhs.data(), lhsWords, aliasRhs.data(), rhsWords, width, aliasRhs.data(), words);
            if (aliasRhs != expected || rhsChanged != (rhs != expected)) throw std::runtime_error("rhs alias mismatch");
        }
    }
}

int main()
{
    std::mt19937_64 random(633);
    helperTests<'&'>(random); helperTests<'|'>(random); helperTests<'^'>(random); helperTests<'~'>(random);
    GrhSIM_cpu_bitwise model;
    for (unsigned sample = 0; sample < 4096; ++sample)
    {
        if (sample % 1024 == 0) model.init();
        model.lhs = {random(), random(), random() & 1};
        model.rhs = sample % 4 == 0 ? std::array<std::uint64_t, 3>{0, 0, 0} :
            std::array<std::uint64_t, 3>{random(), random(), random() & 1};
        model.narrow = random() & 0xfffffff;
        for (unsigned repeat = 0; repeat < 2; ++repeat)
        {
            model.eval();
            for (std::size_t word = 0; word < 3; ++word)
            {
                const auto mask = word == 2 ? UINT64_C(1) : UINT64_MAX;
                const auto a = model.lhs[word], b = model.rhs[word];
                if (model.out_and[word] != ((a & b) & mask) || model.out_xor[word] != ((a ^ b) & mask) ||
                    model.out_not[word] != ((~a) & mask) ||
                    model.out_or[word] != ((a | (word == 0 ? model.narrow : UINT64_C(0))) & mask))
                    throw std::runtime_error("generated bitwise model mismatch");
            }
        }
    }
    std::cout << "wide bitwise PASS helper_cases=1792 model_evals=8192 resets=4\n";
}
