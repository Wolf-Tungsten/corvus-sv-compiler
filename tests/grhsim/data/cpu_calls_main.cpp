#include "grhsim_cpu_calls.hpp"

#include <iostream>
#include <sstream>
#include <stdexcept>

namespace
{
    unsigned mixedCalls = 0, plainCalls = 0, voidCalls = 0, edgeCalls = 0, multiCalls = 0;
    unsigned literalCalls = 0, stringIoCalls = 0;
    const std::string literalText = std::string(256, 'L') + "\"\\\n\r\t";
    void require(bool condition, const char *message) { if (!condition) throw std::runtime_error(message); }
    struct Capture
    {
        std::ostringstream out, err;
        std::streambuf *oldOut = std::cout.rdbuf(out.rdbuf()), *oldErr = std::cerr.rdbuf(err.rdbuf());
        ~Capture() { std::cout.rdbuf(oldOut); std::cerr.rdbuf(oldErr); }
    };
}

extern "C" std::uint8_t cpu_test_mixed(std::uint8_t *out, std::uint8_t *io, std::uint8_t data,
    std::array<std::uint64_t, 3> *wideOut, const std::array<std::uint64_t, 3> &wide, const char *text,
    std::string *textOut, double real, double *realOut, std::array<std::uint64_t, 3> *wideIo)
{
    ++mixedCalls;
    require(*io == data && *wideIo == wide, "DPI inout did not receive input values");
    *out = data + 2; *io += 3;
    *wideOut = wide; (*wideOut)[0] ^= 0x1234; (*wideOut)[2] = UINT64_MAX;
    (*wideIo)[1] ^= 0x5678; (*wideIo)[2] = UINT64_MAX;
    *textOut = std::string(text) + ":called"; *realOut = real + 0.5;
    return data + 1;
}
extern "C" std::uint8_t cpu_test_plain(std::uint8_t data) { ++plainCalls; return data ^ 0x5a; }
extern "C" void cpu_test_void(std::uint8_t) { ++voidCalls; }
extern "C" void cpu_test_edge_void(std::uint8_t) { ++edgeCalls; }
extern "C" void cpu_test_multi_void() { ++multiCalls; }
extern "C" void cpu_test_literal(const char *text)
{
    ++literalCalls;
    require(text == literalText, "DPI long/escaped literal mismatch");
}
extern "C" void cpu_test_string_io(std::string *text)
{
    ++stringIoCalls;
    require(*text == literalText, "DPI string inout did not receive immutable input");
    *text += ":modified";
}
// The native provider spelling must not conflict with generated declarations in its TU.
extern "C" std::uint64_t cpu_test_host_abi(std::uint64_t word) { return word ^ UINT64_C(0x800000000000005a); }
extern "C" bool cpu_test_narrow(std::int8_t *io, bool *out)
{
    require(*io >= -16 && *io <= 15, "DPI signed inout input was not sign extended");
    ++*io; *out = true; return true;
}

int main(int argc, char **argv)
{
    try
    {
        GrhSIM_cpu_calls model; model.init(); model.handle = 0x80000002;
        if (argc > 1)
        {
            model.finish = std::string_view(argv[1]) == "--finish";
            model.fatal = std::string_view(argv[1]) == "--fatal";
            model.eval(); return 99;
        }
        unsigned expectedMixed = 0, expectedEdge = 0, expectedMulti = 0;
        for (unsigned reset = 0; reset < 3; ++reset)
        {
            model.init(); model.clock_a = false; model.clock_b = false; model.enable = false;
            bool previousA = false, previousB = false, once = false;
            std::uint8_t returned = 0, out = 0, io = 0, plain = 0;
            std::int8_t signedBit = 0, narrowIo = 0;
            std::uint64_t hostResult = 0;
            std::array<std::uint64_t, 3> wideOut{}, wideIo{};
            std::string textOut, literalInout; double realOut = 0;
            Capture captured;
            std::string expectedOut, expectedErr;
            for (unsigned sample = 0; sample < 128; ++sample)
            {
                model.clock_a = (sample / 2) % 2;
                model.clock_b = (sample / 3) % 2;
                model.enable = sample % 5 != 1 ? 0x100 : 0;
                model.wide_enable = {0, 0, model.enable ? 1u : 0u};
                model.data = sample * 17;
                model.narrow = sample % 32;
                model.wide = {sample, sample * 91u, sample % 2};
                model.text = std::string(100 + sample, 'a' + sample % 26);
                model.real_in = sample * 0.25;
                model.host_word = -static_cast<std::int64_t>(sample) - 1;
                const bool posA = !previousA && model.clock_a, posB = !previousB && model.clock_b,
                    negB = previousB && !model.clock_b;
                if (model.enable)
                {
                    plain = model.data ^ 0x5a;
                    hostResult = static_cast<std::uint64_t>(model.host_word) ^ UINT64_C(0x800000000000005a);
                    signedBit = -1;
                    const auto nextNarrow = (model.narrow + 1) % 32;
                    narrowIo = nextNarrow >= 16 ? nextNarrow - 32 : nextNarrow;
                    if (negB) ++expectedEdge;
                    if (posA || negB) ++expectedMulti;
                    if (posA)
                    {
                        ++expectedMixed; returned = model.data + 1; out = model.data + 2; io = model.data + 3;
                        wideOut = model.wide; wideOut[0] ^= 0x1234; wideOut[2] = 1;
                        wideIo = model.wide; wideIo[1] ^= 0x5678; wideIo[2] = 1;
                        textOut = model.text + ":called"; realOut = model.real_in + 0.5;
                        literalInout = literalText + ":modified";
                        expectedErr += "value=" + std::to_string(model.data) + "\n";
                    }
                    if (posB && !once) { expectedOut += "once\n"; once = true; }
                    if (posA) expectedOut += "strobe=" + std::to_string(model.data) + "\n";
                }
                const auto beforePlain = plainCalls, beforeVoid = voidCalls;
                model.eval();
                require(mixedCalls == expectedMixed && edgeCalls == expectedEdge && multiCalls == expectedMulti, "event DPI count mismatch");
                require(literalCalls == expectedMixed && stringIoCalls == expectedMixed, "constant-string DPI count mismatch");
                require(model.literal_out == literalText && model.literal_inout == literalInout, "shared constant/inout hold mismatch");
                require((plainCalls > beforePlain) == bool(model.enable) && (voidCalls > beforeVoid) == bool(model.enable), "eventless DPI was dropped or bypassed condition");
                require(plainCalls - beforePlain == voidCalls - beforeVoid, "eventless return/void scheduling differs");
                require(model.returned == returned && model.out == out && model.inout_out == io && model.plain == plain, "DPI scalar results/hold mismatch");
                require(model.signed_bit == signedBit && model.signed_bit_out == signedBit && model.narrow_io == narrowIo, "DPI signed narrow normalization mismatch");
                require(static_cast<std::uint64_t>(model.host_result) == hostResult, "native provider ABI bits mismatch");
                require(model.wide_out == wideOut && model.wide_inout == wideIo, "DPI wide results/padding mismatch");
                require(model.text_out == textOut && model.real_out == realOut, "DPI string/real results mismatch");
                require(model.captured == returned, "DPI result was not captured by clocked state");
                require(captured.out.str() == expectedOut && captured.err.str() == expectedErr, "system-task timing/output mismatch");
                previousA = model.clock_a; previousB = model.clock_b;
            }
        }
        std::cout << "External-call regression passed: 384 samples, return/void, events, output/inout, wide/string/real, system-task timing\n";
    }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
