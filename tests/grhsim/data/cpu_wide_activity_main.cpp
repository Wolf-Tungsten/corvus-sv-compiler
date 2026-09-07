#include "grhsim_cpu_wide_activity.hpp"

#include <iostream>
#include <random>
#include <stdexcept>

template<char Operation>
void legacy(const std::uint64_t *lhs, std::size_t lhsWords, const std::uint64_t *rhs,
            std::size_t rhsWords, std::size_t amount, std::size_t width,
            std::uint64_t *out, std::size_t outWords)
{
    if constexpr (Operation == '+') grhsim_add_words(lhs, lhsWords, rhs, rhsWords, width, out, outWords);
    if constexpr (Operation == '-') grhsim_sub_words(lhs, lhsWords, rhs, rhsWords, width, out, outWords);
    if constexpr (Operation == 'L') grhsim_shl_words(lhs, lhsWords, amount, width, out, outWords);
    if constexpr (Operation == 'R') grhsim_lshr_words(lhs, lhsWords, amount, width, out, outWords);
    if constexpr (Operation == 'A') grhsim_ashr_words(lhs, lhsWords, amount, width, out, outWords);
}

template<char Operation>
bool changed(const std::uint64_t *lhs, std::size_t lhsWords, const std::uint64_t *rhs,
             std::size_t rhsWords, std::size_t amount, std::size_t width,
             std::uint64_t *out, std::size_t outWords)
{
    if constexpr (Operation == '+' || Operation == '-')
        return cpu_arithmetic_words_changed<Operation>(lhs, lhsWords, rhs, rhsWords, width, out, outWords);
    else return cpu_shift_words_changed<Operation>(lhs, lhsWords, amount, width, out, outWords);
}

template<char Operation>
void helperTests(std::mt19937_64 &random)
{
    for (std::size_t width : {65, 127, 128, 129, 192, 448, 4097})
    {
        const auto words = (width + 63) / 64;
        const std::array<std::size_t, 12> amounts{0, 1, 63, 64, 65, 127, 128, width - 1, width, width + 1, 65536, SIZE_MAX};
        for (unsigned sample = 0; sample < 128; ++sample)
        {
            std::vector<std::uint64_t> lhs(words), rhs(words), before(words), expected(words);
            for (std::size_t i = 0; i < words; ++i) { lhs[i] = random(); rhs[i] = random(); before[i] = random(); }
            if (sample % 8 == 0) { std::fill(lhs.begin(), lhs.end(), UINT64_MAX); std::fill(rhs.begin(), rhs.end(), 0); rhs[0] = 1; }
            if (sample % 8 == 1) { std::fill(lhs.begin(), lhs.end(), 0); std::fill(rhs.begin(), rhs.end(), UINT64_MAX); }
            const auto lhsWords = sample % 3 == 0 ? 1u : words;
            const auto rhsWords = sample % 3 == 1 ? 1u : words;
            const auto amount = amounts[sample % amounts.size()];
            legacy<Operation>(lhs.data(), lhsWords, rhs.data(), rhsWords, amount, width, expected.data(), words);
            if (sample % 4 == 0) before = expected;
            if (sample % 4 == 1) { before = expected; before.front() ^= 1; }
            if (sample % 4 == 2) { before = expected; before.back() ^= UINT64_C(1) << 63; }
            auto actual = before;
            const bool didChange = changed<Operation>(lhs.data(), lhsWords, rhs.data(), rhsWords, amount, width, actual.data(), words);
            if (actual != expected || didChange != (before != expected)) throw std::runtime_error("wide changed/output mismatch");
            if (changed<Operation>(lhs.data(), lhsWords, rhs.data(), rhsWords, amount, width, actual.data(), words))
                throw std::runtime_error("unchanged wide output reported changed");
            auto alias = lhs;
            const bool aliasChanged = changed<Operation>(alias.data(), lhsWords, rhs.data(), rhsWords, amount, width, alias.data(), words);
            if (alias != expected || aliasChanged != (lhs != expected)) throw std::runtime_error("lhs alias mismatch");
            if constexpr (Operation == '+' || Operation == '-')
            {
                alias = rhs;
                const bool rhsChanged = changed<Operation>(lhs.data(), lhsWords, alias.data(), rhsWords, amount, width, alias.data(), words);
                if (alias != expected || rhsChanged != (rhs != expected)) throw std::runtime_error("rhs alias mismatch");
            }
        }
    }
}

int main()
{
    std::mt19937_64 random(659);
    helperTests<'+'>(random); helperTests<'-'>(random);
    helperTests<'L'>(random); helperTests<'R'>(random); helperTests<'A'>(random);
    GrhSIM_cpu_wide_activity model;
    for (unsigned sample = 0; sample < 4096; ++sample)
    {
        if (sample % 1024 == 0) model.init();
        model.lhs = {random(), random(), random() & 1};
        model.rhs = {random(), random(), random() & 1};
        if (sample % 2 == 0) model.rhs = {sample % 193, 0, 0};
        model.narrow = random() & 0xfffffff;
        const std::uint64_t narrow = model.narrow;
        const auto amount = grhsim_index_words(model.rhs, 129);
        std::array<std::uint64_t, 3> expected{};
        for (unsigned repeat = 0; repeat < 2; ++repeat)
        {
            model.eval();
            const auto check = [&](const auto &actual) {
                if (actual != expected) throw std::runtime_error("generated wide activity model mismatch");
            };
            legacy<'+'>(model.lhs.data(), 3, model.rhs.data(), 3, amount, 129, expected.data(), 3); check(model.out_add);
            legacy<'-'>(model.lhs.data(), 3, model.rhs.data(), 3, amount, 129, expected.data(), 3); check(model.out_sub);
            legacy<'L'>(model.lhs.data(), 3, nullptr, 0, amount, 129, expected.data(), 3); check(model.out_shl);
            legacy<'R'>(model.lhs.data(), 3, nullptr, 0, amount, 129, expected.data(), 3); check(model.out_lshr);
            legacy<'A'>(model.lhs.data(), 3, nullptr, 0, amount, 129, expected.data(), 3); check(model.out_ashr);
            legacy<'+'>(model.lhs.data(), 3, &narrow, 1, 0, 129, expected.data(), 3); check(model.out_mixed_add);
            legacy<'+'>(&narrow, 1, &narrow, 1, 0, 129, expected.data(), 3); check(model.out_same_add);
            legacy<'L'>(&narrow, 1, nullptr, 0, amount, 129, expected.data(), 3); check(model.out_mixed_shl);
        }
    }
    std::cout << "wide activity PASS helper_cases=4480 model_evals=8192 resets=4\n";
}
