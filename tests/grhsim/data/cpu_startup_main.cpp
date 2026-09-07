#include "grhsim_cpu_startup.hpp"

#include <iostream>
#include <stdexcept>

int main()
{
    try
    {
        for (unsigned instance = 0; instance < 4; ++instance)
        {
            GrhSIM_cpu_startup model;
            for (unsigned reset = 0; reset < 4; ++reset)
            {
                model.init();
                model.clock = false;
                model.enable = false;
                for (unsigned sample = 0; sample < 48; ++sample)
                {
                    model.text = sample % 3 == 0 ? "" : sample % 3 == 1 ? "short" : std::string(4096 + sample, 'a' + sample % 26);
                    model.cpu_overheated = sample % 2 == 0;
                    model.address = sample % 2 == 0 ? 0 : 16 * 1024 * 1024 - 1;
                    model.eval();
                    if (model.cpu_again != model.cpu_overheated || model.cpu_round != model.cpu_overheated)
                        throw std::runtime_error("cpu-prefixed port was shadowed by eval locals");
                    if (sample < 2 && model.q != 0x3c) throw std::runtime_error("large array did not reset to nonzero fill");
                    if (model.text_a != model.text || model.text_b != model.text || model.text_c != model.text)
                        throw std::runtime_error("string input/boundary/output mismatch");
                    if (model.constant_a != std::string(200, 'x') + "constant_a" ||
                        model.constant_b != std::string(200, 'x') + "constant_b" ||
                        model.constant_c != std::string(200, 'x') + "constant_c")
                        throw std::runtime_error("local string helper lifetime mismatch");
                    model.eval();
                }
                model.enable = true;
                for (auto address : {0u, 16 * 1024 * 1024 - 1u})
                {
                    model.clock = false; model.address = address; model.data = 0xa5; model.eval();
                    model.clock = true; model.eval();
                    if (model.q != 0xa5) throw std::runtime_error("large array write did not publish");
                }
            }
        }
        std::cout << "O0 startup passed: 8 MiB stack, 16 MiB memory, 16 resets, 768 string samples\n";
    }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
